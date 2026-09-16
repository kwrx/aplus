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
 * @brief The pointer, as an image out of the cursor theme in /usr/share/cursors.
 *
 * The plane takes the decoder's straight alpha as it is; cairo composites a premultiplied copy of it.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <webp/decode.h>

#include <wm.h>


typedef struct {

    const char* name;

    //? The pixel inside the image that lands on the pointer position. For an arrow it is the
    //? tip; for the resize shapes, which are double-headed and symmetric, it is the middle.
    int hot_x;
    int hot_y;

    //? Loading is attempted once per shape and remembered either way, so a theme with a file
    //? missing does not re-open it on every pointer movement.
    bool tried;

    int width;
    int height;

    cairo_surface_t* surface;

    //? Straight-alpha ARGB32 as the decoder returned it, for the cursor plane. NULL where the
    //? pointer is drawn into the frame instead, since there the premultiplied copy is the only
    //? one anything reads.
    uint32_t* image;

} wm_cursor_t;


static wm_cursor_t wm_cursors[WM_CURSOR_COUNT] = {

    [WM_CURSOR_ARROW]      = {.name = "arrow",      .hot_x = 3,  .hot_y = 2 },
    [WM_CURSOR_HAND]       = {.name = "hand",       .hot_x = 16, .hot_y = 2 },
    [WM_CURSOR_SIZE_ALL]   = {.name = "size_all",   .hot_x = 15, .hot_y = 15},
    [WM_CURSOR_SIZE_HOR]   = {.name = "size_hor",   .hot_x = 16, .hot_y = 15},
    [WM_CURSOR_SIZE_VER]   = {.name = "size_ver",   .hot_x = 16, .hot_y = 15},
    [WM_CURSOR_SIZE_FDIAG] = {.name = "size_fdiag", .hot_x = 16, .hot_y = 15},
    [WM_CURSOR_SIZE_BDIAG] = {.name = "size_bdiag", .hot_x = 16, .hot_y = 15},
};

static wm_cursor_shape_t wm_cursor_shape = WM_CURSOR_ARROW;


/**
 * @brief Reads a whole file into memory, which is how the decoder wants the image.
 *
 * @param path The file to read.
 * @param size Receives the size of the file.
 * @return The contents, or NULL with errno set.
 */
static void* wm_cursor_slurp(const char* path, size_t* size) {

    int fd;

    if ((fd = open(path, O_RDONLY)) < 0) {
        fprintf(stderr, "aplus-wm: warning: cannot open %s: %s\n", path, strerror(errno));
        return NULL;
    }


    struct stat st;

    if (fstat(fd, &st) < 0 || st.st_size <= 0) {
        fprintf(stderr, "aplus-wm: warning: cannot stat %s\n", path);
        close(fd);
        return NULL;
    }


    uint8_t* buffer = malloc((size_t)st.st_size);

    if (!buffer) {
        close(fd);
        return NULL;
    }


    size_t left = (size_t)st.st_size;
    uint8_t* at = buffer;

    while (left > 0) {

        ssize_t n = read(fd, at, left);

        if (n > 0) {

            at += n;
            left -= (size_t)n;

            continue;
        }

        if (n < 0 && errno == EINTR) {
            continue;
        }

        fprintf(stderr, "aplus-wm: warning: cannot read %s: %s\n", path, strerror(errno));

        free(buffer);
        close(fd);

        return NULL;
    }

    close(fd);

    *size = (size_t)st.st_size;

    return buffer;
}


/**
 * @brief Builds the premultiplied copy cairo composites from, rounding the division rather than truncating it.
 *
 * @param image The straight-alpha pixels the decoder produced.
 * @param width The width of the image.
 * @param height The height of the image.
 * @return The surface, or NULL.
 */
static cairo_surface_t* wm_cursor_premultiply(const uint32_t* image, int width, int height) {

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return NULL;
    }


    unsigned char* data = cairo_image_surface_get_data(surface);
    const int stride    = cairo_image_surface_get_stride(surface);

    for (int y = 0; y < height; y++) {

        uint32_t* row = (uint32_t*)(data + (size_t)y * stride);

        for (int x = 0; x < width; x++) {

            const uint32_t pixel = image[(size_t)y * width + x];
            const uint32_t alpha = pixel >> 24;

            if (alpha == 0) {
                row[x] = 0;
                continue;
            }

            if (alpha == 0xFF) {
                row[x] = pixel;
                continue;
            }

            const uint32_t r = (((pixel >> 16) & 0xFF) * alpha + 0x7F) / 0xFF;
            const uint32_t g = (((pixel >> 8) & 0xFF) * alpha + 0x7F) / 0xFF;
            const uint32_t b = (((pixel >> 0) & 0xFF) * alpha + 0x7F) / 0xFF;

            row[x] = (alpha << 24) | (r << 16) | (g << 8) | b;
        }
    }

    cairo_surface_mark_dirty(surface);

    return surface;
}


/**
 * @brief Decodes one shape out of the theme, once, and keeps it.
 *
 * @param cursor The shape to load.
 * @return true when the shape has an image.
 */
static bool wm_cursor_load(wm_cursor_t* cursor) {

    if (cursor->tried) {
        return cursor->surface != NULL;
    }

    cursor->tried = true;


    char path[128];

    snprintf(path, sizeof(path), "%s/%s.webp", WM_CURSOR_PATH, cursor->name);


    size_t size = 0;
    void* file  = wm_cursor_slurp(path, &size);

    if (!file) {
        return false;
    }


    int width  = 0;
    int height = 0;

    if (!WebPGetInfo(file, size, &width, &height)) {
        fprintf(stderr, "aplus-wm: warning: %s is not a webp image\n", path);
        free(file);
        return false;
    }

    if (width <= 0 || height <= 0 || width > WM_CURSOR_MAX_SIZE || height > WM_CURSOR_MAX_SIZE) {
        fprintf(stderr, "aplus-wm: warning: %s is %dx%d, which is not a usable cursor size\n", path, width, height);
        free(file);
        return false;
    }


    if (cursor->hot_x >= width) {
        cursor->hot_x = width - 1;
    }

    if (cursor->hot_y >= height) {
        cursor->hot_y = height - 1;
    }


    uint8_t* pixels = WebPDecodeBGRA(file, size, &width, &height);

    free(file);

    if (!pixels) {
        fprintf(stderr, "aplus-wm: warning: cannot decode %s\n", path);
        return false;
    }


    cursor->surface = wm_cursor_premultiply((const uint32_t*)pixels, width, height);

    if (!cursor->surface) {
        WebPFree(pixels);
        return false;
    }

    cursor->width  = width;
    cursor->height = height;

    if (wm.display.hwcursor) {
        cursor->image = (uint32_t*)pixels;
    } else {
        WebPFree(pixels);
    }

    return true;
}


/**
 * @brief Draws the arrow to fall back on when the theme has given us nothing, with its tip at (x, y).
 *
 * @param cr The cairo context to draw with.
 * @param x The tip of the arrow.
 * @param y The tip of the arrow.
 */
static void wm_cursor_paint_fallback(cairo_t* cr, double x, double y) {

    cairo_save(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x, y + WM_CURSOR_HEIGHT);
    cairo_line_to(cr, x + 4, y + WM_CURSOR_HEIGHT - 4);
    cairo_line_to(cr, x + WM_CURSOR_WIDTH, y + WM_CURSOR_HEIGHT - 4);
    cairo_close_path(cr);

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_fill_preserve(cr);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_restore(cr);
}


/**
 * @brief Reports the rectangle the pointer occupies right now, which a software-drawn pointer must damage.
 *
 * @return The rectangle, with the margin the stroked fallback arrow needs.
 */
wm_rect_t wm_cursor_rect(void) {

    const wm_cursor_t* cursor = &wm_cursors[wm_cursor_shape];

    wm_rect_t r;

    if (cursor->surface) {

        r.x      = wm.pointer.x - cursor->hot_x;
        r.y      = wm.pointer.y - cursor->hot_y;
        r.width  = cursor->width;
        r.height = cursor->height;

    } else {

        r.x      = wm.pointer.x - 2;
        r.y      = wm.pointer.y - 2;
        r.width  = WM_CURSOR_WIDTH + 4;
        r.height = WM_CURSOR_HEIGHT + 4;
    }

    return r;
}


/**
 * @brief Draws the current shape with its hotspot on (x, y), for the software path alone.
 *
 * @param cr The cairo context to draw with.
 * @param x The hotspot's position.
 * @param y The hotspot's position.
 */
void wm_cursor_paint(cairo_t* cr, double x, double y) {

    const wm_cursor_t* cursor = &wm_cursors[wm_cursor_shape];

    if (!cursor->surface) {
        wm_cursor_paint_fallback(cr, x, y);
        return;
    }


    cairo_save(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_surface(cr, cursor->surface, x - cursor->hot_x, y - cursor->hot_y);
    cairo_paint(cr);

    cairo_restore(cr);
}


/**
 * @brief Hands the current shape to the cursor plane.
 *
 * @return 0 on success, or -1 when there is no usable plane.
 */
int wm_cursor_upload(void) {

    wm_cursor_t* cursor = &wm_cursors[wm_cursor_shape];

    if (!wm_cursor_load(cursor) || !cursor->image) {
        return -1;
    }

    return wm_display_cursor_image(&wm.display, cursor->image, cursor->width, cursor->height, cursor->hot_x, cursor->hot_y);
}


/**
 * @brief Switches the pointer to another shape, repainting both boxes where there is no cursor plane.
 *
 * @param shape The shape to switch to.
 */
void wm_cursor_set(wm_cursor_shape_t shape) {

    if (shape == wm_cursor_shape) {
        return;
    }

    if (!wm_cursor_load(&wm_cursors[shape]) && shape != WM_CURSOR_ARROW) {
        return;
    }


    if (wm.display.hwcursor) {

        wm_cursor_shape = shape;

        wm_cursor_upload();

        return;
    }


    const wm_rect_t before = wm_cursor_rect();

    wm_cursor_shape = shape;

    const wm_rect_t after = wm_cursor_rect();

    wm_damage(&before);
    wm_damage(&after);
}


void wm_cursor_fini(void) {

    for (size_t i = 0; i < WM_CURSOR_COUNT; i++) {

        if (wm_cursors[i].surface) {
            cairo_surface_destroy(wm_cursors[i].surface);
            wm_cursors[i].surface = NULL;
        }

        if (wm_cursors[i].image) {
            WebPFree(wm_cursors[i].image);
            wm_cursors[i].image = NULL;
        }

        wm_cursors[i].tried = false;
    }
}
