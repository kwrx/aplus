/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _PEACH_H
#define _PEACH_H



enum {
    PEACH_MSG_ERROR = 0,
    PEACH_MSG_SUBSCRIBE,
    PEACH_MSG_DISPLAY,
    PEACH_MSG_WINDOW_CREATE,
    PEACH_MSG_WINDOW_DESTROY,
    PEACH_MSG_WINDOW_SET_TITLE,
    PEACH_MSG_WINDOW_GET_TITLE,
    PEACH_MSG_WINDOW_SET_BOUNDS,
    PEACH_MSG_WINDOW_GET_BOUNDS,
    PEACH_MSG_WINDOW_SET_FLAGS,
    PEACH_MSG_WINDOW_GET_FLAGS,


    PEACH_MSG_MAGIC = 0x55AA
};


enum {
    PEACH_WINDOW_SHOW = 1,
    PEACH_WINDOW_BORDERLESS = 2
};


struct peach_msg {
    struct {
        uint16_t h_magic;
        pid_t h_pid;
        uint16_t h_type;
        uint16_t h_size;
    } __attribute__((packed)) msg_header;

    union {
        struct {
            uint8_t d_index;
            uint8_t d_active;
            uint16_t d_width;
            uint16_t d_height;
            uint16_t d_bpp;
        } __attribute__((packed)) msg_display;

        struct {
            uint32_t w_id;
            uint16_t w_width;
            uint16_t w_height;
            void* w_frame;
        } __attribute__((packed)) msg_window;

        struct {
            uint32_t w_id;
            char w_title[1];
        } __attribute__((packed)) msg_window_title;

        struct {
            uint32_t w_id;
            uint16_t w_flags;
            uint8_t w_set;
        } __attribute__((packed)) msg_window_flags;

        struct {
            uint32_t w_id;
            uint16_t w_x;
            uint16_t w_y;
            uint16_t w_width;
            uint16_t w_height;
        } __attribute__((packed)) msg_window_bounds;

        struct {
            int e_errno;
            uint16_t e_type;
            char e_details[1];
        } __attribute__((packed)) msg_error;

        char msg_data[BUFSIZ];
    };
} __attribute__((packed));

#endif