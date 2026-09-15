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

/*
 * The pointer, as an image out of the cursor theme in /usr/share/cursors.
 *
 * The shape is not decoration: it is the only thing that says what the border under the
 * pointer will do before the button goes down. A window edge that resizes horizontally and
 * one that resizes diagonally look identical, and the theme has an arrow for each of them.
 *
 * The two output paths disagree about alpha: cairo composites premultiplied ARGB32, and the
 * cursor plane takes straight alpha. The decoder produces the straight one, so that goes to
 * the plane unchanged and is what the premultiplied copy is built from -- and is kept
 * afterwards only when there is a plane that might ask for the shape again.
 *
 * A theme file that is missing or unreadable is not worth refusing to start over, nor even
 * worth a fallback shape: the pointer keeps the last image that did load, and with none at
 * all it is the arrow this file draws by hand.
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


/* Whole file into memory, or NULL. The decoder wants the image in one piece and none of these
   files is larger than a couple of kilobytes. */
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


/* The premultiplied copy cairo composites from, built from the straight-alpha pixels the
 * decoder produced.
 *
 * Rounding the division rather than truncating it matters at the antialiased edge, which is
 * most of what a 32 pixel cursor is made of: truncating darkens every partly transparent pixel
 * by up to one level, and against the near-black desktop the outline picks up a visible fringe.
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


/* One shape out of the theme, decoded once and kept. Returns whether the shape has an image.
 *
 * WebPDecodeBGRA lays the channels out as B, G, R, A, which read back as a little-endian word
 * is the 0xAARRGGBB with straight alpha that the cursor plane is specified in -- so the buffer
 * goes to the adapter exactly as it comes out of the decoder.
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

    /* Both an upper bound on what the cursor plane can be asked to hold and a sanity check on
       the file: anything this large is not a pointer. */
    if (width <= 0 || height <= 0 || width > WM_CURSOR_MAX_SIZE || height > WM_CURSOR_MAX_SIZE) {
        fprintf(stderr, "aplus-wm: warning: %s is %dx%d, which is not a usable cursor size\n", path, width, height);
        free(file);
        return false;
    }


    /* The hotspots in the table describe the theme this server ships with. A replacement
       theme drawing the same shape smaller would put one outside its own image, which the
       cursor plane rejects outright, so it is pulled back inside. */
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


/* The arrow to fall back on when the theme has given us nothing to draw. It is a filled
   triangle with its tip at (x, y), outlined so that it stays visible over a light window. */
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


/* The rectangle the pointer occupies right now, which is what a software-drawn pointer has to
 * damage as it leaves one position and arrives at another.
 *
 * The drawn arrow needs a margin the theme images do not: its outline is stroked with a one
 * pixel pen centred on the path, so it reaches half a pixel outside the shape on every side.
 * Repainting only the arrow's own box left that half pixel behind, and the cursor drew a trail
 * across the screen as it moved.
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


/* The current shape, with its hotspot on (x, y). Only the software path draws the pointer at
   all; an adapter with a cursor plane is handed the image once per shape change instead. */
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


/* Hand the current shape to the cursor plane. Called on every shape change and once at start-up
   to decide whether there is a usable plane at all, which is why it reports failure. */
int wm_cursor_upload(void) {

    wm_cursor_t* cursor = &wm_cursors[wm_cursor_shape];

    if (!wm_cursor_load(cursor) || !cursor->image) {
        return -1;
    }

    return wm_display_cursor_image(&wm.display, cursor->image, cursor->width, cursor->height, cursor->hot_x, cursor->hot_y);
}


/* Switch the pointer to another shape.
 *
 * With a cursor plane this is the one place the image is re-sent; movement afterwards costs
 * nothing but a position. Without one the pointer is part of the frame, so both the box the old
 * shape occupied and the box the new one will occupy have to be repainted -- they are the same
 * position but not the same size, and a smaller shape would otherwise leave the tail of a larger
 * one behind it.
 */
void wm_cursor_set(wm_cursor_shape_t shape) {

    if (shape == wm_cursor_shape) {
        return;
    }

    /* A shape the theme has nothing for is not switched to at all, so the pointer keeps an
       image it does have rather than blinking to the drawn arrow and back. The arrow is the
       exception, being where every other shape returns to; if the theme has no image for that
       either then there is no plane in use, because the plane is only taken up once the arrow
       has loaded, and the drawn one goes into the frame instead. */
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
