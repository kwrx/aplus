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

#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/task.h>



/***
 * Name:        set_tid_address
 * Description: set pointer to thread ID
 * URL:         http://man7.org/linux/man-pages/man2/set_tid_address.2.html
 *
 * Input Parameters:
 *  0: 0xda
 *  1: int  * tidptr
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(
    218, set_tid_address, long sys_set_tid_address(int *tidptr) {
        DEBUG_ASSERT(tidptr);
        DEBUG_ASSERT(uio_check(tidptr, R_OK | W_OK));

        current_task->userspace.tid_address = (uintptr_t)uio_get_ptr(tidptr);

        return current_task->tid;
    });
