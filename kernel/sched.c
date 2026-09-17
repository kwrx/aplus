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
#include <stdatomic.h>
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

/**
 * @brief Wakes the current task if its sleep deadline has come due, and records the time it has left.
 */
static inline void do_sleep(void) {

    if (unlikely(current_task->sleep.timeout.tv_sec || current_task->sleep.timeout.tv_nsec)) {

        struct timespec t0 = {0};

        long e = 0;

        scoped_uio_kernel() {
            e = sys_clock_gettime(current_task->sleep.clockid, &t0);
        }

        if (unlikely(e < 0))
            return;


        uint64_t tss = (current_task->sleep.timeout.tv_sec * 1000000000ULL) + current_task->sleep.timeout.tv_nsec;
        uint64_t tsc = (t0.tv_sec * 1000000000ULL) + t0.tv_nsec;


        if (current_task->sleep.remaining) {

            uint64_t time_remaining_ns = tss > tsc ? tss - tsc : 0ULL;

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


/**
 * @brief Carries out a signal's default disposition: terminate, terminate and dump core, or stop.
 *
 * @param siginfo The signal being delivered.
 */
static void handle_default_signal(const siginfo_t* siginfo) {

    switch (siginfo->si_signo) {

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

        case SIGQUIT:
        case SIGILL:
        case SIGTRAP:
        case SIGABRT:
        case SIGFPE:
        case SIGSEGV:
        case SIGBUS:
        case SIGSYS:
        case SIGXCPU:
        case SIGXFSZ:
            sys_exit((1U << 31) | siginfo->si_signo | 0x80);
            break;

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


/**
 * @brief Delivers one pending signal to the current task.
 *
 * A fatal signal exits here and never returns; a stopped task returns once SIGCONT makes it READY.
 */
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


        while (unlikely(current_task->status == TASK_STATUS_ZOMBIE || current_task->status == TASK_STATUS_STOP))
            schedule(1);
    }
}


/**
 * @brief Picks the next task to run on the current cpu, falling back on its idle task.
 *
 * At most one lap of the run queue is walked, so the scan always terminates. The caller holds
 * sched_lock and taking it disabled interrupts, so a scan that waited here for something to become
 * runnable would be waiting for a tick that can no longer arrive.
 *
 * Sleeping tasks are tested for a wakeup as they are passed, which is the only thing that ever
 * wakes them: each candidate is published in current_task because do_futex() and do_sleep() read
 * it, and sys_clock_gettime() reads the cpu-time clocks off it too.
 *
 * The idle task is not on the queue and its next is NULL, so a lap that starts on it starts at the
 * head, which is itself NULL on a cpu with nothing enqueued.
 */
static void __sched_next(void) {

    task_t* prev = current_task;
    task_t* next = prev->next ? prev->next : current_cpu->sched_queue;

    for (size_t i = current_cpu->sched_count; i && next; i--, next = next->next ? next->next : current_cpu->sched_queue) {

        current_task = next;

        if (current_task->status == TASK_STATUS_SLEEP) {

            if (!queue_is_empty(&current_task->sigqueue)) {
                thread_wake(current_task);
            }


            do_futex();
            do_sleep();
        }

        if (likely(current_task->status == TASK_STATUS_READY)) {
            return;
        }
    }

    current_task = current_cpu->sched_idle ? current_cpu->sched_idle : prev;
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
 * UPDATE_CLOCK carries the remainder into the seconds. Assigning the delta over the accumulated
 * value and then subtracting a whole second throws away the nanoseconds already banked and
 * leaves tv_nsec negative for any delta below a second -- i.e. always.
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
            task->clock[type].tv_nsec += delta;              \
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

    scoped_lock(&current_cpu->sched_lock) {

        if (likely(current_task->status == TASK_STATUS_RUNNING)) {
            current_task->status = TASK_STATUS_READY;
        }

        if (likely((current_task->flags & TASK_FLAGS_NO_FRAME) == 0)) {
            __sched_next();
        }


        current_task->status = TASK_STATUS_RUNNING;

        arch_task_switch(prev_task, current_task);

    }

    do_signals();
}


/**
 * @brief Unlinks a task from one CPU's run queue, with that CPU's sched_lock held.
 *
 * @param cpu The CPU whose queue to search.
 * @param task The task to remove.
 * @return true if the task was on this queue and has been removed.
 */
static bool __sched_unlink(cpu_t* cpu, task_t* task) {

    DEBUG_ASSERT(cpu);
    DEBUG_ASSERT(task);


    if (task == cpu->sched_queue) {

        cpu->sched_queue = task->next;

    } else {

        task_t* tmp = cpu->sched_queue;

        for (; tmp && tmp->next != task; tmp = tmp->next) {
            ;
        }

        if (!tmp) {
            return false;
        }

        tmp->next = task->next;
    }

    task->next = NULL;

    cpu->sched_count--;

    return true;
}


/**
 * @brief Waits until no CPU is running the given task any more.
 *
 * @param task The task to wait for, already unlinked from every run queue.
 */
static void __sched_wait_quiesced(const task_t* task) {

    DEBUG_ASSERT(task);


    for (;;) {

        bool running = false;

        cpu_foreach(cpu) {

            scoped_lock(&cpu->sched_lock) {
                running |= (cpu->sched_running == task);
            }
        }

        if (!running) {
            return;
        }

#if defined(__i386__) || defined(__x86_64__)
        __builtin_ia32_pause();
#endif
    }
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
 * @brief Dequeues a task from its assigned CPU and destroys it.
 *
 * Only the caller that actually unlinks the task destroys it, so two threads of the same
 * process reaping the same zombie at once free it once between them.
 *
 * A zombie becomes reapable before the CPU it died on has finished with it -- sys_exit()
 * still has the rest of its own syscall to return through, on the kernel stack freed here
 * -- so the task is unlinked, waited out, and only then destroyed. The destruction is
 * deliberately outside the queue lock: it frees the kernel stack and the task itself,
 * which takes locks of its own, and by then nothing can reach the task to need protecting
 * from it.
 *
 * @param task The task to be dequeued
 */
void sched_dequeue(task_t* task) {

    bool found = false;

    cpu_foreach_if(cpu, !found) {

        scoped_lock(&cpu->sched_lock) {
            found = __sched_unlink(cpu, task);
        }
    }

#if DEBUG_LEVEL_TRACE
    kprintf("sched: dequeued task(%d) %s\n", task->tid, task->argv[0]);
#endif

    if (!found) {
        return;
    }

    __sched_wait_quiesced(task);

    arch_task_destroy(task);
}


/**
 * @brief Moves a task to the front of the queue it is already on.
 *
 * @param task The task to be requeued.
 */
void sched_requeue(task_t* task) {

    bool found = false;

    cpu_foreach_if(cpu, !found) {

        scoped_lock(&cpu->sched_lock) {

            if ((found = __sched_unlink(cpu, task))) {

                if (cpu->sched_running != task && cpu->sched_running != cpu->sched_idle) {

                    task->next = cpu->sched_running->next;

                    cpu->sched_running->next = task;

                } else {

                    task->next = cpu->sched_queue;

                    cpu->sched_queue = task;
                }

                cpu->sched_count++;
            }
        }
    }

#if DEBUG_LEVEL_TRACE
    kprintf("sched: requeued task(%d) %s\n", task->tid, task->argv[0]);
#endif
}



/**
 * @brief Queues a signal on every task matching a process group, a process or a thread.
 *
 * @param pgrp The process group to match, or -1 not to narrow by it.
 * @param pid The process to match, or -1 not to narrow by it.
 * @param tid The thread to match, or -1 not to narrow by it.
 * @param sig The signal to queue.
 * @param info The signal's payload.
 * @return 0 on success, or -1 with errno set.
 */
int sched_sigqueueinfo(pid_t pgrp, pid_t pid, pid_t tid, int sig, siginfo_t* info) {

    DEBUG_ASSERT(sig >= 0);
    DEBUG_ASSERT(sig < NSIG - 1);
    DEBUG_ASSERT(info);


    size_t found = 0;

    cpu_foreach(cpu) {

        scoped_lock(&cpu->sched_lock) {

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


            bool unstoppable = (sig == SIGKILL || sig == SIGSTOP);


            if (unlikely(!unstoppable && action->handler == SIG_IGN)) {
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
                    if (unlikely(!unstoppable && sigset_is_member(&sighand->sigmask, sig)))
                        queue_enqueue(&tmp->sigpending, siginfo, 0);
                    else
                        queue_enqueue(&tmp->sigqueue, siginfo, 0);
                });
            }
        }
    }


    if (unlikely(found == 0)) {
        return errno = ESRCH, -1;
    }

    return 0;
}


/**
 * @brief Queues a signal raised by a hardware fault on the current task, forcing its default action.
 *
 * The signal is unblocked and its disposition reset when it would otherwise be ignored or deferred,
 * and a signal of the same number already queued is not queued twice.
 *
 * @param sig The signal the fault maps to.
 * @param info The signal's payload.
 * @return 0 on success, or -1 with errno set.
 */
int sched_fault_sigqueueinfo(int sig, siginfo_t* info) {

    DEBUG_ASSERT(current_task);
    DEBUG_ASSERT(sig > 0);
    DEBUG_ASSERT(sig < NSIG - 1);
    DEBUG_ASSERT(info);


    if (unlikely(current_task->status == TASK_STATUS_ZOMBIE)) {
        return 0;
    }


    bool pending = false;

    scoped_lock(&current_task->sigqueue.lock) {

        for (struct queue_element* e = current_task->sigqueue.head; e; e = e->next) {

            if (e->element && ((siginfo_t*)e->element)->si_signo == sig) {

                pending = true;
                break;
            }
        }
    }

    if (pending) {
        return 0;
    }


    shared_ptr_access(current_task->sighand, sighand, {
        if (sighand->action[sig].handler == SIG_IGN || sighand->action[sig].handler == SIG_ERR) {
            sighand->action[sig].handler = SIG_DFL;
        }

        sigset_del(&sighand->sigmask, sig);
    });


    siginfo_t* siginfo = (siginfo_t*)kcalloc(1, sizeof(siginfo_t), GFP_KERNEL);

    if (unlikely(!siginfo)) {
        return errno = ENOMEM, -1;
    }

    memcpy(siginfo, info, sizeof(siginfo_t));

    siginfo->si_signo = sig;

    queue_enqueue(&current_task->sigqueue, siginfo, 0);

    return 0;
}


/**
 * @brief Generates the next unique process ID
 *
 * This function generates and returns the next unique process ID, by incrementing a static variable.
 *
 * @return The next unique process ID
 */
/**
 * @brief The last process id handed out, allocated from atomically because two CPUs draw on it at once.
 */
static atomic_int __sched_lastpid = 0;

pid_t sched_nextpid(void) {
    return (pid_t)(atomic_fetch_add(&__sched_lastpid, 1) + 1);
}

/**
 * @brief Returns the most recently allocated process ID, without allocating one.
 *
 * @return The last process ID handed out by sched_nextpid().
 */
pid_t sched_lastpid(void) {
    return (pid_t)atomic_load(&__sched_lastpid);
}

/**
 * @brief Returns the total number of processes across all CPUs
 * 
 * @return The total number of processes across all CPUs
 */
size_t sched_nprocs(void) {

    size_t count = 0;

    cpu_foreach(cpu) {
        count += cpu->sched_count;
    }

    return count;
}