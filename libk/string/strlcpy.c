/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2026 Antonino Natale
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


size_t strlcpy(char* restrict dest, const char* restrict src, size_t n) {

    DEBUG_ASSERT(dest);
    DEBUG_ASSERT(src);

    size_t len = strlen(src);

    if (n > 0) {

        size_t k = len < n - 1 ? len : n - 1;

        memcpy(dest, src, k);
        dest[k] = '\0';
    }

    return len;
}


TEST(libk_strlcpy_test, {
    char a[16] = {0};
    char b[16] = {0};
    char c[16] = "untouched";

    DEBUG_ASSERT(strlcpy(a, "Hello World!", sizeof(a)) == 12);
    DEBUG_ASSERT(strlcpy(b, "Hello World!", 6) == 12);
    DEBUG_ASSERT(strlcpy(c, "Hello World!", 0) == 12);

    DEBUG_ASSERT(strcmp(a, "Hello World!") == 0);
    DEBUG_ASSERT(strcmp(b, "Hello") == 0);
    DEBUG_ASSERT(strcmp(c, "untouched") == 0);
});
