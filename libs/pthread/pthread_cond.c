
#include <stdint.h>
#include <sched.h>
#include <time.h>
#include <pthread.h>

#include "pthread_internal.h"

EXTERN pthread_context_t* __pthread_queue;



PUBLIC int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    if(!cond) {
        errno = EINVAL;
        return -1;
    }

    cond->waiting = 0;
    cond->semaphore = 0;

    return 0;
}

PUBLIC int pthread_cond_destroy(pthread_cond_t *cond) {
    if(!cond) {
        errno = EINVAL;
        return -1;
    }

    if(cond->waiting) {
        errno = EBUSY;
        return -1;
    }


    cond->waiting = 0;
    cond->semaphore = 0;        
    
    return 0;
}


PUBLIC int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
        if(!cond) {
        errno = EINVAL;
        return -1;
    }

    if(!mutex) {
        errno = EINVAL;
        return -1;
    }

    if(mutex->owner != pthread_self()) {
        errno = EINVAL;
        return -1;
    }

    pthread_context_t* ctx = (pthread_context_t*) pthread_self();
    if(!ctx) {
        errno = EINVAL;
        return -1;
    }

    ctx->cond = cond;
    ctx->cond->waiting = 1;
    ctx->cond->semaphore = 1;

    int t0;
    if(abstime)
        t0 = time(NULL) + abstime->tv_sec;
    else
        t0 = ~0;

    while(ctx->cond->semaphore == 1 && (time(NULL) < t0))
        sched_yield();

    ctx->cond->waiting = 0;
    ctx->cond->semaphore = 0;
    ctx->cond = 0;

    pthread_mutex_unlock(mutex);

    return 0;
}

PUBLIC int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    return pthread_cond_timedwait(cond, mutex, NULL);
}


PUBLIC int pthread_cond_signal(pthread_cond_t *cond) {
    if(!__pthread_queue) {
        errno = ESRCH;
        return -1;
    }


    pthread_context_t* tmp = __pthread_queue;
    while(tmp) {
        if(tmp->cond == cond) {
            if(tmp->cond->waiting) {
                tmp->cond->semaphore = 0;
                return 0;            
            }
        }

        tmp = tmp->next;
    }

    errno = ESRCH;
    return -1;
}

PUBLIC int pthread_cond_broadcast(pthread_cond_t *cond) {
    if(!__pthread_queue) {
        errno = ESRCH;
        return -1;
    }


    pthread_context_t* tmp = __pthread_queue;
    while(tmp) {
        if(tmp->cond == cond) {
            if(tmp->cond->waiting) {
                tmp->cond->semaphore = 0;        
            }
        }

        tmp = tmp->next;
    }


    return 0;
}
