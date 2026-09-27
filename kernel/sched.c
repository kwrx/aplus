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

        futex_release_all(current_task);
        thread_wake(current_task);

        break;
    }
}

/**
 * @brief Bumped whenever a task exits, stops or continues, and watched by wait4() as a futex word.
 */
volatile uint32_t sched_child_event = 0;


/**
 * @brief Wakes the current task if its sleep deadline has come due.
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


        if (tss <= tsc) {

            current_task->sleep.timeout.tv_sec  = 0L;
            current_task->sleep.timeout.tv_nsec = 0L;
            current_task->sleep.expired         = true;

            thread_wake(current_task);
        }
    }
}


/**
 * @brief Asks whether a signal queue holds a signal of the given number.
 *
 * @param queue The queue to search.
 * @param sig The signal to look for.
 * @return true if it is queued.
 */
static bool __sigqueue_holds(queue_t* queue, int sig) {

    bool found = false;

    scoped_lock(&queue->lock) {

        for (struct queue_element* e = queue->head; e && !found; e = e->next)
            found = e->element && ((siginfo_t*)e->element)->si_signo == sig;
    }

    return found;
}


/**
 * @brief Tells a parent that one of its children exited, stopped or continued, with SIGCHLD and a child event.
 *
 * @param ppid The parent's pid, or 0 if there is nobody to tell.
 * @param pid The child.
 * @param uid The child's user.
 * @param code CLD_EXITED, CLD_KILLED, CLD_DUMPED, CLD_STOPPED or CLD_CONTINUED.
 * @param status The exit status, or the signal that stopped, continued or killed the child.
 */
static void __sched_child_changed(pid_t ppid, pid_t pid, uid_t uid, int code, int status) {

    if (ppid > 0) {

        siginfo_t info;

        memset(&info, 0, sizeof(info));

        info.si_signo  = SIGCHLD;
        info.si_code   = code;
        info.si_pid    = pid;
        info.si_uid    = uid;
        info.si_status = status;

        sched_sigqueue(-1, ppid, -1, SIGCHLD, &info, SCHED_SIGQUEUE_KERNEL);
    }

    atomic_fetch_add(&sched_child_event, 1);
}


/**
 * @brief Stops the current task for a stop signal, unless a SIGCONT is already queued behind it.
 *
 * @param signo The stop signal.
 */
static void do_stop(int signo) {

    bool stopped = false;
    pid_t ppid   = 0;

    scoped_lock(&current_cpu->sched_lock) {

        if (!__sigqueue_holds(&current_task->sigqueue, SIGCONT)) {

            current_task->exit.value     = (signo << 8) | 0x7F;
            current_task->status         = TASK_STATUS_STOP;
            current_task->wait_stopped   = true;
            current_task->wait_continued = false;

            stopped = true;
            ppid    = current_task->ppid;
        }
    }

    if (stopped && current_task->tid == current_task->pid)
        __sched_child_changed(ppid, current_task->pid, current_task->uid, CLD_STOPPED, signo);
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
            do_stop(siginfo->si_signo);
            break;
    }
}



static void handle_user_signal(siginfo_t* siginfo, struct ksigaction* action) {

    syscall_interrupt();

    arch_task_prepare_to_signal(siginfo);

    if (action->sa_flags & SA_RESETHAND) {
        action->handler = SIG_DFL;
    }
}


static void handle_default_or_user_signal(siginfo_t* siginfo) {

    struct ksigaction* action = NULL;

    shared_ptr_nullable_access(current_task->sighand, sighand, { action = &sighand->action[siginfo->si_signo]; });


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
            do_stop(SIGSTOP);
            break;

        default:
            handle_default_or_user_signal(siginfo);
            break;
    }
}


/**
 * @brief Delivers one pending signal to the current task.
 *
 * A fatal signal exits here and never returns; a stopped task returns once SIGCONT makes it READY. Nothing is
 * delivered to a parent parked in vfork(), since its child is still running on its stack, to a task already in the
 * middle of exiting, nor to a task parked inside the kernel.
 */
static inline void do_signals(void) {

    DEBUG_ASSERT(current_task);

    if (queue_is_empty(&current_task->sigqueue)) {
        return;
    }

    if (unlikely(current_task->vfork.pending || current_task->sighand == NULL)) {
        return;
    }

    if (unlikely(arch_task_parked_in_kernel(current_task, true))) {
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


        while (unlikely(current_task->status == TASK_STATUS_ZOMBIE || current_task->status == TASK_STATUS_STOP || current_task->status == TASK_STATUS_DEAD))
            schedule(1);
    }
}


/**
 * @brief Destroys the dead tasks of the current cpu that it is no longer running on.
 *
 * A dead task is kept while it is still the current one, or while the cpu is still executing on its
 * kernel stack, as the tail of the interrupt that switched away from it does.
 */
static void __sched_destroy_dead(void) {

    if (likely(current_cpu->sched_dead == NULL))
        return;


    task_t* dead = NULL;

    scoped_lock(&current_cpu->sched_lock) {

        dead = current_cpu->sched_dead;

        current_cpu->sched_dead = NULL;
    }

    while (dead) {

        task_t* next = dead->next;

        if (dead == current_task || arch_task_stack_in_use(dead)) {

            scoped_lock(&current_cpu->sched_lock) {

                dead->next = current_cpu->sched_dead;

                current_cpu->sched_dead = dead;
            }

        } else {

            arch_task_destroy(dead);
        }

        dead = next;
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
 * head, which is itself NULL on a cpu with nothing enqueued. So does a lap that starts on a dead task,
 * whose next links the dead list instead.
 */
static void __sched_next(void) {

    task_t* prev = current_task;
    task_t* next = (prev->next && prev->status != TASK_STATUS_DEAD) ? prev->next : current_cpu->sched_queue;

    for (size_t i = current_cpu->sched_count; i && next; i--, next = next->next ? next->next : current_cpu->sched_queue) {

        current_task = next;

        if (current_task->status == TASK_STATUS_SLEEP) {

            if (!queue_is_empty(&current_task->sigqueue) && !current_task->vfork.pending && current_task->sighand && !arch_task_parked_in_kernel(current_task, current_task == prev)) {
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
 * This function first destroys the dead tasks this cpu has finished with, then updates the clocks of the current task.
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

#define UPDATE_CLOCK(task, type, delta)                                      \
    {                                                                        \
        task->clock[type].tv_nsec += delta;                                  \
        task->clock[type].tv_sec += task->clock[type].tv_nsec / 1000000000L; \
        task->clock[type].tv_nsec %= 1000000000L;                            \
    }



    __sched_destroy_dead();


    task_t* prev_task = current_task;


    uint64_t elapsed = arch_timer_percpu_getns();
    uint64_t delta   = elapsed - current_cpu->ticks;


    UPDATE_CLOCK(current_task, TASK_CLOCK_SCHEDULER, TASK_SCHEDULER_PERIOD_NS);
    UPDATE_CLOCK(current_task, TASK_CLOCK_THREAD_CPUTIME, delta);
    UPDATE_CLOCK(current_task, TASK_CLOCK_PROCESS_CPUTIME, delta);


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

        if (unlikely(current_task->futexes != NULL))
            futex_release_all(current_task);

        arch_task_switch(prev_task, current_task);

        if (unlikely(prev_task != current_task && prev_task->status == TASK_STATUS_ZOMBIE && (prev_task->flags & TASK_FLAGS_AUTOREAP)))
            sched_bury(current_cpu, prev_task);
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
 * @brief Unlinks a zombie from a cpu's run queue and queues it to be destroyed by that cpu.
 *
 * The task is destroyed by the cpu it ran on, and only once that cpu is off its kernel stack.
 * @see __sched_destroy_dead().
 *
 * @param cpu The cpu the task is queued on, whose sched_lock the caller holds.
 * @param task The task to reap.
 */
void sched_bury(cpu_t* cpu, task_t* task) {

    DEBUG_ASSERT(cpu);
    DEBUG_ASSERT(task);

    if (!__sched_unlink(cpu, task))
        return;

    task->status = TASK_STATUS_DEAD;
    task->next   = cpu->sched_dead;

    cpu->sched_dead = task;

#if DEBUG_LEVEL_TRACE
    kprintf("sched: buried task(%d) %s\n", task->tid, task->argv[0]);
#endif
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
 * @brief Asks whether a signal's default action is to be ignored.
 *
 * @param sig The signal.
 * @return true for SIGCHLD, SIGCONT, SIGURG and SIGWINCH.
 */
static inline bool __sig_default_ignored(int sig) {
    return sig == SIGCHLD || sig == SIGCONT || sig == SIGURG || sig == SIGWINCH;
}


/**
 * @brief Queues a signal on every task matching a process group, a process or a thread.
 *
 * Zombies are not matched. Kernel threads, and init for any signal it has not asked to handle, are
 * matched but left alone. SIGCONT and SIGKILL resume a stopped task before anything is queued, a
 * signal that would only be ignored on arrival is dropped rather than queued, and a standard signal
 * already pending on a thread that blocks it is not queued twice.
 *
 * @param pgrp The process group to match, or -1 not to narrow by it.
 * @param pid The process to match, or -1 not to narrow by it.
 * @param tid The thread to match, or -1 not to narrow by it.
 * @param sig The signal to queue.
 * @param info The signal's payload.
 * @param flags SCHED_SIGQUEUE_KERNEL and SCHED_SIGQUEUE_BROADCAST.
 * @return 0 on success, or -1 with errno set.
 */
int sched_sigqueue(pid_t pgrp, pid_t pid, pid_t tid, int sig, siginfo_t* info, int flags) {

    DEBUG_ASSERT(sig >= 0);
    DEBUG_ASSERT(sig < _NSIG);
    DEBUG_ASSERT(info);


    struct {
        pid_t ppid;
        pid_t pid;
        uid_t uid;
    } continued[8];

    size_t ncontinued = 0;
    size_t found      = 0;
    int error         = 0;


    cpu_foreach(cpu) {

        scoped_lock(&cpu->sched_lock) {

            for (task_t* tmp = cpu->sched_queue; tmp; tmp = tmp->next) {

                if (pgrp > 0 && tmp->pgrp != pgrp)
                    continue;

                if (pid > 0 && tmp->pid != pid)
                    continue;

                if (tid > 0 && tmp->tid != tid)
                    continue;

                if (tmp->status == TASK_STATUS_ZOMBIE || tmp->status == TASK_STATUS_DEAD)
                    continue;

                if ((flags & SCHED_SIGQUEUE_BROADCAST) && (tmp->pid == 1 || tmp->pid == current_task->pid))
                    continue;

                if (!(flags & SCHED_SIGQUEUE_KERNEL) && !(current_task->euid == tmp->uid || current_task->uid == tmp->uid))
                    continue;


                found++;

                if (unlikely(sig == 0))
                    continue;

                if (unlikely(tmp->flags & TASK_FLAGS_KTHREAD))
                    continue;

                if (unlikely(tmp->sighand == NULL))
                    continue;


                bool unstoppable = (sig == SIGKILL || sig == SIGSTOP);
                bool blocked     = !unstoppable && sigset_is_member(&tmp->sigmask, sig);

                void (*handler)(int) = SIG_DFL;
                long sa_flags        = 0;

                shared_ptr_nullable_access(tmp->sighand, sighand, {
                    handler  = sighand->action[sig].handler;
                    sa_flags = sighand->action[sig].sa_flags;
                });


                if (unlikely(tmp->pid == 1 && (unstoppable || handler == SIG_DFL || handler == SIG_IGN)))
                    continue;


                if ((sig == SIGCONT || sig == SIGKILL) && tmp->status == TASK_STATUS_STOP) {

                    tmp->status = TASK_STATUS_READY;

                    if (sig == SIGCONT) {

                        tmp->wait_stopped   = false;
                        tmp->wait_continued = true;

                        if (tmp->tid == tmp->pid && ncontinued < sizeof(continued) / sizeof(continued[0])) {

                            continued[ncontinued].ppid = tmp->ppid;
                            continued[ncontinued].pid  = tmp->pid;
                            continued[ncontinued].uid  = tmp->uid;

                            ncontinued++;
                        }
                    }
                }


                if (unlikely(handler == SIG_ERR))
                    continue;

                if (!unstoppable && handler == SIG_IGN)
                    continue;

                if (!unstoppable && !blocked && handler == SIG_DFL && __sig_default_ignored(sig))
                    continue;

                if (sig == SIGCHLD && (sa_flags & SA_NOCLDSTOP) && (info->si_code == CLD_STOPPED || info->si_code == CLD_CONTINUED))
                    continue;

                if (blocked && sig < 32 && __sigqueue_holds(&tmp->sigpending, sig))
                    continue;

                if (!(flags & SCHED_SIGQUEUE_KERNEL) && tmp->sigqueue.size > tmp->rlimits[RLIMIT_SIGPENDING].rlim_cur) {
                    error = EAGAIN;
                    continue;
                }


                siginfo_t* siginfo = (siginfo_t*)kcalloc(1, sizeof(siginfo_t), GFP_KERNEL);

                if (unlikely(!siginfo)) {
                    error = ENOMEM;
                    continue;
                }

                memcpy(siginfo, info, sizeof(siginfo_t));

                siginfo->si_signo = sig;

                queue_enqueue(blocked ? &tmp->sigpending : &tmp->sigqueue, siginfo, 0);
            }
        }
    }


    for (size_t i = 0; i < ncontinued; i++)
        __sched_child_changed(continued[i].ppid, continued[i].pid, continued[i].uid, CLD_CONTINUED, SIGCONT);


    if (unlikely(found == 0)) {
        return errno = ESRCH, -1;
    }

    if (unlikely(error)) {
        return errno = error, -1;
    }

    return 0;
}


/**
 * @brief Queues a signal on behalf of the current task, which must be allowed to signal each target.
 *
 * @param pgrp The process group to match, or -1 not to narrow by it.
 * @param pid The process to match, or -1 not to narrow by it.
 * @param tid The thread to match, or -1 not to narrow by it.
 * @param sig The signal to queue.
 * @param info The signal's payload.
 * @return 0 on success, or -1 with errno set.
 */
int sched_sigqueueinfo(pid_t pgrp, pid_t pid, pid_t tid, int sig, siginfo_t* info) {
    return sched_sigqueue(pgrp, pid, tid, sig, info, 0);
}


/**
 * @brief Queues a signal the current task brought on itself, to be delivered before its syscall returns.
 *
 * @param sig The signal.
 * @param info The signal's payload.
 */
void sched_raise(int sig, siginfo_t* info) {

    DEBUG_ASSERT(current_task);

    if (sched_sigqueue(-1, -1, current_task->tid, sig, info, SCHED_SIGQUEUE_KERNEL) == 0)
        current_task->flags |= TASK_FLAGS_SIGNALED;
}


/**
 * @brief Replaces the current thread's blocked-signal mask, and makes deliverable the pending signals it unblocks.
 *
 * The run-queue lock is held throughout, as it is while a signal is queued, so a signal cannot be parked as blocked
 * just after the move that would have released it.
 *
 * @param set The new mask. SIGKILL and SIGSTOP are never blocked.
 */
void sched_sigmask(const sigset_t* set) {

    DEBUG_ASSERT(current_task);
    DEBUG_ASSERT(set);

    scoped_lock(&current_cpu->sched_lock) {

        memcpy(&current_task->sigmask, set, sizeof(sigset_t));

        sigset_del(&current_task->sigmask, SIGKILL);
        sigset_del(&current_task->sigmask, SIGSTOP);

        for (size_t i = current_task->sigpending.size; i > 0; i--) {

            siginfo_t* info = (siginfo_t*)queue_pop(&current_task->sigpending);

            if (!info)
                break;

            queue_enqueue(sigset_is_member(&current_task->sigmask, info->si_signo) ? &current_task->sigpending : &current_task->sigqueue, info, 0);
        }
    }
}


/**
 * @brief Delivers a pending signal to the current task now, rather than at its next reschedule.
 */
void sched_signals(void) {
    do_signals();
}


/**
 * @brief Turns the current task into a zombie, and settles what that means for its group, its parent and its children.
 *
 * The thread that finds every other thread of its group already dead is the last one out. It makes the group's
 * leader reportable to wait4(), or reaps it at once when the parent ignores SIGCHLD, hands the group's children to
 * init, and tells the parent. More than one thread can conclude it is the last, which is harmless, but because each
 * publishes its own death before looking, at least one always does.
 */
void sched_exit(void) {

    task_t* self = current_task;
    pid_t ppid   = 0;


    scoped_lock(&current_cpu->sched_lock) {

        self->status = TASK_STATUS_ZOMBIE;

        if (self->tid != self->pid)
            self->flags |= TASK_FLAGS_AUTOREAP;

        ppid = self->ppid;
    }


    bool alive  = false;
    bool ignore = false;

    cpu_foreach(cpu) {

        scoped_lock(&cpu->sched_lock) {

            for (task_t* t = cpu->sched_queue; t; t = t->next) {

                if (t != self && t->pid == self->pid && t->status != TASK_STATUS_ZOMBIE && t->status != TASK_STATUS_DEAD)
                    alive = true;

                if (ppid > 0 && t->pid == ppid) {

                    shared_ptr_nullable_access(t->sighand, sighand, {
                        ignore |= sighand->action[SIGCHLD].handler == SIG_IGN || (sighand->action[SIGCHLD].sa_flags & SA_NOCLDWAIT);
                    });
                }
            }
        }
    }

    if (alive)
        return;


    int value = self->exit.value & 0xFFFF;

    cpu_foreach(cpu) {

        scoped_lock(&cpu->sched_lock) {

            task_t* next = NULL;

            for (task_t* t = cpu->sched_queue; t; t = next) {

                next = t->next;

                if (t->ppid == self->pid)
                    t->ppid = 1;

                if (t->pid != self->pid || t->tid != t->pid)
                    continue;

                value = t->exit.value & 0xFFFF;

                if (ignore)
                    sched_bury(cpu, t);
                else
                    t->group_dead = true;
            }
        }
    }


    if (WIFSIGNALED(value))
        __sched_child_changed(ppid, self->pid, self->uid, WCOREDUMP(value) ? CLD_DUMPED : CLD_KILLED, WTERMSIG(value));
    else
        __sched_child_changed(ppid, self->pid, self->uid, CLD_EXITED, WEXITSTATUS(value));
}


/**
 * @brief Kills every other thread of the current process, each reporting the given status rather than SIGKILL.
 *
 * SIGKILL is queued directly, past the limits and dispositions that could otherwise drop it.
 *
 * @param value The wait status the process exits with.
 */
void sched_group_exit(int value) {

    cpu_foreach(cpu) {

        scoped_lock(&cpu->sched_lock) {

            for (task_t* t = cpu->sched_queue; t; t = t->next) {

                if (t == current_task || t->pid != current_task->pid)
                    continue;

                if (t->flags & TASK_FLAGS_KTHREAD)
                    continue;

                if (t->status == TASK_STATUS_DEAD)
                    continue;

                if (t->status == TASK_STATUS_ZOMBIE) {

                    if (t->tid == t->pid)
                        t->exit.value = value;

                    continue;
                }


                t->exit_group_pending = true;
                t->exit_group_value   = value;

                if (t->status == TASK_STATUS_STOP)
                    t->status = TASK_STATUS_READY;


                siginfo_t* siginfo = (siginfo_t*)kcalloc(1, sizeof(siginfo_t), GFP_KERNEL);

                if (unlikely(!siginfo))
                    continue;

                siginfo->si_signo = SIGKILL;
                siginfo->si_code  = SI_KERNEL;
                siginfo->si_pid   = current_task->pid;
                siginfo->si_uid   = current_task->uid;

                queue_enqueue(&t->sigqueue, siginfo, 0);
            }
        }
    }
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


    if (unlikely(current_task->status == TASK_STATUS_ZOMBIE || current_task->status == TASK_STATUS_DEAD)) {
        return 0;
    }

    if (__sigqueue_holds(&current_task->sigqueue, sig)) {
        return 0;
    }


    shared_ptr_access(current_task->sighand, sighand, {
        if (sighand->action[sig].handler == SIG_IGN || sighand->action[sig].handler == SIG_ERR) {
            sighand->action[sig].handler = SIG_DFL;
        }
    });

    sigset_t mask = current_task->sigmask;

    sigset_del(&mask, sig);
    sched_sigmask(&mask);


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