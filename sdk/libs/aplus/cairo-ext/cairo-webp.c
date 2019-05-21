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


#ifndef NO_CAIRO_EXTENSION

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cairo/cairo.h>

#include <webp/decode.h>
#include <webp/encode.h>
#include <webp/types.h>

#include <aplus/base.h>
#include <aplus/cairo-ext/cairo-webp.h>
#include <aplus/cairo-ext/cairo-cache.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif


extern cairo_surface_t* _cairo_surface_create_in_error(cairo_status_t);


static cairo_surface_t* _cairo_image_surface_decode_webp(void* src, size_t size) {

    int w, h;
    if(!WebPGetInfo((const uint8_t*) src, size, &w, &h))
        return _cairo_surface_create_in_error(CAIRO_STATUS_READ_ERROR); 


    uint8_t* frame;
    if(!(frame = WebPDecodeBGRA (
        (const uint8_t*) src, size,
        &w, &h
    ))) {
        return _cairo_surface_create_in_error(CAIRO_STATUS_READ_ERROR);
    }


    cairo_surface_t* surface = cairo_image_surface_create_for_data (
        frame,
        CAIRO_FORMAT_ARGB32,
        w,
        h,
        w * 4
    );

    if(!surface)
        WebPFree(frame);

    return surface;
}

__attribute__((weak))
cairo_surface_t* cairo_image_surface_create_from_webp(const char* filename) {
    cairo_surface_t* surface;
    if((surface = cairo_cache_obtain_resource(filename)))
        return surface;

    
    int fd = open(filename, O_RDONLY | O_BINARY);
    if(fd < 0)
        return _cairo_surface_create_in_error(CAIRO_STATUS_FILE_NOT_FOUND);


    struct stat st;
    fstat(fd, &st);

    void* buf = (void*) __libaplus_malloc(st.st_size);
    if(!buf) {
        close(fd);
        return _cairo_surface_create_in_error(CAIRO_STATUS_NO_MEMORY);
    }

    if(read(fd, buf, st.st_size) != st.st_size) {
        close(fd);
        free(buf);
        return _cairo_surface_create_in_error(CAIRO_STATUS_READ_ERROR);
    }

    close(fd);


    surface = _cairo_image_surface_decode_webp(buf, st.st_size);
    free(buf);

    cairo_cache_put_resource(filename, surface);
    return surface;
}


__attribute__((weak))
cairo_surface_t* cairo_image_surface_create_from_webp_stream(cairo_read_func_t read, void* arg) {
    return NULL;
}

#endif