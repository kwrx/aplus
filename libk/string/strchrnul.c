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


char* strchrnul(const char* str, int uc) {

    DEBUG_ASSERT(str);

    const unsigned char* ustr = (const unsigned char*)str;

    for (size_t i = 0; 1; i++)
        if (ustr[i] == (unsigned char)uc || !ustr[i])
            return (char*)str + i;


    __builtin_unreachable();
}


TEST(libk_strchrnul_test, {
    char a[] = "Hello World!";
    char b[] = "Hello World!";

    DEBUG_ASSERT(strchrnul(a, 'o') - a == strchrnul(b, 'o') - b);
    DEBUG_ASSERT(strchrnul(a, 'o') - a == 4);

    DEBUG_ASSERT(strchrnul(a, 'z') - a == strchrnul(b, 'z') - b);
    DEBUG_ASSERT(strchrnul(a, 'z') - a == 12);

    DEBUG_ASSERT(strchrnul(a, '!') - a == strchrnul(b, '!') - b);
    DEBUG_ASSERT(strchrnul(a, '!') - a == 11);

    DEBUG_ASSERT(strchrnul(a, '\0') - a == strchrnul(b, '\0') - b);
    DEBUG_ASSERT(strchrnul(a, '\0') - a == 12);
});
