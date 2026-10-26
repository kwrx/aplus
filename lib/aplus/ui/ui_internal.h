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

    uint32_t flags;

    int width;
    int height;

    int asked_width;
    int asked_height;

    size_t stride;

    uint32_t* front;

    uint32_t* back;

    int shm_id;

    ui_damage_t damage;

    ui_cursor_t cursor;

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

    struct {

        uint8_t* data;

        size_t head;
        size_t size;
        size_t capacity;

    } rx;

    struct {

        bool valid;

        ui_msg_theme_t msg;

    } theme;
};


int ui_conn_read(ui_connection_t* conn, void* buf, size_t size);
int ui_conn_message_ready(ui_connection_t* conn);
int ui_conn_fill(ui_connection_t* conn);


ui_window_t* ui_window_from_id(ui_connection_t* conn, uint32_t id);

/**
 * @brief Copies a theme the server sent into the one every view created against the system theme draws with.
 *
 * @param msg The theme, as it came off the wire.
 */
void ui_theme_adopt(const ui_msg_theme_t* msg);

int ui_window_adopt_surface(ui_window_t* win, int width, int height, size_t stride, int shm_id, size_t shm_size, uint32_t serial);
void ui_window_drop_surface(ui_window_t* win);

#endif
