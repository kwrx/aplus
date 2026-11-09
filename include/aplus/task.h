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
    #include <stddef.h>
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

/**
 * @brief The task is a thread nobody waits for, destroyed by its cpu as soon as it has switched away from it for good.
 */
    #define TASK_FLAGS_AUTOREAP 64

/**
 * @brief The task is a kernel thread, which user signals never reach and which owns its argv.
 */
    #define TASK_FLAGS_KTHREAD 128


    #define TASK_CAPS_SYSTEM  255
    #define TASK_CAPS_IO      2
    #define TASK_CAPS_NETWORK 4


    #define TASK_CLOCK_MAX             3
    #define TASK_CLOCK_SCHEDULER       0
    #define TASK_CLOCK_THREAD_CPUTIME  1
    #define TASK_CLOCK_PROCESS_CPUTIME 2


    /**
     * @brief Bytes of struct rusage the Linux ABI copies out; musl's struct adds reserved space after them.
     */
    #define TASK_RUSAGE_ABI_SIZE (offsetof(struct rusage, ru_nivcsw) + sizeof(long))


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

/**
 * @brief The most supplementary groups a task may hold, musl's NGROUPS_MAX.
 */
    #define TASK_NGROUPS_MAX 32



struct pty;

struct fd_descriptor {
    struct file* ref;
    bool close_on_exec;
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
    size_t refcount;
};


#define SIGSET_BITS_PER_WORD (8 * sizeof(unsigned long))
#define SIGSET_WORDS         (sizeof(sigset_t) / sizeof(unsigned long))

static inline bool sigset_is_member(const sigset_t* set, int signo) {

    DEBUG_ASSERT(set);

    if (unlikely(signo < 1 || signo >= _NSIG))
        return false;

    size_t bit = (size_t)(signo - 1);
    return (set->__bits[bit / SIGSET_BITS_PER_WORD] & (1UL << (bit % SIGSET_BITS_PER_WORD))) != 0;
}

static inline void sigset_add(sigset_t* set, int signo) {

    DEBUG_ASSERT(set);

    if (unlikely(signo < 1 || signo >= _NSIG))
        return;

    size_t bit = (size_t)(signo - 1);
    set->__bits[bit / SIGSET_BITS_PER_WORD] |= (1UL << (bit % SIGSET_BITS_PER_WORD));
}

static inline void sigset_del(sigset_t* set, int signo) {

    DEBUG_ASSERT(set);

    if (unlikely(signo < 1 || signo >= _NSIG))
        return;

    size_t bit = (size_t)(signo - 1);
    set->__bits[bit / SIGSET_BITS_PER_WORD] &= ~(1UL << (bit % SIGSET_BITS_PER_WORD));
}

struct cpu;

typedef struct task {

    char** argv;
    char** environ;

    pid_t tid;
    pid_t pid;
    pid_t pgrp;

    uid_t uid;
    uid_t euid;
    uid_t suid;
    uid_t fsuid;
    gid_t gid;
    gid_t egid;
    gid_t sgid;
    gid_t fsgid;

    pid_t sid;
    pid_t ppid;

    size_t ngroups;
    gid_t groups[TASK_NGROUPS_MAX];

    ssize_t status;
    ssize_t policy;
    ssize_t priority;
    ssize_t flags;
    ssize_t caps;

    cpu_set_t affinity;

    int errnum;
    void* frame;
    void* fpu;
    void* sigfpu;
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

    struct {
        volatile uint32_t futex;
        pid_t waiter;
        pid_t child;
        bool released;
        bool pending;
    } vfork;

    list(futex_t*, futexes);

    bool group_dead;
    bool wait_stopped;
    bool wait_continued;

    bool exit_group_pending;
    int exit_group_value;

    shared_ptr(struct fd) fd;
    shared_ptr(struct fs) fs;
    shared_ptr(struct sighand) sighand;
    shared_ptr(struct pty*) ctty;

    queue_t sigqueue;
    queue_t sigpending;
    sigset_t sigmask;

    struct {
        uintptr_t stack;
        uintptr_t thread_area;
        uintptr_t cpu_area;
        uintptr_t tid_address;
        stack_t altstack;
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

        struct timespec deadline;
        bool deadline_valid;
        volatile uint32_t deadline_futex;

        sigset_t sigmask;
        bool sigmask_valid;

        size_t progress;
        bool started;
        long interrupted;

    } syscall;

    struct rlimit rlimits[RLIM_NLIMITS];
    struct rusage rusage;
    struct rusage rusage_dead;
    struct rusage rusage_children;

    char comm[TASK_COMM_LEN];
    char cmdline[TASK_CMDLINE_LEN];
    size_t cmdline_len;
    uint64_t start_time;

    spinlock_t lock;
    spinlock_t sched_lock;

    struct cpu* cpu;
    struct task* next;

} task_t;


    #define thread_restart_sched(task) task->flags |= TASK_FLAGS_NEED_RESCHED
    #define thread_restart_syscall(task) task->flags |= TASK_FLAGS_NEED_SYSCALL_RESTART
    #define thread_suspend(task) sched_set_status(task, TASK_STATUS_SLEEP)
    #define thread_wake(task) sched_set_status(task, TASK_STATUS_READY)


/**
 * @brief Tells whether a task belongs to a group, through its filesystem group ID or a supplementary one.
 *
 * @param task The task.
 * @param gid The group.
 * @return true if the task is a member of @p gid.
 */
static inline bool task_in_group(const task_t* task, gid_t gid) {

    if (task->fsgid == gid)
        return true;

    for (size_t i = 0; i < task->ngroups; i++) {
        if (task->groups[i] == gid)
            return true;
    }

    return false;
}


/**
 * @brief Adds one resource usage record into another, keeping the larger peak resident set.
 *
 * @param dst The record added to.
 * @param src The record to add.
 */
static inline void task_rusage_add(struct rusage* dst, const struct rusage* src) {

    dst->ru_utime.tv_sec += src->ru_utime.tv_sec + (dst->ru_utime.tv_usec + src->ru_utime.tv_usec) / 1000000;
    dst->ru_utime.tv_usec = (dst->ru_utime.tv_usec + src->ru_utime.tv_usec) % 1000000;
    dst->ru_stime.tv_sec += src->ru_stime.tv_sec + (dst->ru_stime.tv_usec + src->ru_stime.tv_usec) / 1000000;
    dst->ru_stime.tv_usec = (dst->ru_stime.tv_usec + src->ru_stime.tv_usec) % 1000000;

    if (src->ru_maxrss > dst->ru_maxrss)
        dst->ru_maxrss = src->ru_maxrss;

    dst->ru_minflt += src->ru_minflt;
    dst->ru_majflt += src->ru_majflt;
    dst->ru_nswap += src->ru_nswap;
    dst->ru_inblock += src->ru_inblock;
    dst->ru_oublock += src->ru_oublock;
    dst->ru_msgsnd += src->ru_msgsnd;
    dst->ru_msgrcv += src->ru_msgrcv;
    dst->ru_nsignals += src->ru_nsignals;
    dst->ru_nvcsw += src->ru_nvcsw;
    dst->ru_nivcsw += src->ru_nivcsw;
}


/**
 * @brief Gives a thread's own resource usage, with its CPU time taken from its thread clock.
 *
 * @param task The thread.
 * @param ru Receives the usage.
 */
static inline void task_rusage_self(const task_t* task, struct rusage* ru) {

    *ru = task->rusage;

    ru->ru_utime.tv_sec  = task->clock[TASK_CLOCK_THREAD_CPUTIME].tv_sec;
    ru->ru_utime.tv_usec = task->clock[TASK_CLOCK_THREAD_CPUTIME].tv_nsec / 1000;
}



#define SCHED_SIGQUEUE_KERNEL 1
#define SCHED_SIGQUEUE_BROADCAST 2

__BEGIN_DECLS

void do_unshare(int);
void do_vfork_release(void);
pid_t do_fork(struct kclone_args*, size_t);

pid_t sched_nextpid();
void sched_enqueue(task_t*);
void sched_set_status(task_t*, ssize_t);
void sched_bury(struct cpu*, task_t*);
void sched_exit(void);
void sched_group_exit(int);
void sched_group_setids(pid_t pid, pid_t pgrp, pid_t sid);
void sched_die(int);
int sched_sigqueueinfo(pid_t pgrp, pid_t pid, pid_t tid, int sig, siginfo_t*);
int sched_sigqueue(pid_t pgrp, pid_t pid, pid_t tid, int sig, siginfo_t*, int flags);
int sched_fault_sigqueueinfo(int sig, siginfo_t*);
void sched_raise(int sig, siginfo_t*);
void sched_sigmask(const sigset_t*);
void sched_signals(void);
size_t sched_nprocs(void);
pid_t sched_lastpid(void);
void sched_dnit(void);

void schedule(int);

void idle_main(void*) __noreturn;

extern volatile uint32_t sched_child_event;

__END_DECLS

#endif
#endif
