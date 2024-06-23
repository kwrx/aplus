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
#include <aplus/syscall.h>
#include <stdint.h>


// @see kernel/init/hostname.c
extern char* hostname;


/***
 * Name:        sethostname
 * Description: get/set hostname
 * URL:         http://man7.org/linux/man-pages/man2/sethostname.2.html
 *
 * Input Parameters:
 *  0: 0xaa
 *  1: char  * name
 *  2: int len
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(
    170, sethostname, long sys_sethostname(char* name, int len) {
        if (unlikely(!name))
            return -EFAULT;

        if (unlikely(!uio_check(name, R_OK)))
            return -EFAULT;

        if (unlikely(len < 0))
            return -EINVAL;

        if (unlikely(len > CONFIG_NAME_MAX))
            return -EINVAL;


        uio_memcpy_u2s(hostname, name, len);

        hostname[len] = '\0';

        return 0;
    });
