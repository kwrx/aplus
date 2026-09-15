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


/* Let go of the window's surface.
 *
 * The segment belongs to the server, which has already removed it; detaching is this end's
 * whole share of the bookkeeping, and is what finally lets the kernel hand the frames back once
 * the server has let go too.
 */
void ui_window_drop_surface(ui_window_t* win) {

    if (!win->pixels) {
        return;
    }

    shmdt(win->pixels);

    win->pixels = NULL;
    win->shm_id = -1;
}


/* Take up the surface a UI_EV_CONFIGURE described, along with its size and serial.
 *
 * A segment too small for the surface it describes is refused, so that a bad or truncated
 * configure cannot turn into a write past the end of it. The new one is attached before the old
 * one is let go, so that a failure leaves the window drawing into a surface that still exists
 * rather than into nothing.
 *
 * Nothing is remapped when the id has not changed. The server sends a configure at the end of
 * every drag, not only the ones that changed the size, and only a size change makes it build a
 * new segment; re-attaching the one already held would work, but costs a range of address space
 * that shmdt(2) does not give back. An id is never reused while the segment it names is alive,
 * so the same id is the same memory.
 *
 * The whole surface is damaged on the way out. The server carried the old contents over so that
 * a window does not go black mid-drag, but only the client knows what belongs there at the new
 * size.
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

    win->serial = serial;

    ui_window_damage_all(win);

    return 0;
}


/* @see <aplus/ui.h>.
 *
 * The pending configure is cleared only once the new surface is actually in place. Dropping it
 * up front would leave the window drawing at the old size against a serial the server has
 * already moved past, so every commit from then on is discarded and the window never paints
 * again; keeping it pending means the caller can simply try again.
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

    if (!conn || width <= 0 || height <= 0) {
        errno = EINVAL;
        return NULL;
    }


    ui_window_t* win = (ui_window_t*)calloc(1, sizeof(ui_window_t));

    if (!win) {
        return NULL;
    }

    win->conn   = conn;
    win->shm_id = -1;


    ui_msg_create_window_t req;

    memset(&req, 0, sizeof(req));

    req.width  = (uint16_t)width;
    req.height = (uint16_t)height;

    if (title) {
        strncpy(req.title, title, UI_TITLE_MAX - 1);
    }

    if (ui_send_msg(conn->fd, UI_REQ_CREATE_WINDOW, &req, sizeof(req)) < 0) {
        free(win);
        return NULL;
    }


    /* Wait for the server to name and size the window. The server queues the configure
       first, but step over anything else that turns up rather than treating it as a
       protocol error -- a client with no window yet has nothing to do with a stray event
       anyway, and this keeps the handshake from depending on the server's queue order. */
    ui_msg_configure_t cfg;

    for (;;) {

        ui_msg_header_t hdr;

        if (ui_recv_all(conn->fd, &hdr, sizeof(hdr)) < 0) {
            free(win);
            return NULL;
        }

        if (hdr.length > UI_MSG_PAYLOAD_MAX) {
            free(win);
            errno = EPROTO;
            return NULL;
        }

        if (hdr.type == UI_EV_CONFIGURE && hdr.length == sizeof(cfg)) {

            if (ui_recv_all(conn->fd, &cfg, sizeof(cfg)) < 0) {
                free(win);
                return NULL;
            }

            break;
        }

        uint8_t scratch[256];

        for (size_t left = hdr.length; left;) {

            size_t chunk = left > sizeof(scratch) ? sizeof(scratch) : left;

            if (ui_recv_all(conn->fd, scratch, chunk) < 0) {
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


    if (!win->damage.valid) {

        win->damage.valid = true;
        win->damage.x0    = x0;
        win->damage.y0    = y0;
        win->damage.x1    = x1;
        win->damage.y1    = y1;

        return;
    }

    if (x0 < win->damage.x0) {
        win->damage.x0 = x0;
    }

    if (y0 < win->damage.y0) {
        win->damage.y0 = y0;
    }

    if (x1 > win->damage.x1) {
        win->damage.x1 = x1;
    }

    if (y1 > win->damage.y1) {
        win->damage.y1 = y1;
    }
}


void ui_window_damage_all(ui_window_t* win) {

    if (!win) {
        return;
    }

    win->damage.valid = true;
    win->damage.x0    = 0;
    win->damage.y0    = 0;
    win->damage.x1    = win->width;
    win->damage.y1    = win->height;
}


/* Tell the server which part of the surface changed.
 *
 * One small message for however much changed: the pixels are already where the server reads
 * them from, so a full-screen repaint costs the same as a single character cell.
 */
int ui_window_commit(ui_window_t* win) {

    if (!win) {
        errno = EINVAL;
        return -1;
    }

    if (!win->damage.valid) {
        return 0;
    }


    ui_msg_commit_t commit = {

        .window_id = win->id,
        .serial    = win->serial,
        .x         = (uint16_t)win->damage.x0,
        .y         = (uint16_t)win->damage.y0,
        .width     = (uint16_t)(win->damage.x1 - win->damage.x0),
        .height    = (uint16_t)(win->damage.y1 - win->damage.y0),
    };

    win->damage.valid = false;

    return ui_send_msg(win->conn->fd, UI_REQ_COMMIT, &commit, sizeof(commit));
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
