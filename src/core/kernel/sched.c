/*                                                                      
 * GPL3 License                                                         
 *                                                                      
 * Author(s):                                                              
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
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

#include <stdint.h>                                                        
#include <aplus/core/base.h>
#include <aplus/core/debug.h>
#include <aplus/core/memory.h>
#include <aplus/core/smp.h>
#include <aplus/core/hal.h>
#include <aplus/core/task.h>


task_t* sched_queue = NULL;
spinlock_t sched_lock = SPINLOCK_INIT;




static void __sched_next(void) {

    do {

        current_task = current_task->next;

        if(unlikely(!current_task))
            current_task = sched_queue;


        DEBUG_ASSERT(current_task);
        DEBUG_ASSERT(current_task->frame);


        if(unlikely(current_task->affinity & (1 << current_cpu->id)))
            continue;

        if(unlikely(current_task->status != TASK_STATUS_READY))
            continue;

        break;

    } while(1);

}



void schedule(void* frame) {

    __lock(&sched_lock, {

        // TODO: Calculate timing

        current_task->clock.tms_utime += 1000;

        if(likely(current_task->parent))
            current_task->parent->clock.tms_cutime += 1000;

        
    
        int e;
        switch(current_task->policy) {

            case TASK_POLICY_RR:
                e = current_task->clock.tms_utime % ((20 - current_task->priority) * 1000);
                break;


            // TODO: Scheduling policy

            default:
                e = 1;
                break;

        }


        if(likely(!e))
            break;


        //current_task->rusage.ru_nivcsw++:

        if(likely(current_task->status == TASK_STATUS_RUNNING))
            current_task->status = TASK_STATUS_READY;


        task_t* p = current_task;
        __sched_next();


        current_task->status = TASK_STATUS_RUNNING;
        arch_task_switch(frame, p, current_task);

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
    return ++p;
}


