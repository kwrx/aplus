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


#include <peach/peach.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cairo/cairo.h>
using namespace std;


Renderer::Renderer(Peach* c, uint16_t w, uint16_t h) {
    cairo_format_t fmt;
    switch(c->DefaultScreen->BitsPerPixel) {
        case 16:
            fmt = CAIRO_FORMAT_RGB16_565;
            break;
        case 24:
            fmt = CAIRO_FORMAT_RGB24;
            break;
        case 32:
            fmt = CAIRO_FORMAT_ARGB32;
            break;
        default:
            peach_die("BitsPerPixel");
    }


    this->context = c;
    this->surface = cairo_image_surface_create(fmt, w, h);
    this->cairo = cairo_create(this->surface);
}

Renderer::Renderer(Peach* c, uint16_t w, uint16_t h, void* p) {
    cairo_format_t fmt;
    switch(c->DefaultScreen->BitsPerPixel) {
        case 16:
            fmt = CAIRO_FORMAT_RGB16_565;
            break;
        case 24:
            fmt = CAIRO_FORMAT_RGB24;
            break;
        case 32:
            fmt = CAIRO_FORMAT_ARGB32;
            break;
        default:
            peach_die("BitsPerPixel: " << c->DefaultScreen->BitsPerPixel);
    }


    this->context = c;
    this->surface = cairo_image_surface_create_for_data (
        (unsigned char*) p,
        fmt,
        w,
        h,
        w * (c->DefaultScreen->BitsPerPixel >> 3)
    );

    this->cairo = cairo_create(this->surface);
}

Renderer::~Renderer() {
    cairo_surface_destroy(this->surface);
    cairo_destroy(this->cairo);
}
