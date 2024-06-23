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


int strcmp(const char* a, const char* b) {

    DEBUG_ASSERT(a);
    DEBUG_ASSERT(b);


    for (size_t i = 0; 1; i++) {

        unsigned char ac = (unsigned char)a[i];
        unsigned char bc = (unsigned char)b[i];

        if (ac == '\0' && bc == '\0')
            return 0;

        if (ac < bc)
            return -1;

        if (ac > bc)
            return 1;
    }


    __builtin_unreachable();
}


TEST(libk_strcmp_test, {
    char a[] = "Hello World!";
    char b[] = "Hello World!";

    DEBUG_ASSERT(strcmp(a, b) == 0);
});
