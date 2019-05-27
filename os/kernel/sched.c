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
#include <aplus/timer.h>
#include <stdint.h>
#include <string.h>


task_t* sched_queue = NULL;
spinlock_t sched_lock;


static ktime_t __last_scheduling = 0;

#define __timing(t)                                                 \
    register ktime_t t = arch_timer_getus() - __last_scheduling;    \
    __last_scheduling = arch_timer_getus(); 





static void __sched_next(void) {    
    do {

next:
        current_task = current_task->next;
        if(unlikely(!current_task))
            current_task = sched_queue;

        DEBUG_ASSERT(current_task);

        if(unlikely(current_task->affinity & (1 << current_cpu->id)))
            goto next;

        //if(unlikely(current_task->status == TASK_STATUS_ZOMBIE))
        //    continue;

        /* TODO: Check timers, sleep, waiters */

        //if(likely(current_task->status != TASK_STATUS_SLEEP))
        //    continue;

    } while(current_task->status != TASK_STATUS_READY);
}



void* schedule(void* frame) {

    if(unlikely(!(current_cpu->flags & CPU_FLAGS_SCHED_ENABLED)))
        return frame;


    __lock_irq(&sched_lock, {

        __timing(t);

        current_task->clock.tms_utime += t;
        if(likely(current_task->parent))
            current_task->parent->clock.tms_cutime += t;


        if(likely(((int) current_task->clock.tms_utime / 1000) % ((int) (20 - current_task->priority))))
            break;

        
        current_task->rusage.ru_nivcsw++;

        if(likely(current_task->status == TASK_STATUS_RUNNING))
            current_task->status = TASK_STATUS_READY;


        task_t* prev_task = current_task;
        __sched_next();
        
        current_task->status = TASK_STATUS_RUNNING;
        frame = arch_task_switch(frame, prev_task, current_task);

    });

    return frame;
}



void* schedule_yield(void* frame) {

    if(unlikely(!(current_cpu->flags & CPU_FLAGS_SCHED_ENABLED)))
        return frame;



    __lock_irq(&sched_lock, {

        __timing(t);

        current_task->clock.tms_utime += t;
        if(likely(current_task->parent))
            current_task->parent->clock.tms_cutime += t;


        
        current_task->rusage.ru_nivcsw++;

        if(likely(current_task->status == TASK_STATUS_RUNNING))
            current_task->status = TASK_STATUS_READY;


        task_t* prev_task = current_task;
        __sched_next();

        
        current_task->status = TASK_STATUS_RUNNING;
        arch_task_switch(frame, prev_task, current_task);

    });
}



void sched_enqueue(task_t* task) {
    __lock(&sched_lock, {
        task->next = sched_queue;
        sched_queue = task;
    });
}



void sched_dequeue(task_t* task) {
    __lock(&sched_lock, {
        if(task == sched_queue)
            sched_queue = task->next;

        else {
            task_t* tmp;
            for(tmp = sched_queue; tmp->next; tmp = tmp->next)
                if(tmp->next == task)
                    break;

            DEBUG_ASSERT(tmp->next);
            DEBUG_ASSERT(tmp->next == task);

            tmp->next = task->next;
        }
    });
}



pid_t sched_nextpid(void) {
    static pid_t p = 0;
    return p++;
}



void sched_init(void) {
    
    spinlock_init(&sched_lock);

    cpu_t* cpu;
    foreach_cpu(cpu)
        sched_enqueue(&cpu->task.core);

#if 1
    foreach_cpu(cpu)
        cpu->flags |= CPU_FLAGS_SCHED_ENABLED;
#else
    current_cpu->flags |= CPU_FLAGS_SCHED_ENABLED;
#endif

}