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


#ifndef _CAIRO_WEBP_EXT_H
#define _CAIRO_WEBP_EXT_H

#define CAIRO_HAS_WEBP_FUNCTIONS    1

#ifdef __cplusplus
extern "C" {
#endif


cairo_surface_t* cairo_image_surface_create_from_webp(const char* filename);
cairo_surface_t* cairo_image_surface_create_from_webp_stream(cairo_read_func_t read, void* arg);    

#ifdef __cplusplus
}
#endif

#endif