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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <aplus/base.h>
#include <aplus/utils/async.h>
#include <cairo/cairo.h>
#include "../peach.h"


#define window_property(decl, body)                 \
    int decl {                                      \
        if(id > NR_WINDOW) {                        \
            errno = ESRCH;                          \
            return -1;                              \
        }                                           \
                                                    \
        peach_window_t* s = &window[id];            \
        if(!s->w_active) {                          \
            errno = EINVAL;                         \
            return -1;                              \
        }                                           \
                                                    \
        body;                                       \
    }


peach_window_t window[NR_WINDOW];
uint32_t window_count = 0;



int window_drawer(void) {
    void __window_border(peach_window_t* window) {
#if 0
        cairo_save(current_display->d_backbuffer.b_cairo);
        cairo_set_source_rgb(current_display->d_backbuffer.b_cairo, 0.5, 0.5, 0.5);
        cairo_rectangle(current_display->d_backbuffer.b_cairo, 0.0, 0.0, (double) window->w_width, 30.0);
        cairo_fill(current_display->d_backbuffer.b_cairo);
        cairo_rectangle(current_display->d_backbuffer.b_cairo, 0.0, 30.0, (double) window->w_width, (double) window->w_height + 30.0)
        cairo_restore(current_display->d_backbuffer.b_cairo);
#endif    
    }

    void __window_borderless(peach_window_t* window) {
        cairo_save(current_display->d_backbuffer.b_cairo);
        cairo_set_source_surface(current_display->d_backbuffer.b_cairo, window->w_surface, 0, 0);
        cairo_rectangle(current_display->d_backbuffer.b_cairo, 0.0, 0.0, (double) window->w_width, (double) window->w_height);
        cairo_set_operator(current_display->d_backbuffer.b_cairo, CAIRO_OPERATOR_SOURCE);
        cairo_paint(current_display->d_backbuffer.b_cairo);
        cairo_restore(current_display->d_backbuffer.b_cairo);
    }


    async_do ({
        for(int i = 0, j = 0; i < NR_DISPLAY && j < window_count; i++) {
            if(!window[i].w_active)
                continue;

            j++;

            if(!(window[i].w_flags & PEACH_WINDOW_SHOW))
                continue;

            if(!(window[i].w_flags & PEACH_WINDOW_BORDERLESS))
                __window_border(&window[i]);
            else
                __window_borderless(&window[i]);
            
        }

        usleep(16666);
    }, NULL);
}


peach_window_t* window_create(pid_t pid, uint16_t w, uint16_t h) {
    if(!current_display) {
        errno = ENOSYS;
        return NULL;
    }

    if(!w || !h) {
        errno = EINVAL;
        return NULL;
    }

    peach_window_t* s = NULL;
    for(int i = 0; i < NR_WINDOW; i++) {
        if(window[i].w_active)
            continue;

        s = &window[i];
        s->w_id = i;
        break;
    }

    if(!s) {
        errno = ENOMEM;
        return NULL;
    }

    s->w_display = current_display->d_id;
    s->w_pid = pid;
    s->w_flags = 0;
    s->w_x = 0;
    s->w_y = 0;
    s->w_width = w;
    s->w_height = h;
    s->w_frame = (void*) mmap(NULL, w * h * (current_display->d_bpp / 8), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if(!s->w_frame) {
        errno = ENOMEM;
        return NULL;
    }

    s->w_surface = cairo_image_surface_create_for_data (
        (unsigned char*) s->w_frame,
        cairo_bpp2fmt(current_display->d_bpp),
        w,
        h,
        w * (current_display->d_bpp / 8)
    );

    s->w_cairo = cairo_create(s->w_surface);
    s->w_active = 1;

    window_count++;
    return s;
}

int window_destroy(uint32_t id) {
    if(id > NR_WINDOW) {
        errno = ESRCH;
        return -1;
    }

    peach_window_t* s = &window[id];
    if(!s->w_active) {
        errno = EINVAL;
        return -1;
    }

    cairo_surface_destroy(s->w_surface);
    cairo_destroy(s->w_cairo);
    munmap(s->w_frame, s->w_width * s->w_height * (current_display->d_bpp / 8));

    s->w_active = 0;

    window_count--;
    return 0;
}

window_property (
    window_set_title(uint32_t id, char* title), {
        strncpy(s->w_title, title, NR_WTITLE - 1);
        return 0;
    }
)

window_property (
    window_get_title(uint32_t id, char* title), {
        strncpy(title, s->w_title, NR_WTITLE - 1);
        return 0;
    }
)

window_property (
    window_set_flags(uint32_t id, uint16_t flags, uint8_t set), {
        if(set)
            s->w_flags |= flags;
        else
            s->w_flags &= ~(flags);
        
        return 0;
    }
)

window_property (
    window_get_flags(uint32_t id, uint16_t* flags), {
        if(flags)
            *flags = s->w_flags;

        return 0;
    }
)

window_property (
    window_set_bounds(uint32_t id, uint16_t x, uint16_t y, uint16_t w, uint16_t h), {
        if(w != -1)
            s->w_width = w;
        if(h != -1)
            s->w_height = h;
        if(x != -1)
            s->w_x = x;
        if(y != -1)
            s->w_y = y;
            
        // if(w != -1 || h != -1)
        //    __window_resize(s);
        return 0;
    }
)

window_property (
    window_get_bounds(uint32_t id, uint16_t* x, uint16_t* y, uint16_t* w, uint16_t* h), {
        if(w)
            *w = s->w_width;
        if(h)
            *h = s->w_height;
        if(x)
            *x = s->w_x;
        if(y)
            *y = s->w_y;
            
        return 0;
    }
)


