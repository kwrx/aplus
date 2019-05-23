/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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


#ifndef _APLUS_TASK_H
#define _APLUS_TASK_H

#include <aplus.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>

#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/types.h>


#define TASK_STATUS_READY                   0
#define TASK_STATUS_RUNNING                 1
#define TASK_STATUS_SLEEP                   2
#define TASK_STATUS_STOP                    3
#define TASK_STATUS_ZOMBIE                  4

#define TASK_PRIO_MAX                       -20
#define TASK_PRIO_MIN                       19
#define TASK_PRIO_REG                       0

#define TASK_NSIG                           NSIG
#define TASK_NARGS                          4096
#define TASK_NFD                            32
#define TASK_NPROC                          2048



#if defined(KERNEL)
#include <aplus.h>


typedef struct {
    inode_t* inode;
    off_t position;
    spinlock_t lock;
    int flags;
} fd_t;

typedef struct {
    uintptr_t vmmpd;
    uintptr_t start;
    uintptr_t end;
    uintptr_t used;
    spinlock_t lock;
    int refcount;
} address_space_t;

typedef struct task {
    struct {
        void* frame;
        uint8_t fpu[1024];
    } context;

    char** argv;
    char** environ;

    pid_t pid;
    pid_t tgid;
    pid_t pgid;
    uid_t uid, euid;
    gid_t gid, egid;
    uid_t sid;

    int status;
    int priority;
    int affinity;
    
    struct tms clock;
    struct timespec sleep;
    
    struct {
        struct sigaction handlers[TASK_NSIG];
        //list(siginfo_t*, queue);
        sigset_t mask;
    } signal;

    fd_t fd[TASK_NFD];
    
    inode_t* root;
    inode_t* cwd;
    inode_t* exe;

    mode_t umask;

    //list(struct task*, waiters);

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
        };

        int16_t value:16;
    } exit;


    address_space_t * aspace;
    address_space_t __aspace;

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

    spinlock_t lock;

    struct task* parent;
    struct task* next;

    uint8_t __padding[4];
} __packed task_t;



pid_t sched_nextpid();

void sched_enqueue(task_t*);
void sched_dequeue(task_t*);
void sched_init(void);

void* schedule(void*);
void* schedule_yield(void*);

void* arch_task_switch(void*, task_t*, task_t*);

extern task_t* kernel_task;
extern task_t* sched_queue;

#endif
#endif