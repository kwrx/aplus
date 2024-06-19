/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2022 Antonino Natale
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

#ifndef _ATERM_BUILTIN_FONT_H
#define _ATERM_BUILTIN_FONT_H


#if defined(CONFIG_ATERM_BUILTIN_FONT)

#if defined(CONFIG_ATERM_BUILTIN_FONT_6x10)
    #define ATERM_FONT_WIDTH    6
    #define ATERM_FONT_HEIGHT   10
    #define ATERM_FONT_PITCH    10
    #include "font_6x10.c.in"
#elif defined(CONFIG_ATERM_BUILTIN_FONT_6x11)
    #define ATERM_FONT_WIDTH    6
    #define ATERM_FONT_HEIGHT   11
    #define ATERM_FONT_PITCH    11
    #include "font_6x11.c.in"
#elif defined(CONFIG_ATERM_BUILTIN_FONT_7x14)
    #define ATERM_FONT_WIDTH    7
    #define ATERM_FONT_HEIGHT   14
    #define ATERM_FONT_PITCH    14
    #include "font_7x14.c.in"
#elif defined(CONFIG_ATERM_BUILTIN_FONT_8x8)
    #define ATERM_FONT_WIDTH    8
    #define ATERM_FONT_HEIGHT   8
    #define ATERM_FONT_PITCH    8
    #include "font_8x8.c.in"
#elif defined(CONFIG_ATERM_BUILTIN_FONT_8x16)
    #define ATERM_FONT_WIDTH    8
    #define ATERM_FONT_HEIGHT   16
    #define ATERM_FONT_PITCH    16
    #include "font_8x16.c.in"
#elif defined(CONFIG_ATERM_BUILTIN_FONT_acorn_8x8)
    #define ATERM_FONT_WIDTH    8
    #define ATERM_FONT_HEIGHT   8
    #define ATERM_FONT_PITCH    8
    #include "font_acorn_8x8.c.in"
#elif defined(CONFIG_ATERM_BUILTIN_FONT_mini_4x6)
    #define ATERM_FONT_WIDTH    4
    #define ATERM_FONT_HEIGHT   6
    #define ATERM_FONT_PITCH    6
    #include "font_mini_4x6.c.in"
#elif defined(CONFIG_ATERM_BUILTIN_FONT_pearl_8x8)
    #define ATERM_FONT_WIDTH    8
    #define ATERM_FONT_HEIGHT   8
    #define ATERM_FONT_PITCH    8
    #include "font_pearl_8x8.c.in"
#elif defined(CONFIG_ATERM_BUILTIN_FONT_sun_8x16)
    #define ATERM_FONT_WIDTH    8
    #define ATERM_FONT_HEIGHT   16
    #define ATERM_FONT_PITCH    16
    #include "font_sun8x16.c.in"
#else
    #define ATERM_FONT_WIDTH    8
    #define ATERM_FONT_HEIGHT   16
    #define ATERM_FONT_PITCH    16
    #include "font_8x16.c.in"
#endif

#endif

#endif
