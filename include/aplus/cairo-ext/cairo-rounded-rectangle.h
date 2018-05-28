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


#ifndef _CAIRO_ROUNDED_RECTANGLE_H
#define _CAIRO_ROUNDED_RECTANGLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cairo/cairo.h>




/* void cairo_rounded_rectangle(cr, x, y, w, h, r)
    cairo_t* cr,
    double x,
    double y,
    double w,
    double h,
    double r,   // Radius
*/

#define cairo_rounded_rectangle(cr, x, y, w, h, r) {                        \
    cairo_new_sub_path(cr);                                                 \
    cairo_arc(cr, x + w - r, y + r, r, -(M_PI / 2.0), 0);                   \
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2.0);                  \
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2.0, M_PI);                   \
    cairo_arc(cr, x + r, y + r, r, M_PI, M_PI * 1.5);                       \
    cairo_close_path(cr);                                                   \
}
    
#ifdef __cplusplus
}
#endif
#endif