/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/task.h>

#include <aplus/utils/list.h>
#include <aplus/utils/queue.h>


extern long sys_clock_gettime(clockid_t, struct timespec*);



static inline void do_futex(void) {

    list_each(current_task->futexes, i) {

        if (!futex_expired(i))
            continue;


        list_remove(current_task->futexes, i);

        thread_wake(current_task);
    }
}

static inline void do_sleep(void) {

    if (unlikely(current_task->sleep.timeout.tv_sec || current_task->sleep.timeout.tv_nsec)) {

        struct timespec t0;
        sys_clock_gettime(current_task->sleep.clockid, &t0);

        uint64_t tss = (current_task->sleep.timeout.tv_sec * 1000000000ULL) + current_task->sleep.timeout.tv_nsec;
        uint64_t tsc = (t0.tv_sec * 1000000000ULL) + t0.tv_nsec;


        if (current_task->sleep.remaining) {

            uint64_t time_remaining_ns = tss - tsc;

            current_task->sleep.remaining->tv_sec  = time_remaining_ns / 1000000000ULL;
            current_task->sleep.remaining->tv_nsec = time_remaining_ns % 1000000000ULL;
        }

        if (tss < tsc) {

            current_task->sleep.timeout.tv_sec  = 0L;
            current_task->sleep.timeout.tv_nsec = 0L;
            current_task->sleep.remaining       = NULL;
            current_task->sleep.expired         = true;

            thread_wake(current_task);
        }
    }
}


static void handle_default_signal(const siginfo_t* siginfo) {

    switch (siginfo->si_signo) {

        // TERM signals
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
            sys_exit((1U << 31) | siginfo->si_signo);
            break;

        // CORE signals
        case SIGQUIT:
        case SIGILL:
        case SIGABRT:
        case SIGFPE:
        case SIGSEGV:
        case SIGBUS:
        case SIGSYS:
        case SIGXCPU:
        case SIGXFSZ:
            sys_exit((1U << 31) | siginfo->si_signo | 0x80);
            break;

        // STOP signals
        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:
            sys_exit((1U << 31) | (siginfo->si_signo << 8) | 0x7F);
            break;
    }
}



static void handle_user_signal(siginfo_t* siginfo, struct ksigaction* action) {

    arch_task_prepare_to_signal(siginfo);

    if (action->sa_flags & SA_RESETHAND) {
        action->handler = SIG_DFL;
    }
}


static void handle_default_or_user_signal(siginfo_t* siginfo) {

    struct ksigaction* action = NULL;

    shared_ptr_access(current_task->sighand, sighand, { action = &sighand->action[siginfo->si_signo]; });


    if (unlikely(!action)) {
        return;
    }

    if (unlikely(action->handler == SIG_ERR)) {
        return;
    }

    if (unlikely(action->handler == SIG_IGN)) {
        return;
    }

    if (unlikely(action->handler == SIG_DFL)) {
        handle_default_signal(siginfo);
    } else {
        handle_user_signal(siginfo, action);
    }
}


static void handle_signal(siginfo_t* siginfo) {

#if DEBUG_LEVEL_TRACE
    kprintf("sched: received signal(%d) from tid(%d) to tid(%d)\n", siginfo->si_signo, siginfo->si_pid, current_task->tid);
#endif

    switch (siginfo->si_signo) {

        case SIGKILL:
            sys_exit((1 << 31) | SIGKILL);
            break;

        case SIGSTOP:
            sys_exit((1 << 31) | (SIGSTOP << 8) | 0x7F);
            break;

        default:
            handle_default_or_user_signal(siginfo);
            break;
    }
}


static inline void do_signals(void) {

    DEBUG_ASSERT(current_task);

    if (queue_is_empty(&current_task->sigqueue)) {
        return;
    }


    siginfo_t* siginfo;

    if ((siginfo = (siginfo_t*)queue_pop(&current_task->sigqueue)) != NULL) {

        DEBUG_ASSERT(siginfo);
        DEBUG_ASSERT(siginfo->si_signo >= 0);
        DEBUG_ASSERT(siginfo->si_signo <= _NSIG);

        current_task->rusage.ru_nsignals += 1;

        handle_signal(siginfo);
        kfree(siginfo);
    }
}


static void __sched_next(void) {

    do {


        current_task = current_task->next;

        if (unlikely(!current_task)) {
            current_task = current_cpu->sched_queue;
        }

        if (unlikely(current_task->status == TASK_STATUS_STOP)) {
            continue;
        }

        if (unlikely(current_task->status == TASK_STATUS_ZOMBIE)) {
            continue;
        }



        // //__check_timers();


        if (current_task->status == TASK_STATUS_SLEEP) {

            if (!queue_is_empty(&current_task->sigqueue)) {
                thread_wake(current_task);
            }


            do_futex();
            do_sleep();
        }


    } while (current_task->status != TASK_STATUS_READY);
}


/**
 * @brief Schedules the next task to run
 *
 * This function updates the clocks of the current task and its parent, if it has one.
 * It also keeps track of the number of voluntary and involuntary context switches.
 * If resched is set to true, it marks the current task as TASK_STATUS_READY and selects the next task to run by calling __sched_next().
 * The selected task is then marked as TASK_STATUS_RUNNING and a task switch is performed using the arch_task_switch() function.
 * Finally, the function calls do_signals() to handle any pending signals.
 *
 * @param resched Specifies whether the current task is being voluntarily or involuntarily rescheduled
 *
 */
void schedule(int resched) {

    DEBUG_ASSERT(current_cpu);
    DEBUG_ASSERT(current_task);

#define UPDATE_CLOCK(task, type, delta)                      \
    {                                                        \
        if (task->clock[type].tv_nsec + delta > 999999999) { \
            task->clock[type].tv_nsec = delta;               \
            task->clock[type].tv_nsec -= 1000000000;         \
            task->clock[type].tv_sec += 1;                   \
        } else {                                             \
            task->clock[type].tv_nsec += delta;              \
        }                                                    \
    }



    task_t* prev_task = current_task;


    uint64_t elapsed = arch_timer_percpu_getns();
    uint64_t delta   = elapsed - current_cpu->ticks;


    UPDATE_CLOCK(current_task, TASK_CLOCK_SCHEDULER, TASK_SCHEDULER_PERIOD_NS);
    UPDATE_CLOCK(current_task, TASK_CLOCK_THREAD_CPUTIME, delta);
    UPDATE_CLOCK(current_task, TASK_CLOCK_PROCESS_CPUTIME, delta);

    if (likely(current_task->parent)) {
        UPDATE_CLOCK(current_task->parent, TASK_CLOCK_PROCESS_CPUTIME, delta);
    }


    current_cpu->ticks = elapsed;



    if (!resched) {
        current_task->rusage.ru_nivcsw++;
    } else {
        current_task->rusage.ru_nvcsw++;
    }



    if (likely(current_task->status == TASK_STATUS_RUNNING)) {
        current_task->status = TASK_STATUS_READY;
    }

    if (likely((current_task->flags & TASK_FLAGS_NO_FRAME) == 0)) {
        __sched_next();
    }


    current_task->status = TASK_STATUS_RUNNING;



    scoped_lock(&current_cpu->sched_lock) {
        arch_task_switch(prev_task, current_task);
    }


    do_signals();
}


/**
 * @brief Enqueues a task to a CPU with the least number of tasks
 *
 * This function schedules the task to a CPU with the least number of tasks.
 *
 * @param task The task to be enqueued
 */
void sched_enqueue(task_t* task) {

    cpu_t* cpu = NULL;
    size_t min = ~0UL;


    cpu_foreach(i) {

        if (!(CPU_ISSET(i->id, &task->affinity))) {
            continue;
        }

        if (i->sched_count > min) {
            continue;
        }

        cpu = i;
        min = i->sched_count;
    }

    DEBUG_ASSERT(cpu);

    scoped_lock(&cpu->sched_lock) {
        task->next = cpu->sched_queue;

        cpu->sched_queue = task;
        cpu->sched_count++;
    }


#if DEBUG_LEVEL_TRACE
    kprintf("sched: enqueued task(%d) %s in cpu(%ld) count(%ld)\n", task->tid, task->argv[0], cpu->id, cpu->sched_count);
#endif
}

/**
 * @brief Dequeues a task from its assigned CPU
 *
 * This function removes the task from the queue of the CPU it is assigned to.
 *
 * @param task The task to be dequeued
 */
void sched_dequeue(task_t* task) {

    int found = 0;

    cpu_foreach_if(cpu, !found) {

        found = 1;

        scoped_lock(&cpu->sched_lock) {
            if (task == cpu->sched_queue) {

                cpu->sched_queue = task->next;

            } else {

                task_t* tmp;

                for (tmp = cpu->sched_queue; tmp->next; tmp = tmp->next) {

                    if (tmp->next != task) {
                        continue;
                    }

                    tmp->next = task->next;
                    break;
                }

                if (unlikely(!tmp->next)) {
                    found = 0;
                }
            }
        }

        if (found) {
            cpu->sched_count--;
            break;
        }
    }

#if DEBUG_LEVEL_TRACE
    kprintf("sched: dequeued task(%d) %s\n", task->tid, task->argv[0]);
#endif

    // // kfree(task);
    // FIXME: unsafe to free here. Moreover there are memory leaks of unfreed attributes
}


void sched_requeue(task_t* task) {

    int found = 0;

    cpu_foreach_if(cpu, !found) {

        found = 1;

        scoped_lock(&cpu->sched_lock) {

            if (task == cpu->sched_queue) {

                cpu->sched_queue = task->next;

            } else {

                task_t* tmp;

                for (tmp = cpu->sched_queue; tmp->next; tmp = tmp->next) {

                    if (tmp->next != task) {
                        continue;
                    }

                    tmp->next = task->next;
                    break;
                }

                if (unlikely(!tmp->next)) {
                    found = 0;
                }
            }
        }

        if (found) {

            if (cpu->sched_running != task) {

                task->next = cpu->sched_running->next;

                cpu->sched_running->next = task;

            } else {

                task->next = cpu->sched_queue;

                cpu->sched_queue = task;
            }

            break;
        }
    }

#if DEBUG_LEVEL_TRACE
    kprintf("sched: requeued task(%d) %s\n", task->tid, task->argv[0]);
#endif
}



int sched_sigqueueinfo(gid_t pgrp, pid_t pid, pid_t tid, int sig, siginfo_t* info) {

    DEBUG_ASSERT(sig >= 0);
    DEBUG_ASSERT(sig < NSIG - 1);
    DEBUG_ASSERT(info);


    size_t found = 0;

    cpu_foreach(cpu) {

        for (task_t* tmp = cpu->sched_queue; tmp; tmp = tmp->next) {

            if (pgrp > 0 && tmp->pgrp != pgrp) {
                continue;
            }

            if (pid > 0 && tmp->pid != pid) {
                continue;
            }

            if (tid > 0 && tmp->tid != tid) {
                continue;
            }

            if (tmp->status == TASK_STATUS_ZOMBIE) {
                continue;
            }

            if (!(current_task->euid == tmp->uid || current_task->uid == tmp->uid)) {
                continue;
            }


            found++;

            if (unlikely(sig == 0)) {
                continue;
            }

            if (tmp->sigqueue.size > tmp->rlimits[RLIMIT_SIGPENDING].rlim_cur) {
                return errno = EAGAIN, -1;
            }


            struct ksigaction* action = NULL;

            shared_ptr_access(tmp->sighand, sighand, { action = &sighand->action[sig]; });

            DEBUG_ASSERT(action);


            if (unlikely(action->handler == SIG_IGN)) {
                continue;
            }

            if (unlikely(action->handler == SIG_ERR)) {
                continue;
            }


            siginfo_t* siginfo = (siginfo_t*)kcalloc(1, sizeof(siginfo_t), GFP_KERNEL);

            if (unlikely(!siginfo)) {
                return errno = ENOMEM, -1;
            }

            memcpy(siginfo, info, sizeof(siginfo_t));

            siginfo->si_signo = sig;


            shared_ptr_access(tmp->sighand, sighand, {
                //? Check if the signal is blocked
                if (unlikely(sighand->sigmask.__bits[sig / (sizeof(long) << 3)] & (1 << (sig % (sizeof(long) << 3))))) {

                    if (sighand->action[sig].sa_flags & SA_NODEFER)
                        queue_enqueue(&tmp->sigqueue, siginfo, 0);
                    else
                        queue_enqueue(&tmp->sigpending, siginfo, 0);

                } else {
                    queue_enqueue(&tmp->sigqueue, siginfo, 0);
                }
            });
        }
    }


    if (unlikely(found == 0)) {
        return errno = ESRCH, -1;
    }

    return 0;
}


/**
 * @brief Generates the next unique process ID
 *
 * This function generates and returns the next unique process ID, by incrementing a static variable.
 *
 * @return The next unique process ID
 */
pid_t sched_nextpid(void) {
    static pid_t p = 0;
    return ++p;
}
