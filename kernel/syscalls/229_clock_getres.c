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

// TODO: update <sys/features.h>
#define _POSIX_CPUTIME
#define _POSIX_THREAD_CPUTIME
#define _POSIX_MONOTONIC_CLOCK
#include <time.h>

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
 * Name:        clock_getres
 * Description: clock and time functions
 * URL:         http://man7.org/linux/man-pages/man2/clock_getres.2.html
 *
 * Input Parameters:
 *  0: 0xe5
 *  1: clockid_t which_clock
 *  2: struct timespec  * tp
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(
    229, clock_getres, long sys_clock_getres(clockid_t which_clock, struct timespec* tp) {
        if (unlikely(!tp))
            return -EINVAL;

        if (unlikely(!uio_check(tp, R_OK | W_OK)))
            return -EFAULT;


        switch (which_clock) {

            case CLOCK_REALTIME:

                tp->tv_sec  = 1;
                tp->tv_nsec = 0;
                break;

            case CLOCK_MONOTONIC:

                tp->tv_sec  = 0;
                tp->tv_nsec = 1000000000 / arch_timer_generic_getres();
                break;

            case CLOCK_THREAD_CPUTIME_ID:
            case CLOCK_PROCESS_CPUTIME_ID:

                tp->tv_sec  = 0;
                tp->tv_nsec = 1000000000 / arch_timer_percpu_getres();
                break;

            default:
                return -EINVAL;
        }


        if (tp->tv_sec == 0 && tp->tv_nsec == 0)
            tp->tv_nsec = 1;

        return 0;
    });
