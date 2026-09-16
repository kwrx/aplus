/*
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
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

#ifndef _APLUS_TASK_H
#define _APLUS_TASK_H

#ifndef __ASSEMBLY__

    #include <sched.h>

    #include <signal.h>
    #include <stdint.h>
    #include <time.h>

    #include <sys/resource.h>
    #include <sys/times.h>

    #include <sys/types.h>

    #include <aplus.h>
    #include <aplus/debug.h>
    #include <aplus/ipc.h>
    #include <aplus/memory.h>
    #include <aplus/smp.h>
    #include <aplus/vfs.h>

    #include <aplus/utils/list.h>
    #include <aplus/utils/queue.h>



    #define TASK_STATUS_READY   0
    #define TASK_STATUS_RUNNING 1
    #define TASK_STATUS_SLEEP   2
    #define TASK_STATUS_STOP    3
    #define TASK_STATUS_ZOMBIE  4
    #define TASK_STATUS_DEAD    5


    #define TASK_PRIO_MAX -20
    #define TASK_PRIO_MIN 19
    #define TASK_PRIO_REG 0


    #define TASK_POLICY_RR    0
    #define TASK_POLICY_BATCH 1
    #define TASK_POLICY_IDLE  2


    #define TASK_FLAGS_NO_FRAME             1
    #define TASK_FLAGS_NO_FPU               2
    #define TASK_FLAGS_NEED_RESCHED         4
    #define TASK_FLAGS_SIGNALED             8
    #define TASK_FLAGS_NEED_SYSCALL_RESTART 16

/**
 * @brief The task is kernel code invoking a sys_* entry point directly, so the pointers it passes are kernel ones.
 */
    #define TASK_FLAGS_KERNEL_UIO 32


    #define TASK_CAPS_SYSTEM  255
    #define TASK_CAPS_IO      2
    #define TASK_CAPS_NETWORK 4


    #define TASK_CLOCK_MAX             3
    #define TASK_CLOCK_SCHEDULER       0
    #define TASK_CLOCK_THREAD_CPUTIME  1
    #define TASK_CLOCK_PROCESS_CPUTIME 2


    #define TASK_SCHEDULER_PERIOD_NS 1000000ULL

/**
 * @brief Linux's length for a task name, and what /proc/<pid>/stat's comm field is expected to fit.
 */
    #define TASK_COMM_LEN    16
    #define TASK_CMDLINE_LEN 256

/**
 * @brief The units /proc reports CPU times in, which musl's sysconf(_SC_CLK_TCK) hardcodes to 100.
 */
    #define TASK_USER_HZ 100

    #define TASK_STACK_MAX (0x100000000ULL) // 4GiB
    #define TASK_STACK_MIN (0x1000ULL)      // 4KiB



struct pty;

struct fd_descriptor {

    struct file* ref;

    struct {
        int flags : 30;
        unsigned int close_on_exec : 1;
    };
};

struct fd {
    struct fd_descriptor descriptors[CONFIG_OPEN_MAX];
};


struct fs {

    inode_t* root;
    inode_t* cwd;
    inode_t* exe;

    mode_t umask;
};


struct ksigaction {

    union {
        void (*handler)(int);
        void (*sigaction)(int, siginfo_t*, void*);
    };

    long sa_flags;
    void (*sa_restorer)(void);

    int sa_mask[2];
};


struct kclone_args {

    uint64_t flags;        /* Flags bit mask                                           */
    uint64_t pidfd;        /* Where to store PID file descriptor (pid_t *)             */
    uint64_t child_tid;    /* Where to store child TID, in child's memory (pid_t *)    */
    uint64_t parent_tid;   /* Where to store child TID, in parent's memory (int *)     */
    uint64_t exit_signal;  /* Signal to deliver to parent on child termination         */
    uint64_t stack;        /* Pointer to lowest byte of stack                          */
    uint64_t stack_size;   /* Size of stack                                            */
    uint64_t tls;          /* Location of new TLS                                      */
    uint64_t set_tid;      /* Pointer to a pid_t array                                 */
    uint64_t set_tid_size; /* Number of elements in set_tid                            */
};


struct sighand {

    struct ksigaction action[_NSIG];
    sigset_t sigmask;
    size_t refcount;
};


/**
 * @brief The size of a sigset_t in bytes and in words, which the syscalls and the walks need respectively.
 */
    #define SIGSET_BITS_PER_WORD (8 * sizeof(unsigned long))
    #define SIGSET_WORDS         (sizeof(sigset_t) / sizeof(unsigned long))


/**
 * @brief Asks whether a signal is a member of a set.
 *
 * @param set The set to test.
 * @param signo The signal to look for, numbered from 1.
 * @return true if the signal is in the set.
 */
static inline bool sigset_is_member(const sigset_t* set, int signo) {

    DEBUG_ASSERT(set);

    if (unlikely(signo < 1 || signo >= _NSIG))
        return false;


    size_t bit = (size_t)(signo - 1);

    return (set->__bits[bit / SIGSET_BITS_PER_WORD] & (1UL << (bit % SIGSET_BITS_PER_WORD))) != 0;
}


/**
 * @brief Adds a signal to a set.
 *
 * @param set The set to change.
 * @param signo The signal to add, numbered from 1.
 */
static inline void sigset_add(sigset_t* set, int signo) {

    DEBUG_ASSERT(set);

    if (unlikely(signo < 1 || signo >= _NSIG))
        return;


    size_t bit = (size_t)(signo - 1);

    set->__bits[bit / SIGSET_BITS_PER_WORD] |= (1UL << (bit % SIGSET_BITS_PER_WORD));
}


/**
 * @brief Removes a signal from a set.
 *
 * @param set The set to change.
 * @param signo The signal to remove, numbered from 1.
 */
static inline void sigset_del(sigset_t* set, int signo) {

    DEBUG_ASSERT(set);

    if (unlikely(signo < 1 || signo >= _NSIG))
        return;


    size_t bit = (size_t)(signo - 1);

    set->__bits[bit / SIGSET_BITS_PER_WORD] &= ~(1UL << (bit % SIGSET_BITS_PER_WORD));
}



typedef struct task {

    char** argv;
    char** environ;

    pid_t tid;
    gid_t pid;
    pid_t pgrp;

    uid_t uid;
    uid_t euid;
    gid_t gid;
    gid_t egid;

    uid_t sid;


    ssize_t status;
    ssize_t policy;
    ssize_t priority;
    ssize_t flags;
    ssize_t caps;

    cpu_set_t affinity;


    void* frame;
    void* fpu;
    void* sstack;
    void* kstack;
    void* ustack;
    vmm_address_space_t* address_space;



    struct {

        clockid_t clockid;
        struct timespec timeout;
        struct timespec* remaining;

        bool expired;

    } sleep;


    struct timespec clock[TASK_CLOCK_MAX];


    //? A vfork() borrows the parent's address space, so the two must never run
    //? at the same time: the parent is parked in do_fork() until the child has
    //? either execve()d into a space of its own or exited. `waiter` names the
    //? task parked on this one, and `futex` is the word that task watches,
    //? bumped once the loan is over.
    //?
    //? A tid rather than a pointer because the two ends can outlive each other:
    //? a parent killed while parked becomes a zombie, and whoever reaps it frees
    //? the task outright while the child still holds the link.
    //?
    //? The wait is a restarted syscall like every other one here, so `pending`
    //? and `child` carry what the first attempt already did across the restart:
    //? without them do_fork() would run again from the top and spawn a second
    //? child on every wakeup.
    struct {

        pid_t waiter;

        volatile uint32_t futex;
        bool released;

        pid_t child;
        bool pending;

    } vfork;


    list(futex_t*, futexes);
    list(struct task*, wait_queue);

    int wait_options;
    int* wait_status;
    struct rusage* wait_rusage;


    shared_ptr(struct fd) fd;
    shared_ptr(struct fs) fs;
    shared_ptr(struct sighand) sighand;
    shared_ptr(struct pty*) ctty;

    queue_t sigqueue;
    queue_t sigpending;


    struct {

        uintptr_t stack;
        uintptr_t start;
        uintptr_t end;

        uintptr_t thread_area;
        uintptr_t cpu_area;

        uintptr_t tid_address;

        uintptr_t sigstack;
        siginfo_t* siginfo;

    } userspace;


    struct {
        union {
            struct {
                int16_t o177  : 8;
                int16_t signo : 8;
            } stopped;

            struct {
                int16_t zero   : 8;
                int16_t retval : 8;
            } exited;

            struct {
                int16_t signo : 7;
                int16_t corep : 1;
                int16_t zero  : 8;
            } termed;
        };

        int16_t value : 16;
    } exit;


    struct {
        uint64_t rchar;
        uint64_t wchar;
        uint64_t syscr;
        uint64_t syscw;
        uint64_t read_bytes;
        uint64_t write_bytes;
        uint64_t cancelled_write_bytes;
    } iostat;


    struct {

        long index;
        long param0;
        long param1;
        long param2;
        long param3;
        long param4;
        long param5;

        //? A syscall that sleeps is restarted from the top with these same
        //? arguments, so a relative timeout would start over every time and
        //? never come due. Whoever sleeps stamps the absolute deadline here on
        //? the first attempt and clears it on the way out.
        struct timespec deadline;
        bool deadline_valid;

        //? A word nothing ever bumps, so a sleep with no descriptor to watch
        //? can still be woken by its deadline and nothing else.
        volatile uint32_t deadline_futex;

        //? ppoll() and pselect6() swap the blocked-signal mask for as long as
        //? they wait, so the mask to put back has to outlive the attempt that
        //? installed it. One slot, like the deadline above: a wait nested
        //? inside a signal handler would share it.
        sigset_t sigmask;
        bool sigmask_valid;

        //? The syscall a signal interrupted, snapshotted while the handler is being set up.
        //? The fields above describe whatever the task is running *now*, and by the time
        //? rt_sigreturn(2) executes that is rt_sigreturn itself -- restarting it from there
        //? restarts sigreturn, which never terminates. One slot, like the deadline and the
        //? mask: a signal taken inside a handler shares it, as it already shares sstack.
        struct {

            long index;
            long param0;
            long param1;
            long param2;
            long param3;
            long param4;
            long param5;

        } interrupted;

    } syscall;


    struct rlimit rlimits[RLIM_NLIMITS];
    struct rusage rusage;


    spinlock_t lock;
    spinlock_t sched_lock;

    struct task* parent;
    struct task* next;

    /* Reported by /proc. These are plain storage inside task_t rather than pointers into
       anything the task owns, because a task stays on the run queue as a ZOMBIE after
       sys_exit() has already freed its fs, fd, sighand and address_space -- which is
       exactly when ps comes along to read it. @see sys_exit(). */
    char comm[TASK_COMM_LEN];

    //? argv, packed NUL-separated the way /proc/<pid>/cmdline hands it out. task->argv is
    //? no use for this: it is inherited verbatim from the parent and execve never updates
    //? it, so every process claims to be whatever init was called.
    char cmdline[TASK_CMDLINE_LEN];
    size_t cmdline_len;

    //? USER_HZ ticks since boot, stamped once when the task is created.
    uint64_t start_time;

    //? The parent's tgid, copied at creation: `parent` is cleared when the parent is
    //? reaped, and following it afterwards reads freed memory.
    pid_t ppid;

} task_t;


    #define thread_restart_sched(task) task->flags |= TASK_FLAGS_NEED_RESCHED

    #define thread_restart_syscall(task) task->flags |= TASK_FLAGS_NEED_SYSCALL_RESTART

    #define thread_suspend(task) task->status = TASK_STATUS_SLEEP

    #define thread_wake(task) task->status = TASK_STATUS_READY;



__BEGIN_DECLS

struct cpu;

void do_unshare(int);
void do_vfork_release(void);
pid_t do_fork(struct kclone_args*, size_t);

pid_t sched_nextpid();
void sched_enqueue(task_t*);
void sched_dequeue(task_t*);
void sched_requeue(task_t*);
int sched_sigqueueinfo(pid_t pgrp, pid_t pid, pid_t tid, int sig, siginfo_t*);
int sched_fault_sigqueueinfo(int sig, siginfo_t*);
size_t sched_nprocs(void);
pid_t sched_lastpid(void);

void schedule(int);

__END_DECLS

#endif
#endif
