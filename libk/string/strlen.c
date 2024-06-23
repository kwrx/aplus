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


size_t strlen(const char* src) {

    DEBUG_ASSERT(src);

    size_t len = 0;

    while (src[len] != '\0') {
        len++;
    }

    return len;
}


TEST(libk_strlen_test, {
    DEBUG_ASSERT(strlen("Hello World!") == 12);
    DEBUG_ASSERT(strlen("H") == 1);
});
