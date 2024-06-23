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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


/***
 * Name:        getrandom
 * Description: obtain a series of random bytes
 * URL:         http://man7.org/linux/man-pages/man2/getrandom.2.html
 *
 * Input Parameters:
 *  0: 0x13e
 *  1: char  * buf
 *  2: size_t count
 *  3: unsigned int flags
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(
    318, getrandom, long sys_getrandom(char* buf, size_t count, unsigned int flags) {
        if (unlikely(!buf))
            return -EINVAL;

        if (unlikely(!uio_check(buf, R_OK | W_OK)))
            return -EFAULT;

        if (unlikely(count == 0))
            return 0;


        size_t i = 0;

        for (; i + 8 < count; i += 8) {
            uio_w64((uintptr_t)buf + i, arch_random() & 0xFFFFFFFFFFFFFFFF);
        }

        for (; i + 4 < count; i += 4) {
            uio_w32((uintptr_t)buf + i, arch_random() & 0xFFFFFFFF);
        }

        for (; i + 2 < count; i += 2) {
            uio_w16((uintptr_t)buf + i, arch_random() & 0xFFFF);
        }

        for (; i < count; i += 1) {
            uio_w8((uintptr_t)buf + i, arch_random() & 0xFF);
        }

        return count;
    });
