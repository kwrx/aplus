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
#include <signal.h>
#include <sys/wait.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/task.h>
#include <aplus/hal.h>


extern long sys_clock_gettime(clockid_t, struct timespec*);


spinlock_t sched_lock = SPINLOCK_INIT;




static inline void __do_futex(void) {

    list_each(current_task->futexes, i) {
            
        if(futex_expired(i))
            futex_wakeup_thread(current_task, i);

    }

}


static inline void __do_sleep(void) {

    if(unlikely(current_task->sleep.timeout.tv_sec || current_task->sleep.timeout.tv_nsec)) {


        struct timespec t0;
        sys_clock_gettime(current_task->sleep.clockid, &t0);

        uint64_t tss = (current_task->sleep.timeout.tv_sec * 1000000000ULL) + current_task->sleep.timeout.tv_nsec;
        uint64_t tsc = (t0.tv_sec                          * 1000000000ULL) + t0.tv_nsec;
        

        if(current_task->sleep.remaining) {

            uint64_t d = tss - tsc;

            current_task->sleep.remaining->tv_sec  = d / 1000000000ULL;
            current_task->sleep.remaining->tv_nsec = d % 1000000000ULL;

        }


        if(tss < tsc) {
            
            current_task->sleep.timeout.tv_sec  = 0L;
            current_task->sleep.timeout.tv_nsec = 0L;
            current_task->sleep.remaining = NULL;

            thread_wake(current_task);

        }
            
    }

}


static inline void __do_signals(void) {


    void __do(struct ksigaction* action, siginfo_t* siginfo) {

        DEBUG_ASSERT(action);
        DEBUG_ASSERT(siginfo);

        if(unlikely(action->handler == SIG_ERR))
            return;

        if(unlikely(action->handler == SIG_IGN))
            return;


        if(likely(action->handler == SIG_DFL)) {

            switch(siginfo->si_signo) {

                /* TERM */
                case SIGHUP:
                case SIGINT:
                case SIGPIPE:
                case SIGALRM:
                case SIGTERM:
                case SIGUSR1:
                case SIGUSR2:
                case SIGPOLL:
                case SIGPROF:
                case SIGVTALRM:
                    sys_exit((1 << 31) | siginfo->si_signo);
                    break;


                /* CORE */
                case SIGQUIT:
                case SIGILL:
                case SIGABRT:
                case SIGFPE:
                case SIGSEGV:
                case SIGBUS:
                case SIGSYS:
                case SIGXCPU:
                case SIGXFSZ:
                    sys_exit((1 << 31) | siginfo->si_signo | 0x80);
                    break;


                /* STOP */
                case SIGTSTP:
                case SIGTTIN:
                case SIGTTOU:
                    sys_exit((1 << 31) | (siginfo->si_signo << 8) | 0x7F);
                    break;                


            }

        } else {

            arch_task_prepare_to_signal(siginfo);

            if(action->sa_flags & SA_RESETHAND)
                action->handler = SIG_DFL;

        }


    }


    if(queue_is_empty(&current_task->sigqueue))
        return;

    
    siginfo_t* siginfo;
    
    if((siginfo = (siginfo_t*) queue_pop(&current_task->sigqueue)) != NULL) {
    
        DEBUG_ASSERT(siginfo->si_signo >= 0);
        DEBUG_ASSERT(siginfo->si_signo <= _NSIG);


#if defined(DEBUG) && DEBUG_LEVEL >= 4
        kprintf("sched: received signal(%d) from tid(%d) to tid(%d)\n", siginfo->si_signo, siginfo->si_pid, current_task->tid);
#endif


        siginfo_t tmp;
        memcpy(&tmp, siginfo, sizeof(siginfo_t));

        kfree(siginfo);


        current_task->rusage.ru_nsignals += 1;

        switch(siginfo->si_signo) {

            case SIGKILL:
                sys_exit((1 << 31) | SIGKILL);
                break;

            case SIGSTOP:
                sys_exit((1 << 31) | (SIGSTOP << 8) | 0x7F);
                break;

            default:
                __do(&current_task->sighand->action[tmp.si_signo], &tmp);
                break;
                
        }

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

        } while(!CPU_ISSET(current_cpu->id, &current_task->affinity));



        if(unlikely(current_task->status == TASK_STATUS_STOP))
            continue;

        if(unlikely(current_task->status == TASK_STATUS_ZOMBIE))
            continue;

        

        //__check_timers();


        if(unlikely(current_task->status != TASK_STATUS_SLEEP))
            continue;


        // Wake up if a signal is pending
        if(!queue_is_empty(&current_task->sigqueue))
            thread_wake(current_task);


        __do_futex();
        __do_sleep();


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



    if(!resched)
        current_task->rusage.ru_nivcsw++;
    else
        current_task->rusage.ru_nvcsw++;



    __lock(&current_cpu->sched_lock, {


        if(likely(current_task->status == TASK_STATUS_RUNNING))
            current_task->status = TASK_STATUS_READY;


        __sched_next();

        current_task->status = TASK_STATUS_RUNNING;
        

        arch_task_switch(prev_task, current_task);

    }); 


    __do_signals();


}



void sched_enqueue(task_t* task) {

    cpu_t* cpu = NULL;
    size_t min = ~0UL;


    cpu_foreach(i) {

        if(!(CPU_ISSET(i->id, &task->affinity)))
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
