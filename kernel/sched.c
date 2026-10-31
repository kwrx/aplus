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

volatile uint32_t sched_child_event = 0;


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


static bool __sigqueue_holds(queue_t* queue, int sig) {

    bool found = false;

    scoped_lock(&queue->lock) {

        for (struct queue_element* e = queue->head; e && !found; e = e->next)
            found = e->element && ((siginfo_t*)e->element)->si_signo == sig;
    }

    return found;
}


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
            sched_die(siginfo->si_signo);
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
            sched_die(siginfo->si_signo | 0x80);
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

    if (unlikely(arch_task_prepare_to_signal(siginfo) < 0)) {
        sched_die(SIGSEGV | 0x80);
        return;
    }

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
            sched_die(SIGKILL);
            break;

        case SIGSTOP:
            do_stop(SIGSTOP);
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

    arch_vmm_reclaim(current_task->address_space);

    do_signals();
}


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


static inline bool __sig_default_ignored(int sig) {
    return sig == SIGCHLD || sig == SIGCONT || sig == SIGURG || sig == SIGWINCH;
}


static inline bool __sig_every_thread(int sig, void (*handler)(int)) {

    if (sig == SIGKILL || sig == SIGSTOP)
        return true;

    return handler == SIG_DFL && (sig == SIGTSTP || sig == SIGTTIN || sig == SIGTTOU);
}


struct sigpick {

    pid_t pid;
    pid_t tid;
    int rank;
};


static int __sigpick_consider(struct sigpick** picks, size_t* count, size_t* capacity, const task_t* task, bool blocked) {

    int rank = !blocked ? 3 : task->tid == task->pid ? 2 : 1;

    for (size_t i = 0; i < *count; i++) {

        if ((*picks)[i].pid != task->pid)
            continue;

        if (rank > (*picks)[i].rank) {

            (*picks)[i].tid  = task->tid;
            (*picks)[i].rank = rank;
        }

        return 0;
    }


    if (*count == *capacity) {

        size_t grown      = *capacity ? *capacity * 2 : 16;
        struct sigpick* p = (struct sigpick*)krealloc(*picks, grown * sizeof(struct sigpick), GFP_KERNEL);

        if (unlikely(!p))
            return ENOMEM;

        *picks    = p;
        *capacity = grown;
    }

    (*picks)[*count].pid  = task->pid;
    (*picks)[*count].tid  = task->tid;
    (*picks)[*count].rank = rank;

    (*count)++;

    return 0;
}


static int __sigqueue_on(task_t* task, int sig, const siginfo_t* info, int flags) {

    bool unstoppable = (sig == SIGKILL || sig == SIGSTOP);
    bool blocked     = !unstoppable && sigset_is_member(&task->sigmask, sig);

    void (*handler)(int) = SIG_DFL;
    long sa_flags        = 0;

    shared_ptr_nullable_access(task->sighand, sighand, {
        handler  = sighand->action[sig].handler;
        sa_flags = sighand->action[sig].sa_flags;
    });


    if (unlikely(handler == SIG_ERR))
        return 0;

    if (!unstoppable && handler == SIG_IGN)
        return 0;

    if (!unstoppable && !blocked && handler == SIG_DFL && __sig_default_ignored(sig))
        return 0;

    if (sig == SIGCHLD && (sa_flags & SA_NOCLDSTOP) && (info->si_code == CLD_STOPPED || info->si_code == CLD_CONTINUED))
        return 0;

    if (blocked && sig < 32 && __sigqueue_holds(&task->sigpending, sig))
        return 0;

    if (!(flags & SCHED_SIGQUEUE_KERNEL) && task->sigqueue.size > task->rlimits[RLIMIT_SIGPENDING].rlim_cur)
        return EAGAIN;


    siginfo_t* siginfo = (siginfo_t*)kcalloc(1, sizeof(siginfo_t), GFP_KERNEL);

    if (unlikely(!siginfo))
        return ENOMEM;

    memcpy(siginfo, info, sizeof(siginfo_t));

    siginfo->si_signo = sig;

    queue_enqueue(blocked ? &task->sigpending : &task->sigqueue, siginfo, 0);

    return 0;
}


static int __sigqueue_on_pick(const struct sigpick* pick, int sig, const siginfo_t* info, int flags) {

    pid_t want = pick->tid;

    for (int attempt = 0; attempt < 2 && want > 0; attempt++) {

        pid_t other = 0;

        cpu_foreach(cpu) {

            scoped_lock(&cpu->sched_lock) {

                for (task_t* tmp = cpu->sched_queue; tmp; tmp = tmp->next) {

                    if (tmp->pid != pick->pid || tmp->status == TASK_STATUS_ZOMBIE || tmp->status == TASK_STATUS_DEAD || tmp->sighand == NULL)
                        continue;

                    if (tmp->tid == want)
                        return __sigqueue_on(tmp, sig, info, flags);

                    if (!other && !(tmp->flags & TASK_FLAGS_KTHREAD))
                        other = tmp->tid;
                }
            }
        }

        want = other;
    }

    return 0;
}


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

    struct sigpick* picks = NULL;
    size_t npicks         = 0;
    size_t cpicks         = 0;


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

                void (*handler)(int) = SIG_DFL;

                shared_ptr_nullable_access(tmp->sighand, sighand, { handler = sighand->action[sig].handler; });


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


                int e = 0;

                if (tid > 0 || __sig_every_thread(sig, handler))
                    e = __sigqueue_on(tmp, sig, info, flags);
                else
                    e = __sigpick_consider(&picks, &npicks, &cpicks, tmp, sigset_is_member(&tmp->sigmask, sig));

                if (e)
                    error = e;
            }
        }
    }


    for (size_t i = 0; i < npicks; i++) {

        int e = __sigqueue_on_pick(&picks[i], sig, info, flags);

        if (e)
            error = e;
    }

    if (picks)
        kfree(picks);


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


int sched_sigqueueinfo(pid_t pgrp, pid_t pid, pid_t tid, int sig, siginfo_t* info) {
    return sched_sigqueue(pgrp, pid, tid, sig, info, 0);
}


void sched_raise(int sig, siginfo_t* info) {

    DEBUG_ASSERT(current_task);

    if (sched_sigqueue(-1, -1, current_task->tid, sig, info, SCHED_SIGQUEUE_KERNEL) == 0)
        current_task->flags |= TASK_FLAGS_SIGNALED;
}


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


void sched_signals(void) {
    do_signals();
}


void sched_exit(void) {

    task_t* self = current_task;
    pid_t ppid   = 0;


    if (likely(self->address_space))
        self->rusage.ru_maxrss = MAX(self->rusage.ru_maxrss, (long)((self->address_space->size_peak * PML1_PAGESIZE) >> 10));


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

                if (self->tid != self->pid && t->tid == self->pid) {

                    struct rusage usage;
                    task_rusage_self(self, &usage);

                    task_rusage_add(&t->rusage_dead, &usage);
                    task_rusage_add(&t->rusage_children, &self->rusage_children);
                }

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


void sched_die(int status) {

    if (!current_task->exit_group_pending)
        sched_group_exit(status & 0x7FFF);

    sys_exit((1U << 31) | (status & 0x7FFF));
}


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


static atomic_int __sched_lastpid = 0;

pid_t sched_nextpid(void) {
    return (pid_t)(atomic_fetch_add(&__sched_lastpid, 1) + 1);
}

pid_t sched_lastpid(void) {
    return (pid_t)atomic_load(&__sched_lastpid);
}

size_t sched_nprocs(void) {

    size_t count = 0;

    cpu_foreach(cpu) {
        count += cpu->sched_count;
    }

    return count;
}