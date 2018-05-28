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


#pragma once

#ifndef _POSIX_THREADS
#define _POSIX_THREADS                              1
#endif

#ifndef _UNIX98_THREAD_MUTEX_ATTRIBUTES
#define _UNIX98_THREAD_MUTEX_ATTRIBUTES             1
#endif

#ifndef _POSIX_THREAD_PRIORITY_SCHEDULING
#define _POSIX_THREAD_PRIORITY_SCHEDULING           1
#endif

#ifndef _POSIX_THREAD_PROCESS_SHARED
#define _POSIX_THREAD_PROCESS_SHARED                1
#endif

#ifndef _POSIX_THREAD_PRIO_PROTECT
#define _POSIX_THREAD_PRIO_PROTECT                  1
#endif

#ifndef _POSIX_THREAD_PRIO_INHERIT
#define _POSIX_THREAD_PRIO_INHERIT                  1
#endif

#ifndef _POSIX_TIMERS
#define _POSIX_TIMERS                               1
#endif

#ifndef _POSIX_MONOTONIC_CLOCK
#define _POSIX_MONOTONIC_CLOCK                      1
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sched.h>



#define PKEY_MAX            4096

#include <pthread.h>

struct p_context {
    char name[BUFSIZ];
    pid_t pid;
    void* (*start_routine) (void*);
    void* arg;
    void* status;
    
    int cancel;
    pthread_attr_t attr;
    struct p_context* next;
};

struct p_mutex {
    pid_t owner;
    long lock;
    long refcount;
    pthread_mutexattr_t attr;
};

struct p_cond {
    int lock;
    pthread_condattr_t attr;
};

struct p_context* __pthread_queue;
void __pthread_init(void);
void __pthread_add_queue(struct p_context* cc);
void __pthread_remove_queue(struct p_context* cc);
