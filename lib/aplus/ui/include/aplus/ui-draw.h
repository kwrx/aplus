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

#ifndef _APLUS_UI_DRAW_H
#define _APLUS_UI_DRAW_H

#include <cairo/cairo.h>

#include <aplus/ui.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief The primitives that name a cairo type, which ui.h and ui-widgets.h deliberately do not.
 *
 * An app drawing its own surface reaches for these; the widgets are built on the same ones.
 */


/**
 * @brief Lays out a rectangle with all four corners rounded, as a path.
 *
 * @param cr The cairo context to build the path in.
 * @param rect The rectangle.
 * @param radius The corner radius, clamped to half the shorter side.
 */
void ui_draw_rounded_rect(cairo_t* cr, ui_rect_t rect, double radius);

/**
 * @brief The same path in device units, for the half-pixel insets a centred stroke needs.
 *
 * @param cr The cairo context to build the path in.
 * @param x The left edge.
 * @param y The top edge.
 * @param w The width of the rectangle.
 * @param h The height of the rectangle.
 * @param radius The corner radius, clamped to half the shorter side.
 */
void ui_draw_rounded_rect_d(cairo_t* cr, double x, double y, double w, double h, double radius);

/**
 * @brief Loads a font face from a file, out of a cache that keeps it for the life of the process.
 *
 * @param path The font file to load.
 * @return The face, or NULL when the file cannot be loaded, in which case the caller draws no text.
 */
cairo_font_face_t* ui_font_face(const char* path);

/**
 * @brief Reports a cached scaled font for a face at a size, building it the first time it is asked for.
 *
 * @param face The face to scale.
 * @param size The size in pixels.
 * @return The scaled font, owned by the cache, or NULL.
 */
cairo_scaled_font_t* ui_font_scaled(cairo_font_face_t* face, double size);


#ifdef __cplusplus
}
#endif
#endif
