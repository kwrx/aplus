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


#ifndef _CAIRO_EXT_FC_H
#define _CAIRO_EXT_FC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>

#define CAIRO_FONT_WEIGHT_LIGHT         ((cairo_font_weight_t) 128)
#define CAIRO_FONT_WEIGHT_MEDIUM        ((cairo_font_weight_t) 129)

int cairo_fc_load(const char*);
cairo_font_face_t* cairo_fc_font_face_create(const char*, cairo_font_slant_t, cairo_font_weight_t);
void cairo_fc_select_font_face(cairo_t*, const char*, cairo_font_slant_t, cairo_font_weight_t);
    
#ifdef __cplusplus
}
#endif
#endif