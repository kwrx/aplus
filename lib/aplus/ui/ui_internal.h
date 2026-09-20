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

#ifndef _APLUS_UI_INTERNAL_H
#define _APLUS_UI_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include <aplus/ui-widgets.h>
#include <aplus/ui.h>


struct ui_window {

    ui_connection_t* conn;

    uint32_t id;
    uint32_t serial;

    //? UI_WINDOW_*, as the window was created with. The server will not change them, so
    //? this copy stays the answer for the rest of the window's life.
    uint32_t flags;

    int width;
    int height;

    //? The size last asked for with ui_window_request_size(), or last granted when that
    //? was the server's idea. Compared against rather than the size in force, so that a
    //? second request made while the first is still in flight is not mistaken for a
    //? request to stay as things are.
    int asked_width;
    int asked_height;

    //? Bytes per row, as the server chose it. Never assumed to be width * 4: the two ends
    //? are reading and writing the same memory and have to agree exactly.
    size_t stride;

    //? The window's surface, mapped from the server's shared memory segment. Owned by the
    //? server; this end only attaches and detaches.
    uint32_t* pixels;

    int shm_id;

    ui_damage_t damage;

    //? A configure that has arrived but has not been acted on yet. The surface is only
    //? reallocated when the caller asks, because ui_next_event() usually runs on a
    //? different thread from the one drawing into the pixels.
    struct {

        bool valid;

        int width;
        int height;

        size_t stride;

        int shm_id;
        size_t shm_size;

        uint32_t serial;

    } pending;

    struct ui_window* next;
};


struct ui_connection {

    int fd;
    ui_window_t* windows;

    //? Bytes read off the socket but not yet handed out. Every read on the connection goes
    //? through here, so one read(2) serves a whole burst of events instead of two syscalls
    //? per event. Anything checking for a pending event has to look here before it polls:
    //? a whole event sitting in this buffer raises nothing on the descriptor.
    struct {

        uint8_t* data;

        size_t head;
        size_t size;
        size_t capacity;

    } rx;
};


int ui_conn_read(ui_connection_t* conn, void* buf, size_t size);
int ui_conn_message_ready(ui_connection_t* conn);
int ui_conn_fill(ui_connection_t* conn);


ui_window_t* ui_window_from_id(ui_connection_t* conn, uint32_t id);

int ui_window_adopt_surface(ui_window_t* win, int width, int height, size_t stride, int shm_id, size_t shm_size, uint32_t serial);
void ui_window_drop_surface(ui_window_t* win);

#endif
