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

/**
 * @brief The desktop picture, out of a webp file named on the command line.
 *
 * The image is decoded and scaled to cover the screen once, at startup, so that compositing the
 * desktop stays the plain opaque blit it is with a gradient rather than a resample per frame.
 */

#include <stdio.h>
#include <stdlib.h>

#include <webp/decode.h>

#include <wm.h>


/**
 * @brief Decodes the file into a surface of the given size, which the caller has matched to the image.
 *
 * The decoder's BGRA is already the byte order cairo calls RGB24 on a little-endian machine, with
 * the alpha it drops landing in the byte RGB24 ignores, so the pixels go straight into the surface.
 *
 * @param file The contents of the webp file.
 * @param size The size of the file.
 * @param width The width of the image.
 * @param height The height of the image.
 * @return The surface, or NULL.
 */
static cairo_surface_t* wm_wallpaper_decode(const void* file, size_t size, int width, int height) {

    cairo_surface_t* surface = wm_surface_create(CAIRO_FORMAT_RGB24, width, height);

    if (!surface) {
        return NULL;
    }


    const int stride = cairo_image_surface_get_stride(surface);

    if (!WebPDecodeBGRAInto(file, size, cairo_image_surface_get_data(surface), (size_t)stride * (size_t)height, stride)) {
        cairo_surface_destroy(surface);
        return NULL;
    }

    cairo_surface_mark_dirty(surface);

    return surface;
}


/**
 * @brief Scales the image to cover the screen, cropping whichever axis is left over.
 *
 * The source is padded rather than left undefined at its edges: an image of the screen's own
 * aspect ratio covers it exactly, and the filter then samples right at the boundary, where an
 * undefined edge would show up as a dark seam around the desktop.
 *
 * @param src The image at its own size.
 * @param iw The width of the image.
 * @param ih The height of the image.
 * @param sw The width of the screen.
 * @param sh The height of the screen.
 * @return The screen-sized surface, or NULL.
 */
static cairo_surface_t* wm_wallpaper_cover(cairo_surface_t* src, int iw, int ih, int sw, int sh) {

    cairo_surface_t* surface = wm_surface_create(CAIRO_FORMAT_RGB24, sw, sh);

    if (!surface) {
        return NULL;
    }


    cairo_t* cr = cairo_create(surface);

    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return NULL;
    }


    const double sx    = (double)sw / (double)iw;
    const double sy    = (double)sh / (double)ih;
    const double scale = WM_MAX(sx, sy);

    cairo_translate(cr, ((double)sw - (double)iw * scale) / 2.0, ((double)sh - (double)ih * scale) / 2.0);
    cairo_scale(cr, scale, scale);

    cairo_set_source_surface(cr, src, 0.0, 0.0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);

    cairo_destroy(cr);

    return surface;
}


/**
 * @brief Builds the desktop out of a webp file, at the size of the screen.
 *
 * @param path The image to read.
 * @param width The width of the screen.
 * @param height The height of the screen.
 * @return The pattern to paint the desktop with, or NULL to fall back to the gradient.
 */
cairo_pattern_t* wm_wallpaper_load(const char* path, int width, int height) {

    if (!path || width <= 0 || height <= 0) {
        return NULL;
    }


    size_t size = 0;

    int iw = 0;
    int ih = 0;

    void* file = wm_image_probe(path, WM_WALLPAPER_MAX_SIZE, &size, &iw, &ih);

    if (!file) {
        return NULL;
    }


    cairo_surface_t* surface = wm_wallpaper_decode(file, size, iw, ih);

    free(file);

    if (!surface) {
        fprintf(stderr, "aplus-wm: warning: cannot decode %s\n", path);
        return NULL;
    }


    if (iw != width || ih != height) {

        cairo_surface_t* scaled = wm_wallpaper_cover(surface, iw, ih, width, height);

        cairo_surface_destroy(surface);

        if (!scaled) {
            fprintf(stderr, "aplus-wm: warning: cannot scale %s to %dx%d\n", path, width, height);
            return NULL;
        }

        surface = scaled;
    }


    cairo_pattern_t* pattern = cairo_pattern_create_for_surface(surface);

    cairo_surface_destroy(surface);

    if (cairo_pattern_status(pattern) != CAIRO_STATUS_SUCCESS) {
        cairo_pattern_destroy(pattern);
        return NULL;
    }

    cairo_pattern_set_filter(pattern, CAIRO_FILTER_NEAREST);
    cairo_pattern_set_extend(pattern, CAIRO_EXTEND_PAD);

    fprintf(stderr, "aplus-wm: %s is %dx%d, covering %dx%d\n", path, iw, ih, width, height);

    return pattern;
}


/**
 * @brief Builds the desktop the server falls back on when it has no wallpaper.
 *
 * @param height The height of the screen, which the gradient spans.
 * @return The pattern, or NULL.
 */
cairo_pattern_t* wm_wallpaper_gradient(int height) {

    cairo_pattern_t* pattern = cairo_pattern_create_linear(0.0, 0.0, 0.0, (double)height);

    if (cairo_pattern_status(pattern) != CAIRO_STATUS_SUCCESS) {
        cairo_pattern_destroy(pattern);
        return NULL;
    }

    cairo_pattern_add_color_stop_rgb(pattern, 0.0, WM_COLOR_DESKTOP_TOP);
    cairo_pattern_add_color_stop_rgb(pattern, 1.0, WM_COLOR_DESKTOP_BOTTOM);

    return pattern;
}
