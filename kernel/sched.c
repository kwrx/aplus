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
#include <time.h>                
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/task.h>
#include <aplus/hal.h>


extern long sys_clock_gettime(clockid_t, struct timespec*);


spinlock_t sched_lock = SPINLOCK_INIT;




static inline void __check_futex(void) {

    list_each(current_task->futexes, i) {
            
        if(futex_expired(i))
            futex_wakeup_thread(current_task, i);

    }

}


static inline void __check_sleep(void) {

    if(unlikely(current_task->sleep.timeout.tv_sec || current_task->sleep.timeout.tv_nsec)) {

        uint64_t tss = current_task->sleep.timeout.tv_sec;
        uint64_t tsn = current_task->sleep.timeout.tv_nsec;

        struct timespec t0;
        sys_clock_gettime(current_task->sleep.clockid, &t0);


        if((tss * 1000000000ULL) + tsn < ((uint64_t) t0.tv_sec * 1000000000ULL) + (uint64_t) t0.tv_nsec)
            thread_wake(current_task);
            
    }

}



static void __sched_next(void) {

    do {

        do {

            current_task = current_task->next;

            if(unlikely(!current_task))
                current_task = current_cpu->sched_queue;


            DEBUG_ASSERT(current_task);
            DEBUG_ASSERT(current_task->frame);

        } while(current_task->affinity & (1 << current_cpu->id));



        if(unlikely(current_task->status == TASK_STATUS_STOP))
            continue;

        if(unlikely(current_task->status == TASK_STATUS_ZOMBIE))
            continue;

        

        //__check_timers();


        if(unlikely(current_task->status != TASK_STATUS_SLEEP))
            continue;


        __check_futex();
        __check_sleep();


    } while(current_task->status != TASK_STATUS_READY);

}



void schedule(int resched) {


    task_t* prev_task = current_task;


    #define __update_clock(task, type, delta) {                             \
        if(task->clock[type].tv_nsec + delta > 999999999) {                 \
            task->clock[type].tv_nsec  = delta;                             \
            task->clock[type].tv_nsec -= 1000000000;                        \
            task->clock[type].tv_sec  += 1;                                 \
        } else {                                                            \
            task->clock[type].tv_nsec += delta;                             \
        }                                                                   \
    }



    uint64_t elapsed = arch_timer_percpu_getns();
    uint64_t delta   = elapsed - current_cpu->ticks;


    __update_clock(current_task, TASK_CLOCK_SCHEDULER, TASK_SCHEDULER_PERIOD_NS);
    __update_clock(current_task, TASK_CLOCK_THREAD_CPUTIME, delta);
    __update_clock(current_task, TASK_CLOCK_PROCESS_CPUTIME, delta);

    if(likely(current_task->parent))
        __update_clock(current_task->parent, TASK_CLOCK_PROCESS_CPUTIME, delta);
    

    current_cpu->ticks = elapsed;



    if(!resched) {

        int64_t e;
        switch(current_task->policy) {

            case TASK_POLICY_RR:
                
                e = ((int64_t) current_task->clock[TASK_CLOCK_SCHEDULER].tv_sec  * 1000LL +
                     (int64_t) current_task->clock[TASK_CLOCK_SCHEDULER].tv_nsec / 1000000LL) % (20LL - current_task->priority);
                
                break;


            // TODO: Scheduling policy

            default:
                e = 0;
                break;

        }


        if(likely(e))
            return;

        current_task->rusage.ru_nivcsw++;

    } else
        current_task->rusage.ru_nvcsw++;




    __lock(&current_cpu->sched_lock, {


        if(likely(current_task->status == TASK_STATUS_RUNNING))
            current_task->status = TASK_STATUS_READY;


        __sched_next();

        current_task->status = TASK_STATUS_RUNNING;
        

        arch_task_switch(prev_task, current_task);

    }); 

}



void sched_enqueue(task_t* task) {

    cpu_t* cpu = NULL;
    size_t min = ~0UL;


    cpu_foreach(i) {

        if(task->affinity & (1 << i->id))
            continue;

        if(i->sched_count > min)
            continue;

        cpu = i;
        min = i->sched_count;

    }


    DEBUG_ASSERT(cpu);

    __lock(&cpu->sched_lock, {

        task->next = cpu->sched_queue;
        cpu->sched_queue = task;
        cpu->sched_count++;

    });


#if defined(DEBUG) && DEBUG_LEVEL >= 4
    kprintf("sched: enqueued task(%d) %s in cpu(%d) count(%d)\n", task->tid, task->argv[0], cpu->id, cpu->sched_count);
#endif

}


void sched_dequeue(task_t* task) {

    int found = 0;

    cpu_foreach_if(cpu, !found) {

        found = 1;

        __lock(&cpu->sched_lock, {
            
            if(task == cpu->sched_queue)
                cpu->sched_queue = task->next;

            else {

                task_t* tmp;
                for(tmp = cpu->sched_queue; tmp->next; tmp = tmp->next)
                    if(tmp->next == task)
                        break;

                if(unlikely(!tmp->next))
                    found = 0;
                else 
                    tmp->next = task->next;
                
            }

        });

        if(found)
            cpu->sched_count--;

    }

#if defined(DEBUG) && DEBUG_LEVEL >= 4
    kprintf("sched: dequeued task(%d) %s\n", task->tid, task->argv[0]);
#endif

    kfree(task);

}


pid_t sched_nextpid(void) {
    static pid_t p = 0;
    return ++p;
}
