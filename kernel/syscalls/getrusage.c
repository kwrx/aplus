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
 
#define _GNU_SOURCE
#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>


static inline void __add(struct rusage* d, struct rusage* s) {
    d->ru_maxrss += s->ru_maxrss;
    d->ru_ixrss += s->ru_ixrss;
    d->ru_idrss += s->ru_idrss;
    d->ru_isrss += s->ru_isrss;
    d->ru_minflt += s->ru_minflt;
    d->ru_majflt += s->ru_majflt;
    d->ru_nswap += s->ru_nswap;
    d->ru_inblock += s->ru_inblock;
    d->ru_oublock += s->ru_oublock;
    d->ru_msgsnd += s->ru_msgsnd;
    d->ru_msgrcv += s->ru_msgrcv;
    d->ru_nsignals += s->ru_nsignals;
    d->ru_nvcsw += s->ru_nvcsw;
    d->ru_nivcsw = d->ru_nivcsw;



    uint64_t tmd = (d->ru_utime.tv_sec * 1000000) +
                   (d->ru_utime.tv_usec % 1000000);
    
    uint64_t tms = (s->ru_utime.tv_sec * 1000000) +
                   (s->ru_utime.tv_usec % 1000000);

    tmd += tms;

    d->ru_utime.tv_sec = tmd / 1000000;
    d->ru_utime.tv_usec = tmd % 1000000;
}


SYSCALL(77, getrusage,
int sys_getrusage(int who, struct rusage* usage) {
    if(unlikely(!usage)) {
        errno = EINVAL;
        return -1;
    }

    memset(usage, 0, sizeof(struct rusage));


    switch(who) {
        case RUSAGE_THREAD:
            memcpy(usage, &current_task->rusage, sizeof(struct rusage));
            return 0;

        case RUSAGE_CHILDREN: {
            task_t* tmp;
            for(tmp = task_queue; tmp; tmp = tmp->next) {
                if(tmp->parent != current_task)
                    continue;

                if(tmp->status != TASK_STATUS_ZOMBIE)
                    continue;

                __add(usage, &tmp->rusage);
            }

            return 0;
        } break;

        case RUSAGE_SELF: {
            task_t* tmp;
            for(tmp = task_queue; tmp; tmp = tmp->next) {
                if(tmp->tgid != current_task->pid)
                    continue;

                __add(usage, &tmp->rusage);
            }

            if(current_task->tgid != current_task->pid)
                __add(usage, &current_task->rusage);

            return 0;
        } break;

        default:
            break;
    }

    errno = EINVAL;
    return -1;
});
