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

#include <string.h>

#include <wm.h>


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
 * @brief Renders a theme icon at a size into a surface of its own, falling back to WM_ICON_FALLBACK.
 *
 * The copy is the caller's: libui's cache destroys its own surfaces once other icons push them out.
 *
 * @param name A name in the icon theme; a path, a name the theme lacks, or NULL gets the fallback.
 * @param size The width and height in pixels.
 * @return The surface, or NULL when not even the fallback can be loaded.
 */
cairo_surface_t* wm_icon_create(const char* name, int size) {

    cairo_surface_t* source = NULL;

    if (name && *name && !strchr(name, '/')) {
        source = ui_icon_load(name, size);
    }

    if (!source) {
        source = ui_icon_load(WM_ICON_FALLBACK, size);
    }

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
