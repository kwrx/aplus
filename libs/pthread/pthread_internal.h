#pragma once

#include <pthread.h>

struct p_context {
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

struct p_context* __pthread_queue;
void __pthread_init(void);
void __pthread_add_queue(struct p_context* cc);
void __pthread_remove_queue(struct p_context* cc);
