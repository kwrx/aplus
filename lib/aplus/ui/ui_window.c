/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

#include <aplus/ui.h>

#include "ui_internal.h"


ui_window_t* ui_window_from_id(ui_connection_t* conn, uint32_t id) {

    for (ui_window_t* w = conn->windows; w; w = w->next) {

        if (w->id == id) {
            return w;
        }
    }

    return NULL;
}


/**
 * @brief Lets go of the window's surface, which the server has already removed.
 *
 * @param win The window to detach.
 */
void ui_window_drop_surface(ui_window_t* win) {

    if (!win->pixels) {
        return;
    }

    shmdt(win->pixels);

    win->pixels = NULL;
    win->shm_id = -1;
}


/**
 * @brief Takes up the surface a UI_EV_CONFIGURE described, along with its size and serial.
 *
 * The new surface is attached before the old one is let go, and the whole of it is damaged.
 *
 * @param win The window to attach it to.
 * @param width The new width in pixels.
 * @param height The new height in pixels.
 * @param stride The new row stride in bytes.
 * @param shm_id The shared memory segment holding the surface.
 * @param shm_size The size of that segment.
 * @param serial The serial of the configure being answered.
 * @return 0 on success, or -1 with errno set.
 */
int ui_window_adopt_surface(ui_window_t* win, int width, int height, size_t stride, int shm_id, size_t shm_size, uint32_t serial) {

    if (width <= 0 || height <= 0 || shm_id < 0) {
        errno = EINVAL;
        return -1;
    }

    if (stride < (size_t)width * sizeof(uint32_t) || shm_size < stride * (size_t)height) {
        errno = EPROTO;
        return -1;
    }


    if (shm_id != win->shm_id || !win->pixels) {

        uint32_t* pixels = (uint32_t*)shmat(shm_id, NULL, 0);

        if (pixels == (uint32_t*)-1) {
            return -1;
        }

        ui_window_drop_surface(win);

        win->pixels = pixels;
        win->shm_id = shm_id;
    }

    win->width  = width;
    win->height = height;
    win->stride = stride;

    win->asked_width  = width;
    win->asked_height = height;

    win->serial = serial;

    ui_window_damage_all(win);

    return 0;
}


/**
 * @brief Applies a pending configure, clearing it only once the new surface is in place.
 *
 * @param win The window to configure.
 * @return 1 when a configure was applied, 0 when none was pending, or -1 with errno set.
 */
int ui_window_apply_configure(ui_window_t* win) {

    if (!win) {
        errno = EINVAL;
        return -1;
    }

    if (!win->pending.valid) {
        return 0;
    }

    if (ui_window_adopt_surface(win, win->pending.width, win->pending.height, win->pending.stride, win->pending.shm_id, win->pending.shm_size, win->pending.serial) < 0) {
        return -1;
    }

    win->pending.valid = false;

    return 1;
}


ui_window_t* ui_window_create(ui_connection_t* conn, int width, int height, const char* title) {
    return ui_window_create_ex(conn, width, height, title, UI_WINDOW_DECORATED);
}


/**
 * @brief Creates a window of a given kind, blocking until the server has configured it.
 *
 * A UI_WINDOW_BORDERLESS window is handed the whole of its frame: nothing is drawn around
 * it, so there is no titlebar to drag it by and no edge to resize it from, though it can
 * still ask for a size with ui_window_request_size(). A UI_WINDOW_CENTERED one comes up in
 * the middle of the display instead of on the cascade. A UI_WINDOW_TRANSLUCENT one is
 * handed a surface that carries alpha, which the server blends over what is behind it.
 *
 * @param conn The connection to create it on.
 * @param width The width of the content area in pixels, which the server may clamp.
 * @param height The height of the content area in pixels, which the server may clamp.
 * @param title The window title, truncated at UI_TITLE_MAX, or NULL.
 * @param flags UI_WINDOW_*.
 * @return The window, or NULL with errno set.
 */
ui_window_t* ui_window_create_ex(ui_connection_t* conn, int width, int height, const char* title, uint32_t flags) {

    if (!conn || width <= 0 || height <= 0) {
        errno = EINVAL;
        return NULL;
    }

    if (flags & ~(uint32_t)UI_WINDOW_FLAGS_ALL) {
        errno = EINVAL;
        return NULL;
    }


    ui_window_t* win = (ui_window_t*)calloc(1, sizeof(ui_window_t));

    if (!win) {
        return NULL;
    }

    win->conn   = conn;
    win->flags  = flags;
    win->shm_id = -1;


    ui_msg_create_window_t req;

    memset(&req, 0, sizeof(req));

    req.width  = (uint16_t)width;
    req.height = (uint16_t)height;
    req.flags  = flags;

    if (title) {
        strncpy(req.title, title, UI_TITLE_MAX - 1);
    }

    if (ui_send_msg(conn->fd, UI_REQ_CREATE_WINDOW, &req, sizeof(req)) < 0) {
        free(win);
        return NULL;
    }


    ui_msg_configure_t cfg;

    for (;;) {

        ui_msg_header_t hdr;

        if (ui_conn_read(conn, &hdr, sizeof(hdr)) < 0) {
            free(win);
            return NULL;
        }

        if (hdr.length > UI_MSG_PAYLOAD_MAX) {
            free(win);
            errno = EPROTO;
            return NULL;
        }

        if (hdr.type == UI_EV_CONFIGURE && hdr.length == sizeof(cfg)) {

            if (ui_conn_read(conn, &cfg, sizeof(cfg)) < 0) {
                free(win);
                return NULL;
            }

            break;
        }

        uint8_t scratch[256];

        for (size_t left = hdr.length; left;) {

            size_t chunk = left > sizeof(scratch) ? sizeof(scratch) : left;

            if (ui_conn_read(conn, scratch, chunk) < 0) {
                free(win);
                return NULL;
            }

            left -= chunk;
        }
    }


    win->id = cfg.window_id;

    if (ui_window_adopt_surface(win, cfg.width, cfg.height, cfg.stride, cfg.shm_id, cfg.shm_size, cfg.serial) < 0) {
        free(win);
        return NULL;
    }


    win->next     = conn->windows;
    conn->windows = win;

    return win;
}


void ui_window_destroy(ui_window_t* win) {

    if (!win) {
        return;
    }


    ui_connection_t* conn = win->conn;

    ui_msg_window_t req = {.window_id = win->id};

    ui_send_msg(conn->fd, UI_REQ_DESTROY_WINDOW, &req, sizeof(req));


    ui_window_t** it = &conn->windows;

    while (*it) {

        if (*it == win) {
            *it = win->next;
            break;
        }

        it = &(*it)->next;
    }

    ui_window_drop_surface(win);

    free(win);
}


uint32_t* ui_window_pixels(ui_window_t* win) {
    return win ? win->pixels : NULL;
}

int ui_window_width(ui_window_t* win) {
    return win ? win->width : 0;
}

int ui_window_height(ui_window_t* win) {
    return win ? win->height : 0;
}

size_t ui_window_stride(ui_window_t* win) {
    return win ? win->stride : 0;
}

uint32_t ui_window_id(ui_window_t* win) {
    return win ? win->id : 0;
}

uint32_t ui_window_flags(ui_window_t* win) {
    return win ? win->flags : 0;
}

bool ui_window_translucent(ui_window_t* win) {
    return win ? (win->flags & UI_WINDOW_TRANSLUCENT) != 0 : false;
}


int ui_window_request_size(ui_window_t* win, int width, int height) {

    if (!win || width <= 0 || height <= 0 || width > UINT16_MAX || height > UINT16_MAX) {
        errno = EINVAL;
        return -1;
    }

    if (width == win->asked_width && height == win->asked_height) {
        return 0;
    }


    ui_msg_resize_t req;

    memset(&req, 0, sizeof(req));

    req.window_id = win->id;
    req.width     = (uint16_t)width;
    req.height    = (uint16_t)height;

    if (ui_send_msg(win->conn->fd, UI_REQ_RESIZE_WINDOW, &req, sizeof(req)) < 0) {
        return -1;
    }

    win->asked_width  = width;
    win->asked_height = height;

    return 0;
}


void ui_window_damage(ui_window_t* win, int x, int y, int width, int height) {

    if (!win || width <= 0 || height <= 0) {
        return;
    }


    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + width;
    int y1 = y + height;

    if (x1 > win->width) {
        x1 = win->width;
    }

    if (y1 > win->height) {
        y1 = win->height;
    }

    if (x0 >= x1 || y0 >= y1) {
        return;
    }


    ui_rect_t rect = {x0, y0, x1 - x0, y1 - y0};

    ui_damage_add(&win->damage, rect);
}


void ui_window_damage_all(ui_window_t* win) {

    if (!win) {
        return;
    }


    ui_rect_t all = {0, 0, win->width, win->height};

    ui_damage_reset(&win->damage);
    ui_damage_add(&win->damage, all);
}


/**
 * @brief Tells the server which parts of the surface changed, one message per rectangle.
 *
 * @param win The window to commit.
 * @return 0 on success, or -1 with errno set.
 */
int ui_window_commit(ui_window_t* win) {

    if (!win) {
        errno = EINVAL;
        return -1;
    }

    if (!win->damage.count) {
        return 0;
    }


    const size_t count = win->damage.count;

    ui_msg_commit_t commits[UI_DAMAGE_MAX];

    for (size_t i = 0; i < count; i++) {

        commits[i].window_id = win->id;
        commits[i].serial    = win->serial;
        commits[i].x         = (uint16_t)win->damage.rects[i].x;
        commits[i].y         = (uint16_t)win->damage.rects[i].y;
        commits[i].width     = (uint16_t)win->damage.rects[i].width;
        commits[i].height    = (uint16_t)win->damage.rects[i].height;
    }

    ui_damage_reset(&win->damage);


    for (size_t i = 0; i < count; i++) {

        if (ui_send_msg(win->conn->fd, UI_REQ_COMMIT, &commits[i], sizeof(commits[i])) < 0) {
            return -1;
        }
    }

    return 0;
}


int ui_window_set_title(ui_window_t* win, const char* title) {

    if (!win) {
        errno = EINVAL;
        return -1;
    }


    ui_msg_set_title_t req;

    memset(&req, 0, sizeof(req));

    req.window_id = win->id;

    if (title) {
        strncpy(req.title, title, UI_TITLE_MAX - 1);
    }

    return ui_send_msg(win->conn->fd, UI_REQ_SET_TITLE, &req, sizeof(req));
}
