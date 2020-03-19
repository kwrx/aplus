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
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/task.h>

#include <hal/cpu.h>
#include <hal/task.h>
#include <hal/timer.h>
#include <hal/interrupt.h>


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



void schedule(int yield) {

    task_t* prev_task = current_task;


    __lock(&sched_lock, {


        DEBUG_ASSERT(CLOCKS_PER_SEC >= 0);
        DEBUG_ASSERT(CLOCKS_PER_SEC <= 1000000000);


#if   CLOCKS_PER_SEC == 1000
        uint64_t elapsed = arch_timer_percpu_getms();
#elif CLOCKS_PER_SEC == 1000000
        uint64_t elapsed = arch_timer_percpu_getus();
#elif CLOCKS_PER_SEC == 1000000000
        uint64_t elapsed = arch_timer_percpu_getns();
#else
        uint64_t elapsed = arch_timer_percpu_getns() / (1000000000 / CLOCKS_PER_SEC)
#endif


        // FIXME
        current_task->clock.tms_utime += elapsed - current_cpu->running_ticks;

        if(likely(current_task->parent))
            current_task->parent->clock.tms_cutime += elapsed - current_cpu->running_ticks;
        
        current_cpu->running_ticks = elapsed;



        if(current_task->rusage.ru_utime.tv_usec + 10000000 > 999999999) {

            current_task->rusage.ru_utime.tv_usec = 0;
            current_task->rusage.ru_utime.tv_sec += 1;

        } else
            current_task->rusage.ru_utime.tv_usec += 10000000;
        


        if(!yield) {

            int e;
            switch(current_task->policy) {

                case TASK_POLICY_RR:
                   
                    e = ((uint64_t) current_task->rusage.ru_utime.tv_sec * 1000000ULL +
                         (uint64_t) current_task->rusage.ru_utime.tv_usec             ) % ((20 - current_task->priority) * 1000);
                    
                    break;


                // TODO: Scheduling policy

                default:
                    e = 0;
                    break;

            }


            current_task->rusage.ru_nivcsw++;

            if(likely(e))
                break;


        } else
            current_task->rusage.ru_nvcsw++;




        if(likely(current_task->status == TASK_STATUS_RUNNING))
            current_task->status = TASK_STATUS_READY;

        __sched_next();

        current_task->status = TASK_STATUS_RUNNING;
        

        if(likely(prev_task != current_task))
            arch_task_switch(prev_task, current_task);

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


