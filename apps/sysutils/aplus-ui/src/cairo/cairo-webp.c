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

#pragma GCC diagnostic ignored "-Wparentheses"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cairo/cairo.h>

#include <webp/decode.h>
#include <webp/encode.h>
#include <webp/types.h>



extern cairo_surface_t* _cairo_surface_create_in_error(cairo_status_t);


static cairo_surface_t* _cairo_image_surface_decode_webp(const void* data, size_t size) {

    assert(data);
    assert(size);


    int w, h;
    if(!WebPGetInfo(data, size, &w, &h)) {
        return _cairo_surface_create_in_error(CAIRO_STATUS_READ_ERROR);
    }


    uint8_t* frame = WebPDecodeBGRA(data, size, &w, &h);

    if(!frame) {
        return _cairo_surface_create_in_error(CAIRO_STATUS_READ_ERROR);
    }


    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        frame,
        CAIRO_FORMAT_ARGB32,
        w,
        h,
        cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, w)
    );


    if(!surface || cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        WebPFree(frame);
        return _cairo_surface_create_in_error(CAIRO_STATUS_READ_ERROR);
    }

    return surface;

}



__attribute__((weak))
cairo_surface_t* cairo_image_surface_create_from_webp(const char* filename) {

    assert(filename);

    
    int fd = open(filename, O_RDONLY);

    if(fd < 0) {
        return _cairo_surface_create_in_error(CAIRO_STATUS_READ_ERROR);
    }


    struct stat st;
    if(fstat(fd, &st) < 0) {
        close(fd);
        return _cairo_surface_create_in_error(CAIRO_STATUS_READ_ERROR);
    }


    void* data = malloc(st.st_size);

    if(!data) {
        close(fd);
        return _cairo_surface_create_in_error(CAIRO_STATUS_NO_MEMORY);
    }


    if(read(fd, data, st.st_size) != st.st_size) {
        free(data);
        close(fd);
        return _cairo_surface_create_in_error(CAIRO_STATUS_READ_ERROR);
    }


    close(fd);


    cairo_surface_t* surface = _cairo_image_surface_decode_webp(data, st.st_size);

    return free(data), surface;
    
}
