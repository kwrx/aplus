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
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <aplus/hal.h>



/***
 * Name:        arch_prctl
 * Description: set architecture-specific thread state
 * URL:         http://man7.org/linux/man-pages/man2/arch_prctl.2.html
 *
 * Input Parameters:
 *  0: 0x9e
 *  1: int
 *  2: unsigned long
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(
    158, arch_prctl, long sys_arch_prctl(int code, unsigned long addr) {
        if (unlikely(!addr))
            return -EINVAL;


#if defined(__x86_64__)

    #define ARCH_SET_GS 0x1001
    #define ARCH_SET_FS 0x1002
    #define ARCH_GET_FS 0x1003
    #define ARCH_GET_GS 0x1004

        switch (code) {

            case ARCH_SET_GS:
                current_task->userspace.cpu_area = (uintptr_t)addr;
                break;

            case ARCH_SET_FS:
                current_task->userspace.thread_area = (uintptr_t)addr;
                break;

            case ARCH_GET_GS:
                *(uintptr_t *)addr = current_task->userspace.cpu_area;
                break;

            case ARCH_GET_FS:
                *(uintptr_t *)addr = current_task->userspace.thread_area;
                break;

            default:
                return -ENOSYS;
        }


        // Update stats
        arch_task_switch(current_task, current_task);

        return 0;

#endif

        return -ENOSYS;
    });
