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

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>


void* memchr(const void* src, int c, size_t n) {

    DEBUG_ASSERT(src);
    DEBUG_ASSERT(n > 0);

    for (size_t i = 0; i < n; i++) {

        if (((const char*)src)[i] == (char)c)
            return (void*)&((char*)src)[i];
    }

    return NULL;
}


TEST(libk_memchr_test, {
    char* s = "Hello World!";

    DEBUG_ASSERT(memchr(s, 'H', 12) == s);
    DEBUG_ASSERT(memchr(s, 'e', 12) == s + 1);
    DEBUG_ASSERT(memchr(s, 'l', 12) == s + 2);
    DEBUG_ASSERT(memchr(s, 'o', 12) == s + 4);
    DEBUG_ASSERT(memchr(s, 'W', 12) == s + 6);
    DEBUG_ASSERT(memchr(s, 'r', 12) == s + 8);
    DEBUG_ASSERT(memchr(s, 'd', 12) == s + 10);
    DEBUG_ASSERT(memchr(s, '!', 12) == s + 11);
    DEBUG_ASSERT(memchr(s, 'x', 12) == NULL);
});
