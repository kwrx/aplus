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
 * @brief The cairo odds and ends that are not any one module's.
 *
 * The rounded rectangle and the font come from libui, which the widgets are drawn with too.
 */

#include <math.h>
#include <string.h>

#include <wm.h>


#define WM_OUTLINE_CACHE 4


/**
 * @brief Creates an image surface, reporting a failure as NULL rather than as an errored surface.
 *
 * @param format The pixel format.
 * @param width The width in pixels.
 * @param height The height in pixels.
 * @return The surface, or NULL.
 */
cairo_surface_t* wm_surface_create(cairo_format_t format, int width, int height) {

    cairo_surface_t* surface = cairo_image_surface_create(format, width, height);

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return NULL;
    }

    return surface;
}


/**
 * @brief Replaces with '?' every byte that is not part of a well-formed UTF-8 sequence, and drops one cut short at the end,
 * since cairo refuses a whole string over either and then stops drawing anything at all.
 *
 * @param text The string, changed in place.
 */
void wm_utf8_sanitize(char* text) {

    unsigned char* s = (unsigned char*)text;

    while (*s) {

        const unsigned char c = s[0];

        const size_t length = c < 0x80 ? 1 : c >= 0xC2 && c <= 0xDF ? 2 : c >= 0xE0 && c <= 0xEF ? 3 : c >= 0xF0 && c <= 0xF4 ? 4 : 0;

        const unsigned char lo = c == 0xE0 ? 0xA0 : c == 0xF0 ? 0x90 : 0x80;
        const unsigned char hi = c == 0xED ? 0x9F : c == 0xF4 ? 0x8F : 0xBF;

        bool valid = length > 0;
        size_t i   = 1;

        for (; valid && i < length; i++) {
            valid = i == 1 ? s[i] >= lo && s[i] <= hi : s[i] >= 0x80 && s[i] <= 0xBF;
        }

        if (!valid && length > 0 && s[i - 1] == '\0') {
            *s = '\0';
            break;
        }

        if (!valid) {
            *s++ = '?';
            continue;
        }

        s += length;
    }
}


/**
 * @brief Renders a theme icon at a size into a surface of its own, falling back to WM_ICON_FALLBACK.
 *
 * The copy is the caller's: libui's cache destroys its own surfaces once other icons push them out.
 *
 * @param name A name in the icon theme; a path, a name the theme lacks, or NULL gets the fallback.
 * @param size The width and height in pixels.
 * @return The surface, or NULL when not even the fallback can be loaded.
 */
cairo_surface_t* wm_icon_create(const char* name, int size) {

    cairo_surface_t* icon = wm_icon_find(name, size);

    return icon ? icon : wm_icon_find(WM_ICON_FALLBACK, size);
}


/**
 * @brief Renders a theme icon at a size into a surface of its own, with nothing to fall back on.
 *
 * @param name A name in the icon theme.
 * @param size The width and height in pixels.
 * @return The surface, or NULL for a path, a name the theme lacks, or NULL.
 */
cairo_surface_t* wm_icon_find(const char* name, int size) {

    cairo_surface_t* source = name && *name && !strchr(name, '/') ? ui_icon_load(name, size) : NULL;

    if (!source) {
        return NULL;
    }


    cairo_surface_t* icon = wm_surface_create(CAIRO_FORMAT_ARGB32, size, size);

    if (!icon) {
        return NULL;
    }


    cairo_t* cr = cairo_create(icon);

    ui_draw_icon(cr, (ui_rect_t){0, 0, size, size}, source);

    cairo_destroy(cr);

    return icon;
}


/**
 * @brief Sets a theme colour as the source, its alpha scaled by another.
 *
 * @param cr The cairo context.
 * @param color The colour.
 * @param alpha How much of the colour's own alpha to keep, 0 to 1.
 */
void wm_source(cairo_t* cr, ui_color_t color, double alpha) {

    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a * alpha);
}


/**
 * @brief Sets as the source a colour partway between two, alpha included.
 *
 * @param cr The cairo context.
 * @param a The colour at 0.
 * @param b The colour at 1.
 * @param t How far towards b, 0 to 1.
 */
void wm_source_mix(cairo_t* cr, ui_color_t a, ui_color_t b, double t) {

    cairo_set_source_rgba(cr, ui_lerp(a.r, b.r, t), ui_lerp(a.g, b.g, t), ui_lerp(a.b, b.b, t), ui_lerp(a.a, b.a, t));
}


typedef struct {

    double radius;
    int corner;
    cairo_surface_t* mask;
    cairo_pattern_t* pattern;

} wm_outline_t;


static wm_outline_t wm_outlines[WM_OUTLINE_CACHE];
static size_t wm_outlines_next;


bool wm_pixel_exact(cairo_t* cr) {

    cairo_matrix_t m;

    cairo_get_matrix(cr, &m);

    return m.xx == 1.0 && m.yy == 1.0 && m.xy == 0.0 && m.yx == 0.0 && m.x0 == floor(m.x0) && m.y0 == floor(m.y0);
}


static const wm_outline_t* wm_outline_get(double radius) {

    for (size_t i = 0; i < WM_OUTLINE_CACHE; i++) {

        if (wm_outlines[i].mask && wm_outlines[i].radius == radius) {
            return &wm_outlines[i];
        }
    }


    const int corner = (int)ceil(radius + 0.5) + 1;
    const int size   = 2 * corner + 1;

    cairo_surface_t* mask = wm_surface_create(CAIRO_FORMAT_A8, size, size);

    if (!mask) {
        return NULL;
    }

    cairo_t* cr = cairo_create(mask);

    cairo_set_line_width(cr, 1.0);
    ui_draw_rounded_rect_d(cr, 0.5, 0.5, size - 1.0, size - 1.0, radius);
    cairo_stroke(cr);

    cairo_destroy(cr);


    wm_outline_t* slot = &wm_outlines[wm_outlines_next++ % WM_OUTLINE_CACHE];

    if (slot->mask) {
        cairo_pattern_destroy(slot->pattern);
        cairo_surface_destroy(slot->mask);
    }

    slot->radius  = radius;
    slot->corner  = corner;
    slot->mask    = mask;
    slot->pattern = cairo_pattern_create_for_surface(mask);

    return slot;
}


void wm_stroke_frame(cairo_t* cr, int x, int y, int width, int height, double radius) {

    const wm_outline_t* outline = wm_pixel_exact(cr) ? wm_outline_get(radius) : NULL;

    if (!outline || width < 2 * outline->corner + 1 || height < 2 * outline->corner + 1) {
        cairo_set_line_width(cr, 1.0);
        ui_draw_rounded_rect_d(cr, x + 0.5, y + 0.5, width - 1.0, height - 1.0, radius);
        cairo_stroke(cr);
        return;
    }


    const int corner = outline->corner;
    const int size   = 2 * corner + 1;

    for (int i = 0; i < 4; i++) {

        const int right = i & 1;
        const int lower = i >> 1;

        cairo_matrix_t matrix;

        cairo_matrix_init_translate(&matrix, -(right ? x + width - size : x), -(lower ? y + height - size : y));
        cairo_pattern_set_matrix(outline->pattern, &matrix);

        cairo_save(cr);

        cairo_rectangle(cr, right ? x + width - corner : x, lower ? y + height - corner : y, corner, corner);
        cairo_clip(cr);

        cairo_mask(cr, outline->pattern);

        cairo_restore(cr);
    }

    cairo_rectangle(cr, x + corner, y, width - 2 * corner, 1);
    cairo_rectangle(cr, x + corner, y + height - 1, width - 2 * corner, 1);
    cairo_rectangle(cr, x, y + corner, 1, height - 2 * corner);
    cairo_rectangle(cr, x + width - 1, y + corner, 1, height - 2 * corner);
    cairo_fill(cr);
}


void wm_stroke_fini(void) {

    for (size_t i = 0; i < WM_OUTLINE_CACHE; i++) {

        if (wm_outlines[i].mask) {
            cairo_pattern_destroy(wm_outlines[i].pattern);
            cairo_surface_destroy(wm_outlines[i].mask);
            wm_outlines[i].mask = NULL;
        }
    }
}
