/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/task.h>



#if DEBUG_LEVEL_INFO

SYSCALL(
    404, systest, long sys_systest(long p0, long p1, long p2, long p3, long p4, long p5) {
        bool a = p0 == 0x01;
        bool b = p1 == 0x02;
        bool c = p2 == 0x03;
        bool d = p3 == 0x04;
        bool e = p4 == 0x05;
        bool f = p5 == 0x06;

        if (a && b && c && d && e && f)
            return 0xDEADBEEF;

        return -EINVAL;
    });

#endif
