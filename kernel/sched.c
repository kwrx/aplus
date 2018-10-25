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


#include <aplus.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <aplus/task.h>
#include <aplus/intr.h>
#include <aplus/mm.h>
#include <libc.h>





volatile task_t* current_task = NULL;
volatile task_t* kernel_task = NULL;
volatile task_t* task_queue = NULL;

static ktime_t __last_scheduling = 0;

#define __timing(t)                                                 \
    register ktime_t t = timer_getus() - __last_scheduling;         \
    __last_scheduling = timer_getus();                              \



static void sched_next(void) {
    do {
        current_task = current_task->next;
        if(unlikely(!current_task))
            current_task = task_queue;

        KASSERT(current_task);

        
        if(unlikely(current_task->status == TASK_STATUS_ZOMBIE))
            continue;


        /* Check Timers */
        for(int i = 0; i < 3; i++) {
            if(likely(!current_task->itimers[i].it_value))
                continue;
                

             ktime_t tm = (timer_gettimestamp() * 1000000) +
                          (timer_getus() % 1000000);

                        
            if(likely(current_task->itimers[i].it_value <= tm)) {
                if(current_task->itimers[i].it_interval)
                    current_task->itimers[i].it_value += current_task->itimers[i].it_interval;
                else
                    current_task->itimers[i].it_value = 0;


                siginfo_t si;
                si.si_code = SI_TIMER;
                si.si_pid = current_task->pid;
                si.si_uid = current_task->uid;
                si.si_value.sival_ptr = NULL;
                si.si_timerid = i;
                 
                switch(i) {
                    case ITIMER_REAL:
                        sched_sigqueueinfo(current_task, SIGALRM, &si);
                        break;
                    case ITIMER_VIRTUAL:
                        sched_sigqueueinfo(current_task, SIGVTALRM, &si);
                        break;
                    case ITIMER_PROF:
                        sched_sigqueueinfo(current_task, SIGPROF, &si);
                        break;
                }
            }
        }



        if(likely(current_task->status != TASK_STATUS_SLEEP))
            continue;
        


        /* Check Sleep */
        if(unlikely(current_task->sleep.tv_sec || current_task->sleep.tv_nsec)) {
            uint64_t ts = current_task->sleep.tv_sec;
            uint64_t tn = current_task->sleep.tv_nsec;

            struct timespec t0;
            sys_clock_gettime(CLOCK_MONOTONIC, &t0);

            if((ts * 1000000000ULL) + tn < ((uint64_t) t0.tv_sec * 1000000000ULL) + t0.tv_nsec) {
                current_task->status = TASK_STATUS_READY;
                break;
            }
        }

        
        /* Check Waiters */
        list_each(current_task->waiters, w) {
            if(likely(w->status != TASK_STATUS_ZOMBIE && w->status != TASK_STATUS_STOP))
                continue;

            current_task->status = TASK_STATUS_READY;
            break;
        }
        
    } while(current_task->status != TASK_STATUS_READY);
}




pid_t sched_nextpid() {
    static pid_t nextpid = 1;
    return nextpid++;
}


void sched_dosignals() {
    void __do(struct sigaction* act, siginfo_t* sig) {
        void* fn = act->sa_handler;
        if(unlikely(fn == SIG_ERR || fn == SIG_IGN))
            return;

        if(act->sa_flags & SA_RESETHAND)
            act->sa_handler = SIG_DFL;

        if(likely(fn == SIG_DFL)) {
            switch(sig->si_signo) {

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
                    sys_exit((1 << 31) | W_EXITCODE(0, sig->si_signo));
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
                    sys_exit((1 << 31) | W_EXITCODE(0, sig->si_signo) | WCOREFLAG);
                    break;

                /* STOP */
                case SIGTSTP:
                case SIGTTIN:
                case SIGTTOU:
                    sys_exit((1 << 31) | W_STOPCODE(sig->si_signo));
                    break;

            }
        } else {
            sigset_t old;
            sys_rt_sigprocmask(SIG_SETMASK, &act->sa_mask, &old, 0);
            

            if(act->sa_flags & SA_SIGINFO)
                ((void(*)(int, void*, void*)) fn) (sig->si_signo, sig, NULL);
            else
                ((void(*)(int)) fn) (sig->si_signo);
                

            sys_rt_sigprocmask(SIG_SETMASK, &old, NULL, 0);
        }
    }


    if(unlikely((list_length(current_task->signal.s_queue) > 0))) {            
        siginfo_t* sig;
        sig = list_back(current_task->signal.s_queue);
        list_pop_back(current_task->signal.s_queue);
        
        switch(sig->si_signo) {
            case SIGKILL:
                kfree(sig);
                sys_exit((1 << 31) | W_EXITCODE(0, SIGKILL));
                break;
            case SIGSTOP:
                sys_exit((1 << 31) | W_STOPCODE(SIGSTOP));
                break;
            default:
                __do(&current_task->signal.s_handlers[sig->si_signo], sig);
                break;
        }

        kfree(sig);
    }
}



void sched_sigqueueinfo(task_t* tk, int signo, siginfo_t* sig) {
    if(unlikely(!tk || !sig))
        return;

    if(unlikely(tk == kernel_task))
        return;
    
    if(unlikely(signo < 0 || signo > TASK_NSIG))
        return;

    if(unlikely(tk->status == TASK_STATUS_ZOMBIE))
        return;

    if(unlikely(sigismember(&tk->signal.s_mask, signo)))
        return;

    siginfo_t* vsig = (siginfo_t*) kmalloc(sizeof(siginfo_t), GFP_KERNEL);
    memcpy(vsig, sig, sizeof(siginfo_t));
    
    vsig->si_signo = signo; 
    list_push(tk->signal.s_queue, vsig);

    if (tk->status != TASK_STATUS_STOP || (signo == SIGCONT || signo == SIGKILL))
        tk->status = TASK_STATUS_READY;

    tk->rusage.ru_nsignals++;
}


void schedule(void) {
    if(unlikely(!current_task))
        return;

    INTR_OFF;

    __timing(t);

    current_task->clock.tms_utime += t;
    if(likely(current_task->parent))
        current_task->parent->clock.tms_cutime += t;

    if(likely(((int)current_task->clock.tms_utime / 1000) % ((int)(20 - current_task->priority))))
        goto nosched;



    current_task->rusage.ru_nivcsw++;

    if(likely(current_task->status == TASK_STATUS_RUNNING))
        current_task->status = TASK_STATUS_READY;


    volatile task_t* prev_task = current_task;
    sched_next();

    
    current_task->status = TASK_STATUS_RUNNING;
    task_switch(prev_task, current_task);

nosched:
    INTR_ON;
    return;
}

void schedule_yield(void) {
    if(unlikely(!current_task))
        return;

    INTR_OFF;

    __timing(t);

    current_task->clock.tms_utime += t;
    if(likely(current_task->parent))
        current_task->parent->clock.tms_cutime += t;


    current_task->rusage.ru_nvcsw++;

    if(likely(current_task->status == TASK_STATUS_RUNNING))
        current_task->status = TASK_STATUS_READY;


    volatile task_t* prev_task = current_task;
    sched_next();
    

    current_task->status = TASK_STATUS_RUNNING;
    task_switch(prev_task, current_task);

    INTR_ON;
}


EXPORT(current_task);
EXPORT(kernel_task);
EXPORT(task_queue);
