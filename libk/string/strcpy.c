/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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

#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>


char* strcpy(char* dest, const char* src) {

    DEBUG_ASSERT(dest);
    DEBUG_ASSERT(src);

    char* p = dest;
    while (*src)
        *dest++ = *src++;

    *dest = '\0';
    return p;
}


TEST(libk_strcpy_test, {
    char a[] = "Hello World!";
    char b[] = "Hello World!";

    strcpy(a, "Hello World!");
    strcpy(b, "Hello World!");

    DEBUG_ASSERT(strcmp(a, b) == 0);
});
