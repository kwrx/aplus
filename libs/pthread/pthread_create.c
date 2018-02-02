#include <pthread.h>
#include <sys/types.h>
#include <sched.h>
#include <errno.h>

#include "pthread_internal.h"


static void* __pthread_routine(void* arg) {
    struct pthread_context* cc = (struct pthread_context*) arg;
    if(!cc)
        pthread_exit(NULL);

    pthread_exit(cc->start_routine(cc->arg));
}

int pthread_create(pthread_t* th, const pthread_attr_t* attr, void* (*start_routine) (void*), void* arg) {
    __pthread_init();
    
    struct pthread_context* cc = (struct pthread_context*) calloc(1, sizeof(struct pthread_context));
    if(!cc) {
        errno = ENOMEM;
        return -1;
    }

    if(attr)
        memcpy(&cc->attr, &attr, sizeof(pthread_attr_t));

    cc->start_routine = start_routine;
    cc->arg = arg;
    cc->pid = clone(__pthread_routine, cc->attr.stackaddr, CLONE_FS | CLONE_FILES | CLONE_PARENT | CLONE_SIGHAND, cc);

    if(cc->pid < 0) {
        free(cc);
        return -1;
    }


    __pthread_add_queue(cc);

    (*th) = (pthread_t) cc;
    return 0;
}