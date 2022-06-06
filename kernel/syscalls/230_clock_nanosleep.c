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
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/hal.h>
#include <aplus/errno.h>


extern long sys_clock_gettime(clockid_t, struct timespec*);


/***
 * Name:        clock_nanosleep
 * Description: high-resolution sleep with specifiable clock
 * URL:         http://man7.org/linux/man-pages/man2/clock_nanosleep.2.html
 *
 * Input Parameters:
 *  0: 0xe6
 *  1: clockid_t which_clock
 *  2: int flags
 *  3: const struct timespec __user * rqtp
 *  4: struct timespec __user * rmtp
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

SYSCALL(230, clock_nanosleep,
long sys_clock_nanosleep (clockid_t which_clock, int flags, const struct timespec __user * __rqtp, struct timespec __user * __rmtp) {
    
    if(unlikely(!__rqtp))
        return -EINVAL;

    if(unlikely(!uio_check(__rqtp, R_OK)))
        return -EFAULT;

    if(unlikely(__rmtp))
        if(unlikely(!uio_check(__rmtp, R_OK | W_OK)))
            return -EFAULT;


    struct timespec rqtp;
    uio_memcpy_u2s(&rqtp, __rqtp, sizeof(struct timespec));



    if(rqtp.tv_nsec < 0 || rqtp.tv_nsec > 999999999)
        return -EINVAL;



    if(unlikely(current_task->sleep.timeout.tv_sec || current_task->sleep.timeout.tv_nsec || current_task->sleep.expired)) {

        bool expired = current_task->sleep.expired;

        current_task->sleep.timeout.tv_sec  = 0L;
        current_task->sleep.timeout.tv_nsec = 0L;
        current_task->sleep.remaining = NULL;
        current_task->sleep.expired = false;
        
        if(expired)
            return 0;
        else
            return -EINTR;

    }



    switch(which_clock) {

        case CLOCK_REALTIME:
        case CLOCK_MONOTONIC:
        case CLOCK_PROCESS_CPUTIME_ID:
            break;

        default:
            return -EINVAL;

    }


    uint64_t tss = 0ULL;
    uint64_t tsn = 0ULL;

#if defined(TIMER_ABSTIME)
    if(!(flags & TIMER_ABSTIME))
#endif
    {
        struct timespec t0;
        if(sys_clock_gettime(which_clock, &t0) < 0)
            return -EINVAL;

        tss = t0.tv_sec;
        tsn = t0.tv_nsec;

        if(tsn + rqtp.tv_nsec > 1000000000ULL)
            tss++;

    }



    current_task->sleep.clockid = which_clock;
    current_task->sleep.timeout.tv_sec =  (tss + rqtp.tv_sec);
    current_task->sleep.timeout.tv_nsec = (tsn + rqtp.tv_nsec) % 1000000000ULL;

    if(__rmtp)
        current_task->sleep.remaining = uio_get_ptr(__rmtp);

    current_task->sleep.expired = false;


    thread_suspend(current_task);
    thread_restart_sched(current_task);
    thread_restart_syscall(current_task);

    
    return -EINTR;

});
