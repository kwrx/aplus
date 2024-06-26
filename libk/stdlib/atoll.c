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
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>


long long atoll(const char* s) {

    DEBUG_ASSERT(s);

    long long val = 0;
    bool neg      = 0;

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


TEST(libk_atoll_test, {
    DEBUG_ASSERT(atoll("0") == 0);
    DEBUG_ASSERT(atoll("1") == 1);
    DEBUG_ASSERT(atoll("10") == 10);
    DEBUG_ASSERT(atoll("100") == 100);
    DEBUG_ASSERT(atoll("4294967295") == 4294967295LL);
    DEBUG_ASSERT(atoll("-4294967295") == -4294967295LL);
    DEBUG_ASSERT(atoll("abc") == 0);
});
