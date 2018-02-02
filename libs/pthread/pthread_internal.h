#pragma once

#include <pthread.h>

typedef struct pthread_context {
    pid_t pid;
    void* (*start_routine) (void*);
    void* arg;

    int cancel;
    pthread_attr_t* attr;
    struct pthread_context* next;
};

struct pthread_context* __pthread_queue;
void __pthread_init(void);
void __pthread_add_queue(struct pthread_context* cc);
void __pthread_remove_queue(struct pthread_context* cc);
