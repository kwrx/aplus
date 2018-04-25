#include "pthread_internal.h"


static int __pthread_routine(void* arg) {
    struct p_context* cc = (struct p_context*) arg;
    if(!cc)
        pthread_exit(NULL);

    pthread_exit(cc->start_routine(cc->arg));
}

int pthread_create(pthread_t* th, const pthread_attr_t* attr, void* (*start_routine) (void*), void* arg) {
    __pthread_init();
    
    struct p_context* cc = (struct p_context*) calloc(1, sizeof(struct p_context));
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

