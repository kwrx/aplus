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


#pragma once

#include <aplus/base.h>
#include <aplus/msg.h>
#include <aplus/peach.h>

#include <cairo/cairo.h>


#define NR_DISPLAY          16
#define NR_WINDOW           2048
#define NR_WTITLE           64


#define cairo_bpp2fmt(x)                                        \
    x == 16 ? CAIRO_FORMAT_RGB16_565    : (                     \
    x == 24 ? CAIRO_FORMAT_RGB24        : (                     \
    x == 32 ? CAIRO_FORMAT_ARGB32       : (                     \
    CAIRO_FORMAT_INVALID))) 

#define msg_build(m, t, s) {                                    \
    (m)->msg_header.h_magic = PEACH_MSG_MAGIC;                  \
    (m)->msg_header.h_type = t;                                 \
    (m)->msg_header.h_size = s;                                 \
}

#define msg_send(p, m, s) {                                     \
    if(ipc_msg_send(p, m, sizeof((m)->msg_header) + s) < 0)     \
        die("ipc_msg_send");                                    \
}



#define API(name)                                               \
    void api_##name(struct peach_msg* msg)

#define API_DECL(name)                                          \
    [name] = &api_##name


#define API_REPLY(m, type, size) {                              \
    (m)->msg_header.h_magic = PEACH_MSG_MAGIC;                  \
    (m)->msg_header.h_type = type;                              \
    (m)->msg_header.h_size = size;                              \
    msg_send((m)->msg_header.h_pid, m, size)                    \
}

#define API_ERROR(m, e, d) {                                    \
    printf("peach: API-ERROR: %s: %s\n", strerror(e), d);       \
    (m)->msg_error.e_errno = e;                                 \
    (m)->msg_error.e_type = (m)->msg_header.h_type;             \
    memcpy(&(m)->msg_error.e_details[0], d, strlen(d) + 1);     \
    API_REPLY(m, PEACH_MSG_ERROR, strlen(d) + 1 + sizeof(int)); \
    return;                                                     \
}



typedef struct peach_display {
    uint8_t d_active;
    uint32_t d_id;
    uint16_t d_width;
    uint16_t d_height;
    uint16_t d_bpp;
    uint32_t d_stride;
    struct {
        void* b_raw;

        cairo_surface_t* b_surface;
        cairo_t* b_cairo;
    } d_backbuffer;


    struct {
        void* b_raw;

        cairo_surface_t* b_surface;
        cairo_t* b_cairo;
    } d_framebuffer;
    
    void (*d_flip) (struct peach_display*);
} peach_display_t;

typedef struct {
    uint8_t w_active;
    uint32_t w_id;
    uint32_t w_display;
    pid_t w_pid;
    uint16_t w_flags;
    
    uint16_t w_x;
    uint16_t w_y;
    uint16_t w_width;
    uint16_t w_height;

    char w_title[NR_WTITLE];
    void* w_frame;

    cairo_t* w_cairo;
    cairo_surface_t* w_surface;
} peach_window_t;


typedef void (*api_t)(struct peach_msg*);

extern peach_display_t display[];
extern peach_window_t window[];
extern api_t api_list[];
extern peach_display_t* current_display;


void die(char*);
int init_display(uint16_t w, uint16_t h, uint16_t bpp, void* framebuffer, int doublebuffer);

int window_drawer(void);
peach_window_t* window_create(pid_t pid, uint16_t w, uint16_t h);
int window_destroy(uint32_t id);
int window_set_title(uint32_t id, char* title);
int window_get_title(uint32_t id, char* title);
int window_set_flags(uint32_t id, uint16_t flags, uint8_t set);
int window_get_flags(uint32_t id, uint16_t* flags);
int window_set_bounds(uint32_t id, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
int window_get_bounds(uint32_t id, uint16_t* x, uint16_t* y, uint16_t* w, uint16_t* h);