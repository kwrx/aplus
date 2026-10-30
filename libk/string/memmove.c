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

#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>


void* memmove(void* dest, const void* src, size_t n) {

    DEBUG_ASSERT(dest);
    DEBUG_ASSERT(src);
    DEBUG_ASSERT(n > 0);

    uintptr_t d = (uintptr_t)dest;
    uintptr_t s = (uintptr_t)src;


    if (d < s) {

        size_t i = 0;

        for (; i + 8 <= n; i += 8)
            *((uint64_t*)(d + i)) = *((uint64_t*)(s + i));

        for (; i < n; i++)
            *((uint8_t*)(d + i)) = *((uint8_t*)(s + i));

    } else {

        size_t i = n;

        for (; i >= 8; i -= 8)
            *((uint64_t*)(d + i - 8)) = *((uint64_t*)(s + i - 8));

        for (; i > 0; i--)
            *((uint8_t*)(d + i - 1)) = *((uint8_t*)(s + i - 1));
    }

    return dest;
}


TEST(libk_memmove_test, {
    char a[] = "Hello World!";
    char b[13];

    DEBUG_ASSERT(memmove(b, a, sizeof(a)) == b);
    DEBUG_ASSERT(memcmp(a, b, sizeof(a)) == 0);
});
