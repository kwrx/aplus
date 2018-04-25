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
