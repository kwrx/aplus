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

#include <aplus/base.h>
#include <aplus/fb.h>
#include <cairo/cairo.h>
#include "../peach.h"


peach_display_t display[NR_DISPLAY];
peach_display_t* current_display = NULL;



int init_display(uint16_t w, uint16_t h, uint16_t bpp, void* framebuffer, int doublebuffer) {
    int i;
    for(i = 0; i < NR_DISPLAY; i++)
        if(!display[i].d_active)
            break;

    if(i == NR_DISPLAY) {
        errno = EBUSY;
        return -1;
    } 



    display[i].d_active = 1;
    display[i].d_id = i;
    display[i].d_width = w;
    display[i].d_height = h;
    display[i].d_bpp = bpp;
    display[i].d_stride = w * (bpp / 8);

    display[i].d_backbuffer.b_raw = !doublebuffer
                                        ? (void*) framebuffer
                                        : malloc(w * h * (bpp / 8))
                                        ;

    display[i].d_framebuffer.b_raw = !doublebuffer
                                        ? NULL 
                                        : framebuffer
                                        ;

    if(!display[i].d_backbuffer.b_raw) {
        errno = ENOMEM;
        return -1;
    }


    display[i].d_backbuffer.b_surface = cairo_image_surface_create_for_data (
        (unsigned char*) display[i].d_backbuffer.b_raw,
        cairo_bpp2fmt(display[i].d_bpp),
        display[i].d_width,
        display[i].d_height,
        display[i].d_stride
    );

    if(!display[i].d_backbuffer.b_surface)
        return -1;

    display[i].d_backbuffer.b_cairo = cairo_create(display[i].d_backbuffer.b_surface);
    
    if(!doublebuffer)
        goto done;


    display[i].d_framebuffer.b_surface = cairo_image_surface_create_for_data (
        (unsigned char*) display[i].d_framebuffer.b_raw,
        cairo_bpp2fmt(display[i].d_bpp),
        display[i].d_width,
        display[i].d_height,
        display[i].d_stride
    );

    if(!display[i].d_framebuffer.b_surface)
        return -1;

    display[i].d_framebuffer.b_cairo = cairo_create(display[i].d_framebuffer.b_surface);
    

done:
    current_display = &display[i];
    return 0;
}