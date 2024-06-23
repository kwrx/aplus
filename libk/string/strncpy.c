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
#include <string.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>


char* strncpy(char* restrict dest, const char* restrict src, size_t n) {

    DEBUG_ASSERT(dest);
    DEBUG_ASSERT(src);
    DEBUG_ASSERT(n > 0);

    if (n != 0) {

        register char* d       = dest;
        register const char* s = src;

        do {

            if ((*d++ = *s++) == 0) {

                while (--n != 0) {
                    *d++ = 0;
                }

                break;
            }

        } while (--n != 0);
    }

    return (dest);
}


TEST(libk_strncpy_test, {
    char buf[32];
    memset(buf, 0, 32);

    DEBUG_ASSERT(strncpy(buf, "Hello World!", 12) == buf);
    DEBUG_ASSERT(strncmp(buf, "Hello World!", 12) == 0);

    DEBUG_ASSERT(strncpy(buf, "Hello World!", 5) == buf);
    DEBUG_ASSERT(strncmp(buf, "Hello", 5) == 0);

    DEBUG_ASSERT(strncpy(buf, "Hello World!", 32) == buf);
    DEBUG_ASSERT(strncmp(buf, "Hello World!", 32) == 0);
});
