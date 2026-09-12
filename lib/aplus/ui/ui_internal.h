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

#include <aplus/ui.h>


struct ui_window {

    ui_connection_t* conn;

    uint32_t id;
    uint32_t serial;

    int width;
    int height;

    uint32_t* pixels;

    //? Pixels the allocation can hold, which is not the same as width * height. A resize
    //? that fits reuses the buffer: sys_mmap() never rewinds its cursor and munmap() does
    //? not give the address space back, so a client that reallocated on every configure
    //? would eat its own mmap window one drag at a time.
    size_t capacity;

    struct {

        bool valid;

        int x0;
        int y0;
        int x1;
        int y1;

    } damage;

    //? A configure that has arrived but has not been acted on yet. The surface is only
    //? reallocated when the caller asks, because ui_next_event() usually runs on a
    //? different thread from the one drawing into the pixels.
    struct {

        bool valid;

        int width;
        int height;

        uint32_t serial;

    } pending;

    struct ui_window* next;
};


struct ui_connection {

    int fd;
    ui_window_t* windows;
};


ui_window_t* ui_window_from_id(ui_connection_t* conn, uint32_t id);
int ui_window_reconfigure(ui_window_t* win, int width, int height, uint32_t serial);

#endif
