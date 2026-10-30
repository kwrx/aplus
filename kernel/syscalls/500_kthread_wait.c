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
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/syscall.h>
#include <aplus/task.h>

SYSCALL(
    SYSCALL_NR_KTHREAD_WAIT, kthread_wait, long sys_kthread_wait(uint32_t* s, uint32_t value, uint32_t timeout) {
        DEBUG_ASSERT(s);

        if (!timeout) {

            futex_wait(current_task, s, value, NULL);

        } else {

            struct timespec ts;
            ts.tv_sec  = timeout / 1000;
            ts.tv_nsec = (timeout % 1000) * 1000000;

            futex_wait(current_task, s, value, &ts);
        }

        thread_suspend(current_task);
        thread_restart_sched(current_task);

        return 0;
    });