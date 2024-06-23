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


int atoi(const char* s) {

    DEBUG_ASSERT(s);

    int val  = 0;
    bool neg = 0;

    while ((*s) == ' ' || (*s) == '\t')
        s++;

    switch (*s) {
        case '-':
            neg = true;
            s++;
            break;
        case '+':
            s++;
            break;
    }

    while ((*s) >= '0' && (*s) <= '9') {
        val = 10 * val - (*s++ - '0');
    }

    return neg ? val : -val;
}


TEST(libk_atoi_test, {
    DEBUG_ASSERT(atoi("0") == 0);
    DEBUG_ASSERT(atoi("1") == 1);
    DEBUG_ASSERT(atoi("2") == 2);
    DEBUG_ASSERT(atoi("33993") == 33993);
    DEBUG_ASSERT(atoi("2147483647") == 2147483647);
    DEBUG_ASSERT(atoi("-2147483648") == -2147483648);
    DEBUG_ASSERT(atoi("-1") == -1);
    DEBUG_ASSERT(atoi("ss") == 0);
});
