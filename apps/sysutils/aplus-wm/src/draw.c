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
