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

#define _GNU_SOURCE
#include <sched.h>

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


#define ASPACE_TYPE_UNKNOWN                 0
#define ASPACE_TYPE_CODE                    1
#define ASPACE_TYPE_DATA                    2
#define ASPACE_TYPE_TLS                     3
#define ASPACE_TYPE_BRK                     4
#define ASPACE_TYPE_SHARED                  5
#define ASPACE_TYPE_SWAP                    6


#if defined(KERNEL)
#include <aplus.h>


typedef struct address_space address_space_t;

typedef struct {

    struct {
        uintptr_t type:4;
        uintptr_t flags:28;
    };

    uintptr_t start;
    uintptr_t end;

    spinlock_t lock;
    int refcount;
    
} address_space_map_t;


typedef struct {

    void (*open)   (address_space_t*);
    void (*close)  (address_space_t*);
    void (*nopage) (address_space_t*, uintptr_t);

} address_space_ops_t;


struct address_space {

    uintptr_t vmmpd;
    uintptr_t start;
    uintptr_t end;
    uintptr_t size;
    address_space_ops_t ops;

    list(address_space_map_t*, mappings);
    
};





typedef struct {

    inode_t* inode;
    off_t position;
    
    struct {
        int flags:30;
        int close_on_exec:1;
    };

    spinlock_t lock;
} fd_t;


typedef struct task {
    struct {
        void* frame;
        uint8_t fpu[1024];
    } context;

    char** argv;
    char** environ;

    pid_t tid;
    pid_t tgid; /* pid */
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


void task_init(void);

pid_t sched_nextpid();

void sched_enqueue(task_t*);
void sched_dequeue(task_t*);
void sched_init(void);

void* schedule(void*);
void* schedule_yield(void*);

void* arch_task_switch(void*, task_t*, task_t*);
void* arch_task_init_frame(void*, void*, void*);



void aspace_create(address_space_t**, uintptr_t, address_space_ops_t*, uintptr_t, uintptr_t);
void aspace_create_nomem(address_space_t**, address_space_t*, uintptr_t, address_space_ops_t*, uintptr_t, uintptr_t);
void aspace_free(address_space_t*);
void aspace_destroy(address_space_t**);
void aspace_clone(address_space_t**, address_space_t*);
void aspace_fork(address_space_t**, address_space_t*);
void aspace_ref_map(address_space_t*, address_space_map_t*);
void aspace_enlarge_map(address_space_t*, address_space_map_t*, size_t);
void aspace_shrink_map(address_space_t*, address_space_map_t*, size_t);
void aspace_add_map(address_space_t*, uint8_t, uintptr_t, uintptr_t, uint32_t);
void aspace_remove_map(address_space_t*, address_space_map_t*);
void aspace_get_maps(address_space_t*, address_space_map_t**, uint8_t, size_t);


extern task_t* kernel_task;
extern task_t* sched_queue;


#endif
#endif