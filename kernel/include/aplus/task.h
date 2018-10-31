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


#ifndef _TASK_H
#define _TASK_H

#include <aplus.h>
#include <aplus/base.h>
#include <aplus/mm.h>
#include <aplus/vfs.h>
#include <aplus/elf.h>
#include <aplus/utils/list.h>
#include <libc.h>


#define TASK_STATUS_READY               0
#define TASK_STATUS_RUNNING             1
#define TASK_STATUS_SLEEP               2
#define TASK_STATUS_STOP                3
#define TASK_STATUS_ZOMBIE              4

#define TASK_PRIO_MAX                   -20
#define TASK_PRIO_MIN                   19
#define TASK_PRIO_REGULAR               0

#define TASK_NSIG                       NSIG
#define TASK_NARGS                      8192
#define TASK_NAUXV                      38

#define TASK_FD_COUNT                   32
#define TASK_FIFOSZ                     PAGE_SIZE

#define TASK_ROOT_UID                   ((uid_t) 0)
#define TASK_ROOT_GID                   ((gid_t) 0)


#define WCOREFLAG                       0200
#define W_EXITCODE(ret, sig)            ((ret) << 8 | sig)
#define W_STOPCODE(sig)                 ((sig) << 8 | 0177)


#ifndef __ASSEMBLY__

typedef struct fd {
    inode_t* inode;
    off_t position;
    int flags;
} fd_t;


struct __user_desc {
    unsigned int entry_number;
    unsigned long base_addr;
    unsigned int limit;
    unsigned int seg_32bit:1;
    unsigned int contents:2;
    unsigned int read_exec_only:1;
    unsigned int limit_in_pages:1;
    unsigned int seg_not_present:1;
    unsigned int useable:1;
};


typedef struct task {

    char* name;
    char* description;
    char** argv;
    char** environ;

    pid_t pid;
    pid_t tgid;
    pid_t pgid;
    uid_t uid;
    gid_t gid;
    uid_t sid;

    void* tid;

    int status;
    int priority;
    uintptr_t vmsize;

    void* context;
    void* sys_stack;
    int thread_area;
        
    
    struct tms clock;
    struct timespec sleep;
    ktime_t starttime;

    struct {
        ktime_t it_interval;
        ktime_t it_value;
    } itimers[3];

    struct {
        struct sigaction s_handlers[TASK_NSIG];
        list(siginfo_t*, s_queue);
        sigset_t s_mask;
    } signal;


    fd_t fd[TASK_FD_COUNT];    
    fifo_t fifo;

    inode_t* root;
    inode_t* cwd;
    inode_t* exe;
    mode_t umask;


    list(struct task*, waiters);

    struct {
        union {
            struct {
                int16_t o177:8;
                int16_t signo:8;
            } stopped;

            struct {
                int16_t zero:8;
                int16_t retval:8;
            } exited;

            struct {
                int16_t signo:7;
                int16_t corep:1;
                int16_t zero:8;
            } termed;

            int16_t value:16;
        };
    } exit;


    struct {
        uintptr_t start;
        uintptr_t end;
        int refcount;
    } __image, *image;

    struct {
        uint64_t rchar;
        uint64_t wchar;
        uint64_t syscr;
        uint64_t syscw;
        uint64_t read_bytes;
        uint64_t write_bytes;
        uint64_t cancelled_write_bytes;
    } iostat;

    struct rlimit rlimits[RLIM_NLIMITS];
    struct rusage rusage;

    struct task* parent;
    struct task* next;
} task_t;


extern volatile task_t* current_task;
extern volatile task_t* kernel_task;
extern volatile task_t* task_queue;

void schedule(void);
void schedule_yield(void);
pid_t sched_nextpid();
void sched_dosignals();
void sched_sigqueueinfo(task_t*, int, siginfo_t*);

extern void task_switch(volatile task_t*, volatile task_t*);
extern volatile task_t* task_fork(void);
extern volatile task_t* task_clone(int (*) (void*), void*, int, void*);
extern void task_yield(void);
extern void task_release(volatile task_t* task);
extern int task_set_thread_area(volatile task_t* tk, struct __user_desc* uinfo);
extern int task_fork_thread_area(int th_area);

#define task_create_tasklet(nm, handler, task)                                                                  \
        task = task_clone(handler, NULL, CLONE_VM | CLONE_SIGHAND | CLONE_FILES | CLONE_FS, NULL);              \
        task->name = strdup(nm);


#define is_superuser(task)                                                                                      \
    ((task->uid == TASK_ROOT_UID) || (task->gid == TASK_ROOT_GID))


#endif

#endif
