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
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <aplus/ui.h>

#include "ui_internal.h"


static int ui_drain_payload(ui_connection_t* conn, size_t size) {

    uint8_t scratch[256];

    while (size) {

        size_t chunk = size > sizeof(scratch) ? sizeof(scratch) : size;

        if (ui_conn_read(conn, scratch, chunk) < 0) {
            return -1;
        }

        size -= chunk;
    }

    return 0;
}


/**
 * @brief Waits for the next event on a connection, recording a configure rather than acting on it.
 *
 * @param conn The connection to read from.
 * @param out Receives the event.
 * @param timeout_ms How long to wait, or a negative value to wait indefinitely.
 * @return 1 when an event was read, 0 on timeout, or -1 with errno set.
 */
int ui_next_event(ui_connection_t* conn, ui_event_t* out, int timeout_ms) {

    if (!conn || !out) {
        errno = EINVAL;
        return -1;
    }


    for (;;) {

        const int ready = ui_conn_message_ready(conn);

        if (ready < 0) {
            return -1;
        }

        if (!ready) {

            if (timeout_ms >= 0) {

                struct pollfd pfd = {

                    .fd      = conn->fd,
                    .events  = POLLIN,
                    .revents = 0,
                };

                int e = poll(&pfd, 1, timeout_ms);

                if (e < 0) {

                    if (errno == EINTR) {
                        continue;
                    }

                    return -1;
                }

                if (e == 0) {
                    return 0;
                }
            }

            if (ui_conn_fill(conn) < 0) {
                return -1;
            }

            continue;
        }


        ui_msg_header_t hdr;

        if (ui_conn_read(conn, &hdr, sizeof(hdr)) < 0) {
            return -1;
        }


        memset(out, 0, sizeof(*out));

        switch (hdr.type) {

            case UI_EV_CONFIGURE: {

                ui_msg_configure_t cfg;

                if (hdr.length != sizeof(cfg)) {
                    errno = EPROTO;
                    return -1;
                }

                if (ui_conn_read(conn, &cfg, sizeof(cfg)) < 0) {
                    return -1;
                }

                ui_window_t* win = ui_window_from_id(conn, cfg.window_id);

                if (win) {

                    win->pending.valid    = true;
                    win->pending.width    = cfg.width;
                    win->pending.height   = cfg.height;
                    win->pending.stride   = cfg.stride;
                    win->pending.shm_id   = cfg.shm_id;
                    win->pending.shm_size = cfg.shm_size;
                    win->pending.serial   = cfg.serial;
                }

                out->type                = UI_EVENT_CONFIGURE;
                out->window_id           = cfg.window_id;
                out->configure.width     = cfg.width;
                out->configure.height    = cfg.height;
                out->configure.serial    = cfg.serial;

                return 1;
            }

            case UI_EV_KEY: {

                ui_msg_key_t key;

                if (hdr.length != sizeof(key)) {
                    errno = EPROTO;
                    return -1;
                }

                if (ui_conn_read(conn, &key, sizeof(key)) < 0) {
                    return -1;
                }

                out->type      = UI_EVENT_KEY;
                out->window_id = key.window_id;
                out->key.vkey  = key.vkey;
                out->key.down  = key.down;

                return 1;
            }

            case UI_EV_POINTER: {

                ui_msg_pointer_t ptr;

                if (hdr.length != sizeof(ptr)) {
                    errno = EPROTO;
                    return -1;
                }

                if (ui_conn_read(conn, &ptr, sizeof(ptr)) < 0) {
                    return -1;
                }

                out->type            = UI_EVENT_POINTER;
                out->window_id       = ptr.window_id;
                out->pointer.x       = ptr.x;
                out->pointer.y       = ptr.y;
                out->pointer.buttons = ptr.buttons;

                return 1;
            }

            case UI_EV_FOCUS: {

                ui_msg_focus_t focus;

                if (hdr.length != sizeof(focus)) {
                    errno = EPROTO;
                    return -1;
                }

                if (ui_conn_read(conn, &focus, sizeof(focus)) < 0) {
                    return -1;
                }

                out->type          = UI_EVENT_FOCUS;
                out->window_id     = focus.window_id;
                out->focus.focused = focus.focused;

                return 1;
            }

            case UI_EV_LEAVE: {

                ui_msg_window_t win;

                if (hdr.length != sizeof(win)) {
                    errno = EPROTO;
                    return -1;
                }

                if (ui_conn_read(conn, &win, sizeof(win)) < 0) {
                    return -1;
                }

                out->type      = UI_EVENT_LEAVE;
                out->window_id = win.window_id;

                return 1;
            }

            case UI_EV_CLOSE: {

                ui_msg_window_t win;

                if (hdr.length != sizeof(win)) {
                    errno = EPROTO;
                    return -1;
                }

                if (ui_conn_read(conn, &win, sizeof(win)) < 0) {
                    return -1;
                }

                out->type      = UI_EVENT_CLOSE;
                out->window_id = win.window_id;

                return 1;
            }

            default:

                if (ui_drain_payload(conn, hdr.length) < 0) {
                    return -1;
                }

                break;
        }
    }
}
