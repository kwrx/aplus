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


void* memset(void* dest, int c, size_t n) {

    DEBUG_ASSERT(dest);

    uintptr_t d = (uintptr_t)dest;

    uint64_t value = (uint8_t)c;
    value |= value << 8;
    value |= value << 16;
    value |= value << 32;

    size_t i = 0;

    for (; i + 64 < n; i += 64) {

        *((uint64_t*)(d + 0))  = value;
        *((uint64_t*)(d + 8))  = value;
        *((uint64_t*)(d + 16)) = value;
        *((uint64_t*)(d + 24)) = value;
        *((uint64_t*)(d + 32)) = value;
        *((uint64_t*)(d + 40)) = value;
        *((uint64_t*)(d + 48)) = value;
        *((uint64_t*)(d + 56)) = value;

        d += 64;
    }

    for (; i + 32 < n; i += 32) {

        *((uint64_t*)(d + 0))  = value;
        *((uint64_t*)(d + 8))  = value;
        *((uint64_t*)(d + 16)) = value;
        *((uint64_t*)(d + 24)) = value;

        d += 32;
    }

    for (; i + 16 < n; i += 16) {

        *((uint64_t*)(d + 0)) = value;
        *((uint64_t*)(d + 8)) = value;

        d += 16;
    }

    for (; i + 8 < n; i += 8) {

        *((uint64_t*)(d)) = value;

        d += 8;
    }

    for (; i + 4 < n; i += 4) {

        *((uint32_t*)(d)) = (uint32_t)value;

        d += 4;
    }

    for (; i + 2 < n; i += 2) {

        *((uint16_t*)(d)) = (uint16_t)value;

        d += 2;
    }

    for (; i < n; i += 1) {

        *((uint8_t*)(d)) = c;

        d += 1;
    }

    return dest;
}


TEST(libk_memset_test, {
    char a[] = "Hello World!";
    char b[] = "Hello World!";
    uint8_t c[128];

    memset(a, 'A', 5);
    memset(b, 'A', 5);
    memset(c, 0xA5, sizeof(c));

    DEBUG_ASSERT(strcmp(a, b) == 0);

    for (size_t i = 0; i < sizeof(c); i++)
        DEBUG_ASSERT(c[i] == 0xA5);
});
