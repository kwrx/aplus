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

#include <fcntl.h>
#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cairo/cairo.h>

#include <jpeglib.h>

#include <aplus/cairo-ext/cairo-jpeg.h>

#ifndef O_BINARY
    #define O_BINARY 0
#endif

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define CAIRO_JPEG_COLOR_SPACE JCS_EXT_XRGB
#else
    #define CAIRO_JPEG_COLOR_SPACE JCS_EXT_BGRX
#endif


extern cairo_surface_t* _cairo_surface_create_in_error(cairo_status_t);


/**
 * @brief The libjpeg error manager, extended with the place to jump back to.
 */

typedef struct {

    struct jpeg_error_mgr pub;

    jmp_buf env;

} cairo_jpeg_error_t;


static const cairo_user_data_key_t cairo_jpeg_data_key;


/**
 * @brief Leaves the decoder instead of exiting the process, which is what libjpeg does by default.
 *
 * @param cinfo The decompressor the error came from.
 */

static void _cairo_jpeg_error_exit(j_common_ptr cinfo) {

    cairo_jpeg_error_t* err = (cairo_jpeg_error_t*)cinfo->err;

    longjmp(err->env, 1);
}


/**
 * @brief Releases the pixels a surface was created over.
 *
 * @param data The buffer.
 */

static void _cairo_jpeg_destroy_data(void* data) {

    free(data);
}


/**
 * @brief Decodes a jpeg image held in memory.
 *
 * @param src The encoded bytes.
 * @param size How many of them there are.
 * @return The surface, which owns the pixels, or a surface in error status.
 */

static cairo_surface_t* _cairo_image_surface_decode_jpeg(const void* src, size_t size) {

    struct jpeg_decompress_struct cinfo;
    cairo_jpeg_error_t err;

    unsigned char* volatile data = NULL;


    cinfo.err          = jpeg_std_error(&err.pub);
    err.pub.error_exit = _cairo_jpeg_error_exit;

    if (setjmp(err.env)) {

        jpeg_destroy_decompress(&cinfo);
        free(data);

        return _cairo_surface_create_in_error(CAIRO_STATUS_READ_ERROR);
    }


    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, (const unsigned char*)src, (unsigned long)size);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {

        jpeg_destroy_decompress(&cinfo);
        return _cairo_surface_create_in_error(CAIRO_STATUS_READ_ERROR);
    }


    cinfo.out_color_space = CAIRO_JPEG_COLOR_SPACE;

    jpeg_start_decompress(&cinfo);


    const int width  = (int)cinfo.output_width;
    const int height = (int)cinfo.output_height;

    const int stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, width);

    if (width <= 0 || height <= 0 || stride <= 0 || height > INT_MAX / stride) {

        jpeg_destroy_decompress(&cinfo);
        return _cairo_surface_create_in_error(CAIRO_STATUS_INVALID_SIZE);
    }


    if (!(data = (unsigned char*)malloc((size_t)stride * (size_t)height))) {

        jpeg_destroy_decompress(&cinfo);
        return _cairo_surface_create_in_error(CAIRO_STATUS_NO_MEMORY);
    }


    while (cinfo.output_scanline < cinfo.output_height) {

        unsigned char* row = data + (size_t)stride * (size_t)cinfo.output_scanline;

        jpeg_read_scanlines(&cinfo, &row, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);


    cairo_surface_t* surface = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_RGB24, width, height, stride);

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {

        free(data);
        return surface;
    }

    if (cairo_surface_set_user_data(surface, &cairo_jpeg_data_key, data, _cairo_jpeg_destroy_data) != CAIRO_STATUS_SUCCESS) {

        cairo_surface_destroy(surface);
        free(data);

        return _cairo_surface_create_in_error(CAIRO_STATUS_NO_MEMORY);
    }

    cairo_surface_mark_dirty(surface);

    return surface;
}


__attribute__((weak)) cairo_surface_t* cairo_image_surface_create_from_jpeg_data(const void* data, size_t size) {

    if (!data || !size)
        return _cairo_surface_create_in_error(CAIRO_STATUS_READ_ERROR);

    return _cairo_image_surface_decode_jpeg(data, size);
}


__attribute__((weak)) cairo_surface_t* cairo_image_surface_create_from_jpeg(const char* filename) {

    int fd = open(filename, O_RDONLY | O_BINARY);
    if (fd < 0)
        return _cairo_surface_create_in_error(CAIRO_STATUS_FILE_NOT_FOUND);


    struct stat st;
    fstat(fd, &st);

    void* buf = (void*)calloc(st.st_size, sizeof(char));
    if (!buf) {
        close(fd);
        return _cairo_surface_create_in_error(CAIRO_STATUS_NO_MEMORY);
    }

    if (read(fd, buf, st.st_size) != st.st_size) {
        close(fd);
        free(buf);
        return _cairo_surface_create_in_error(CAIRO_STATUS_READ_ERROR);
    }

    close(fd);


    cairo_surface_t* surface = _cairo_image_surface_decode_jpeg(buf, st.st_size);
    free(buf);

    return surface;
}
