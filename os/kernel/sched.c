/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/task.h>
#include <aplus/smp.h>
#include <stdint.h>
#include <string.h>

task_t* task_queue;


static void __sched_next(void) {
    
    __lock_irq(&current_cpu->lock, {

        task_t* tmp = current_task;

        do {
            tmp = tmp->next;
            if(!tmp)
                tmp = task_queue;

            if(unlikely(tmp == current_task))
                break;
                
            if(unlikely(!(tmp->affinity & (1 << current_cpu->id))))
                continue;

            if(likely(tmp->status != TASK_STATUS_SLEEP))
                continue;

#if 0
            if(unlikely(tmp->sleep.tv_sec || tmp->sleep.tv_nsec)) {
                uint64_t ts = tmp->sleep.tv_sec;
                uint64_t tn = tmp->sleep.tv_nsec;

                struct timespec t0;
                sys_clock_gettime(CLOCK_MONOTONIC, &t0);

                if((ts * 1000000000ULL) + tn < ((uint64_t) t0.tv_sec * 1000000000ULL) + t0.tv_nsec) {
                    tmp->status = TASK_STATUS_READY;
                    break;
                }
            }
#endif

        } while(
            !(tmp->status == TASK_STATUS_READY) &&
            !(tmp->affinity & (1 << current_cpu->id))
        );

        tmp->status = TASK_STATUS_RUNNING;
        current_task = tmp;
    });
}


pid_t sched_nextpid(void) {
    static pid_t p = 0;
    return p++;
}

void* schedule(void* frame) {
    return frame;
}