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

#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>


char* strcat(char* restrict dest, const char* restrict src) {

    DEBUG_ASSERT(dest);
    DEBUG_ASSERT(src);

    strcpy(dest + strlen(dest), src);

    return dest;
}


TEST(libk_strcat_test, {
    char a[32] = "Hello World!";
    char b[32] = "Hello World!";

    strcat(a, " Hello World!");
    strcat(b, " Hello World!");

    DEBUG_ASSERT(strcmp(a, b) == 0);
});
