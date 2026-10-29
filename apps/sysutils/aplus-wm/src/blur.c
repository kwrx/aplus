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
 * @brief Frosted glass: what the frame being composited has already drawn under something, read back from the back buffer
 * at half size and blurred, for that something to be painted over.
 */

#include <stdlib.h>
#include <string.h>

#include <wm.h>


/**
 * @brief How many frosted things damage is widened for at once: the menu and the windows.
 */
#define WM_BLUR_AREAS_MAX 65


/**
 * @brief Reports what frosting a shape reads back: the shape and as far around it as the blur reaches, inside the display.
 *
 * @param shape The shape.
 * @return The rectangle.
 */
static wm_rect_t wm_blur_reach(const wm_rect_t* shape) {

    const wm_rect_t r = {shape->x - WM_BLUR_RADIUS, shape->y - WM_BLUR_RADIUS, shape->width + 2 * WM_BLUR_RADIUS, shape->height + 2 * WM_BLUR_RADIUS};

    return wm_rect_clip(&r, wm.display.width, wm.display.height);
}


/**
 * @brief Collects the shapes frosted this frame: a client's menu and every frosted window.
 *
 * @param areas Receives what each of them reads back.
 * @param max How many fit.
 * @return How many there are.
 */
static size_t wm_blur_areas(wm_rect_t* areas, size_t max) {

    size_t count = 0;

    wm_rect_t shape;

    if (count < max && wm_menu_frosted(&shape)) {
        areas[count++] = wm_blur_reach(&shape);
    }

    for (wm_window_t* win = wm.windows; win && count < max; win = win->next) {

        if (wm_window_frosted(win)) {

            shape = wm_window_frame(win);

            areas[count++] = wm_blur_reach(&shape);
        }
    }

    return count;
}


/**
 * @brief Widens damage that reaches under anything frosted to all that it reads back, until no more is drawn in, so that
 * no backdrop read back holds what the last frame painted over it.
 */
void wm_blur_damage(void) {

    wm_rect_t areas[WM_BLUR_AREAS_MAX];
    bool widened[WM_BLUR_AREAS_MAX] = {false};

    const size_t count = wm_blur_areas(areas, WM_BLUR_AREAS_MAX);

    for (bool changed = true; changed;) {

        changed = false;

        for (size_t i = 0; i < count; i++) {

            for (size_t j = 0; j < wm.damage.count && !widened[i]; j++) {

                if (wm_rect_intersects(&wm.damage.rects[j], &areas[i])) {

                    wm_damage(&areas[i]);

                    widened[i] = true;
                    changed    = true;
                }
            }
        }
    }
}


/**
 * @brief Runs one box blur along a line of RGB24 pixels, in place, its ends held at the first and last pixel.
 *
 * @param pixels The first pixel.
 * @param step How many pixels apart two neighbours on the line are.
 * @param length How many pixels the line has.
 * @param radius How far either side of a pixel the box reaches.
 * @param line A scratch line of at least `length` pixels.
 */
static void wm_blur_line(uint32_t* pixels, size_t step, int length, int radius, uint32_t* line) {

    const int span = 2 * radius + 1;

    for (int i = 0; i < length; i++) {
        line[i] = pixels[i * step];
    }

    int r = 0;
    int g = 0;
    int b = 0;

    for (int i = -radius; i <= radius; i++) {

        const uint32_t p = line[WM_CLAMP(i, 0, length - 1)];

        r += (int)((p >> 16) & 0xFF);
        g += (int)((p >> 8) & 0xFF);
        b += (int)(p & 0xFF);
    }

    for (int x = 0; x < length; x++) {

        pixels[x * step] = 0xFF000000u | (uint32_t)(r / span) << 16 | (uint32_t)(g / span) << 8 | (uint32_t)(b / span);

        const uint32_t in  = line[WM_MIN(x + radius + 1, length - 1)];
        const uint32_t out = line[WM_MAX(x - radius, 0)];

        r += (int)((in >> 16) & 0xFF) - (int)((out >> 16) & 0xFF);
        g += (int)((in >> 8) & 0xFF) - (int)((out >> 8) & 0xFF);
        b += (int)(in & 0xFF) - (int)(out & 0xFF);
    }
}


/**
 * @brief Reads back what this frame has drawn under a shape, at half size, and blurs it.
 *
 * @param blur Receives the blurred picture and where it goes, keeping its surface from one frame to the next.
 * @param shape The shape to be frosted.
 * @return false when there is nothing to read, or no memory to blur it in.
 */
bool wm_blur_capture(wm_blur_t* blur, const wm_rect_t* shape) {

    const wm_rect_t r = wm_blur_reach(shape);

    const int width  = (r.width + 1) / 2;
    const int height = (r.height + 1) / 2;

    if (width <= 0 || height <= 0) {
        return false;
    }

    if (!blur->surface || cairo_image_surface_get_width(blur->surface) != width || cairo_image_surface_get_height(blur->surface) != height) {

        wm_blur_fini(blur);

        if (!(blur->surface = wm_surface_create(CAIRO_FORMAT_RGB24, width, height))) {
            return false;
        }
    }

    uint32_t* line = malloc(sizeof(uint32_t) * (size_t)WM_MAX(width, height));

    if (!line) {
        return false;
    }

    blur->rect = r;

    cairo_surface_flush(wm.display.back);

    cairo_t* cr = cairo_create(blur->surface);

    cairo_scale(cr, 0.5, 0.5);
    cairo_set_source_surface(cr, wm.display.back, -r.x, -r.y);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_destroy(cr);

    cairo_surface_flush(blur->surface);

    uint32_t* data     = (uint32_t*)cairo_image_surface_get_data(blur->surface);
    const size_t pitch = (size_t)cairo_image_surface_get_stride(blur->surface) / sizeof(uint32_t);
    const int box      = WM_MAX(WM_BLUR_RADIUS / 2 / WM_BLUR_PASSES, 1);

    for (int pass = 0; pass < WM_BLUR_PASSES; pass++) {

        for (int y = 0; y < height; y++) {
            wm_blur_line(data + y * pitch, 1, width, box, line);
        }

        for (int x = 0; x < width; x++) {
            wm_blur_line(data + x, pitch, height, box, line);
        }
    }

    free(line);

    cairo_surface_mark_dirty(blur->surface);

    return true;
}


/**
 * @brief Paints a blurred backdrop back where it was read from, inside the current clip.
 *
 * @param cr The cairo context, clipped to the shape being frosted.
 * @param blur The backdrop, from wm_blur_capture().
 */
void wm_blur_paint(cairo_t* cr, const wm_blur_t* blur) {

    if (!blur->surface) {
        return;
    }

    cairo_save(cr);

    cairo_translate(cr, blur->rect.x, blur->rect.y);
    cairo_scale(cr, 2.0, 2.0);
    cairo_set_source_surface(cr, blur->surface, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
    cairo_paint(cr);

    cairo_restore(cr);
}


/**
 * @brief Lets go of a backdrop's picture.
 *
 * @param blur The backdrop.
 */
void wm_blur_fini(wm_blur_t* blur) {

    if (blur->surface) {
        cairo_surface_destroy(blur->surface);
        blur->surface = NULL;
    }
}
