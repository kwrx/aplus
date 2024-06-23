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



#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/smp.h>
#include <aplus/errno.h>
#include <aplus/hal.h>





/***
 * Name:        clock_gettime
 * Description: clock and time functions
 * URL:         http://man7.org/linux/man-pages/man2/clock_gettime.2.html
 *
 * Input Parameters:
 *  0: 0xe4
 *  1: clockid_t which_clock
 *  2: struct timespec  * tp
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(228, clock_gettime,
long sys_clock_gettime (clockid_t which_clock, struct timespec  * user_tp) {
    
    if(unlikely(!user_tp))
        return -EINVAL;

    if(unlikely(!uio_check(user_tp, R_OK | W_OK)))
        return -EFAULT;


    struct timespec tp;
    
    switch(which_clock) {

        case CLOCK_REALTIME:

            tp.tv_sec  = arch_timer_gettime();
            tp.tv_nsec = 0;
            break;

        case CLOCK_MONOTONIC:

            tp.tv_sec  = arch_timer_generic_getms() / 1000ULL;
            tp.tv_nsec = arch_timer_generic_getns() % 1000000000ULL;
            break;

        case CLOCK_THREAD_CPUTIME_ID:

            tp.tv_sec  = current_task->clock[TASK_CLOCK_THREAD_CPUTIME].tv_sec;
            tp.tv_nsec = current_task->clock[TASK_CLOCK_THREAD_CPUTIME].tv_nsec;
            break;

         case CLOCK_PROCESS_CPUTIME_ID:

            tp.tv_sec  = current_task->clock[TASK_CLOCK_PROCESS_CPUTIME].tv_sec;
            tp.tv_nsec = current_task->clock[TASK_CLOCK_PROCESS_CPUTIME].tv_nsec;
            break;

        default:
            return -EINVAL;

    }


    uio_memcpy_s2u(user_tp, &tp, sizeof(struct timespec));

    return 0;
    
});
