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


int ui_window_reconfigure(ui_window_t* win, int width, int height, uint32_t serial) {

    if (width <= 0 || height <= 0) {
        errno = EINVAL;
        return -1;
    }


    const size_t needed = (size_t)width * (size_t)height;

    if (needed > win->capacity) {

        uint32_t* pixels = (uint32_t*)calloc(needed, sizeof(uint32_t));

        if (!pixels) {
            return -1;
        }

        free(win->pixels);

        win->pixels   = pixels;
        win->capacity = needed;

    } else if (width != win->width || height != win->height) {

        /* Reusing the buffer means the old contents are still in it, laid out at the old
           stride. Clearing avoids a frame of garbage in whatever the client does not
           redraw straight away. */
        memset(win->pixels, 0, needed * sizeof(uint32_t));
    }

    win->width  = width;
    win->height = height;

    win->serial = serial;

    /* The old contents are gone, so the next commit has to carry the whole surface
       however little the client thinks it changed. */
    ui_window_damage_all(win);

    return 0;
}


int ui_window_apply_configure(ui_window_t* win) {

    if (!win) {
        errno = EINVAL;
        return -1;
    }

    if (!win->pending.valid) {
        return 0;
    }

    /* Cleared only once the new size is actually in place. Dropping it up front would
       leave the window drawing at the old size against a serial the server has already
       moved past, so every commit from then on is discarded and the window never paints
       again; keeping it pending means the caller can simply try the resize once more. */
    if (ui_window_reconfigure(win, win->pending.width, win->pending.height, win->pending.serial) < 0) {
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

    win->conn = conn;


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

    if (ui_window_reconfigure(win, cfg.width, cfg.height, cfg.serial) < 0) {
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

    free(win->pixels);
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
    return win ? (size_t)win->width * sizeof(uint32_t) : 0;
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


static int ui_window_send_band(ui_window_t* win, int x, int y, int width, int height) {

    ui_msg_commit_t commit = {

        .window_id = win->id,
        .serial    = win->serial,
        .x         = (uint16_t)x,
        .y         = (uint16_t)y,
        .width     = (uint16_t)width,
        .height    = (uint16_t)height,
    };

    const size_t row     = (size_t)width * sizeof(uint32_t);
    const size_t payload = sizeof(commit) + row * (size_t)height;


    ui_msg_header_t hdr = {

        .type   = UI_REQ_COMMIT,
        .flags  = 0,
        .length = (uint32_t)payload,
    };

    if (ui_send_all(win->conn->fd, &hdr, sizeof(hdr)) < 0) {
        return -1;
    }

    if (ui_send_all(win->conn->fd, &commit, sizeof(commit)) < 0) {
        return -1;
    }

    /* One write per row rather than one for the band: the damage rect is a sub-rectangle
       of a wider surface, so the rows it covers are not contiguous in memory. */
    for (int i = 0; i < height; i++) {

        const uint32_t* src = win->pixels + (size_t)(y + i) * (size_t)win->width + (size_t)x;

        if (ui_send_all(win->conn->fd, src, row) < 0) {
            return -1;
        }
    }

    return 0;
}


int ui_window_commit(ui_window_t* win) {

    if (!win) {
        errno = EINVAL;
        return -1;
    }

    if (!win->damage.valid) {
        return 0;
    }


    const int x0 = win->damage.x0;
    const int y0 = win->damage.y0;
    const int x1 = win->damage.x1;
    const int y1 = win->damage.y1;

    win->damage.valid = false;


    /* The socket buffer is 65535 bytes and a write only ever makes partial progress, so a
       whole-surface commit would never fit in one frame. Split the damage into tiles that
       each stay well inside the buffer; the server stitches them back together because
       every tile names its own position. */
    const int max_pixels = UI_COMMIT_BAND_MAX / (int)sizeof(uint32_t);

    int chunk_w = x1 - x0;

    if (chunk_w > max_pixels) {
        chunk_w = max_pixels;
    }


    for (int x = x0; x < x1; x += chunk_w) {

        int bw = x1 - x;

        if (bw > chunk_w) {
            bw = chunk_w;
        }

        int rows = max_pixels / bw;

        if (rows < 1) {
            rows = 1;
        }

        for (int y = y0; y < y1; y += rows) {

            int bh = y1 - y;

            if (bh > rows) {
                bh = rows;
            }

            if (ui_window_send_band(win, x, y, bw, bh) < 0) {
                return -1;
            }
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
