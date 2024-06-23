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
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/task.h>


/***
 * Name:        sched_getaffinity
 * Description: set  and get a thread's CPU
       affinity mask
 * URL:         http://man7.org/linux/man-pages/man2/sched_getaffinity.2.html
 *
 * Input Parameters:
 *  0: 0xcc
 *  1: pid_t pid
 *  2: unsigned int len
 *  3: unsigned long  * user_mask_ptr
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(
    204, sched_getaffinity, long sys_sched_getaffinity(pid_t pid, unsigned int len, unsigned long* user_mask_ptr) {
        if (unlikely(len != CPU_SETSIZE))
            return -EINVAL;

        if (unlikely(!user_mask_ptr))
            return -EINVAL;

        if (unlikely(!uio_check(user_mask_ptr, R_OK | W_OK)))
            return -EFAULT;

        if (unlikely(pid < 0))
            return -EINVAL;


        if (pid == 0) {
            pid = current_task->tid;
        }


        cpu_set_t __safe_mask_ptr;

        cpu_foreach(cpu) {

            for (task_t* tmp = cpu->sched_queue; tmp; tmp = tmp->next) {

                if (tmp->tid != pid)
                    continue;

                if (!(tmp->euid == current_task->euid || tmp->euid == current_task->uid))
                    return -EPERM;


                uio_memcpy_u2s(&__safe_mask_ptr, user_mask_ptr, sizeof(cpu_set_t));

                CPU_ZERO(&__safe_mask_ptr);
                CPU_OR(&__safe_mask_ptr, &__safe_mask_ptr, &tmp->affinity);

                uio_memcpy_s2u(user_mask_ptr, &__safe_mask_ptr, sizeof(cpu_set_t));

                return CPU_SETSIZE;
            }
        }


        return -ESRCH;
    });
