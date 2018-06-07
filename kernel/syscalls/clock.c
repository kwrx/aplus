/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/timer.h>
#include <libc.h>

SYSCALL(265, clock_gettime,
int sys_clock_gettime(clockid_t id, struct timespec *tv) {
    if(unlikely(!tv)) {
        errno = EINVAL;
        return -1;
    }

    switch(id) {
        case CLOCK_REALTIME:
            tv->tv_sec = timer_gettimestamp();
            tv->tv_nsec = (timer_getus() % 1000000) * 1000;
            break;
        case CLOCK_MONOTONIC:
            tv->tv_sec = timer_getus() / 1000000;
            tv->tv_nsec = (timer_getus() % 1000000) * 1000;
            break;
        case CLOCK_PROCESS_CPUTIME_ID:
            tv->tv_sec = (current_task->clock.tms_utime + current_task->clock.tms_cutime) / CLOCKS_PER_SEC;
            tv->tv_nsec = (current_task->clock.tms_utime + current_task->clock.tms_cutime) * (1000000000 / CLOCKS_PER_SEC);
            break;
        case CLOCK_THREAD_CPUTIME_ID:
            tv->tv_sec = (current_task->clock.tms_utime) / CLOCKS_PER_SEC;
            tv->tv_nsec = (current_task->clock.tms_utime) * (1000000000 / CLOCKS_PER_SEC);
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    return 0;
});

SYSCALL(266, clock_getres,
int sys_clock_getres(clockid_t id, struct timespec *tv) {
    if(unlikely(!tv)) {
        errno = EINVAL;
        return -1;
    }

    tv->tv_sec = 0;
    tv->tv_nsec = (1000000000 / CLOCKS_PER_SEC);

    return 0;
});
