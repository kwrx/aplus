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
#include <stdlib.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>


static uint64_t seed;

void srand(unsigned s) {
    seed = s - 1;
}

int rand(void) {
    seed = 6364136223846793005ULL * seed + 1;
    return seed >> 33;
}


TEST(libk_rand_test, {
    int a = rand();
    int b = rand();

    DEBUG_ASSERT(a != b);
});
