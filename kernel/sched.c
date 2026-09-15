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
 * @brief Wake a sleeping task whose deadline has come due, and report the time it has left.
 *
 * The clock is read through scoped_uio_kernel(): &t0 is a kernel buffer while the task being
 * looked at is a userspace one, so without saying so the check rejects the pointer,
 * sys_clock_gettime() returns -EFAULT having written nothing, and the deadline is compared
 * against whatever the stack happened to hold. The time left is computed as an unsigned
 * difference, so an already-due deadline has to be reported as none left rather than wrapping
 * into a near-eternal one.
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
 * @brief Carry out a signal's default disposition.
 *
 * The three groups are the ones POSIX names: the first terminates, the second terminates and
 * dumps core, the third stops. Anything not listed has no default action here.
 *
 * @param siginfo   The signal being delivered.
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
 * @brief Deliver one pending signal to the current task.
 *
 * A fatal signal runs the whole of sys_exit() right here, wherever this task happened to be
 * inside the kernel -- and sys_exit() closes every descriptor and tears down the address space
 * before returning normally. Returning would resume the interrupted kernel path with the
 * resources it was in the middle of using already freed: a task killed while parked in the
 * network stack came back into lwIP holding a netconn and a mailbox its own exit had released.
 *
 * So a dead task gives the CPU away and does not come back. A stopped one may: __sched_next()
 * passes over it until SIGCONT makes it READY, at which point this returns and the interrupted
 * path carries on -- safe, because sys_exit() leaves a stopped task's descriptors alone.
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
 * @brief Unlink a task from one CPU's run queue.
 *
 * The caller must hold @p cpu's sched_lock -- which is also what makes the task safe to
 * touch at all, since this is the only place a task leaves a queue and sched_dequeue()
 * destroys it immediately afterwards. The walk carries the node itself as its cursor, so
 * that unlinking the tail is told apart from running off the end.
 *
 * @param cpu   The CPU whose queue to search.
 * @param task  The task to remove.
 *
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
 * @brief Wait until no CPU is running the given task any more.
 *
 * Only meaningful for a task that has already been unlinked from every run queue: nothing
 * can select it again from there, so once a CPU has moved off it, it stays off it.
 * sched_running is read under each CPU's queue lock, which is what schedule() updates it
 * under.
 *
 * @param task  The task to wait for.
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
 * The unlink and the relink share one critical section: between the two the task belongs to
 * no queue at all, and that is exactly when the CPU owning the queue may be walking it.
 *
 * @param task The task to be requeued
 */
void sched_requeue(task_t* task) {

    bool found = false;

    cpu_foreach_if(cpu, !found) {

        scoped_lock(&cpu->sched_lock) {

            if ((found = __sched_unlink(cpu, task))) {

                if (cpu->sched_running != task) {

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



//? pgrp, pid and tid are each either a thing to match or -1 for "do not narrow by this". That
//? sentinel is why they have to be signed: pgrp used to be a gid_t, which is unsigned, so the -1
//? every caller passes arrived as a huge positive number, `pgrp > 0` was always true, and no task
//? ever matched a process group. Nothing could be signalled at all.
//?
//? Each run queue is held for the whole of its walk, not merely to read the head:
//? sched_dequeue() unlinks and then frees a task under that same lock, so without it a
//? reaper on another CPU can hand a node back to the heap between one `tmp->next` and the
//? next.
//?
//? SIGKILL and SIGSTOP cannot be caught, ignored or blocked. rt_sigaction() and
//? rt_sigprocmask() both refuse to set that up, but the guarantee is enforced here as well:
//? this is the only path a signal reaches a task by, and a disposition or mask that got set
//? some other way would otherwise make a process unkillable.
//?
//? A blocked signal waits, whatever its action says. SA_NODEFER decides the mask a handler
//? runs under -- whether the signal is added on entry to its own handler -- and says nothing
//? about whether sigprocmask() may hold it back, so honouring it here delivers signals the
//? thread had explicitly blocked.
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
 * @brief Generates the next unique process ID
 *
 * This function generates and returns the next unique process ID, by incrementing a static variable.
 *
 * @return The next unique process ID
 */
//? Atomic because two CPUs allocate from it at once. A plain ++ is a load, an add and a
//? store, so two concurrent fork()s used to hand out the same number -- and a duplicated
//? tid is not merely a cosmetic problem: spinlock_get_new_owner() identifies the holder of
//? a task-owned spinlock by tid, so the twin on the other CPU is taken for the owner and
//? either walks into the critical section or panics with a DEADLOCK that is not one.
static atomic_int __sched_lastpid = 0;

pid_t sched_nextpid(void) {
    return (pid_t)(atomic_fetch_add(&__sched_lastpid, 1) + 1);
}

/**
 * @brief Returns the most recently allocated process ID, without allocating one.
 *
 * Reported by /proc/loadavg as its last-pid field.
 *
 * @return The last process ID handed out by sched_nextpid()
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