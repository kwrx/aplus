
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#include "pthread_internal.h"


PRIVATE int __locked_mtx(pthread_mutex_t* mutex) {
    if(mutex->time > 0) {
        if(time(NULL) < mutex->time)
            return -1;
        else {
            pthread_mutex_unlock(mutex);
            mutex->time = 0;
        }
    }


    if(mutex->lock == 1)
        return -1;

    return 0;
}

PUBLIC int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    if(!mutex) {
        errno = EINVAL;
        return -1;
    }

    mutex->lock = 0;
    mutex->recursion = 0;
    mutex->kind = PTHREAD_MUTEX_DEFAULT;
    mutex->owner = 0;
    mutex->event = 0;
    mutex->time = 0;

    if(attr)
        mutex->kind = attr->kind;

    return 0;
}


PUBLIC int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    if(!mutex) {
        errno = EINVAL;
        return -1;
    }

    if(mutex->lock == 1) {
        errno = EBUSY;
        return -1;
    }

    mutex->lock = 0;
    mutex->recursion = 0;
    mutex->kind = 0;
    mutex->owner = 0;
    mutex->event = 0;
    mutex->time = 0;

    return 0;
}

PUBLIC int pthread_mutex_lock(pthread_mutex_t *mutex) {
    if(!mutex) {
        errno = EINVAL;
        return -1;
    }

    if(mutex->owner != pthread_self()) {
        while(__locked_mtx(mutex))
            sched_yield();
        
        mutex->owner = pthread_self();
        mutex->recursion = 0;
        mutex->lock = 1;
    } else if(mutex->kind == PTHREAD_MUTEX_ERRORCHECK) {
        errno = EDEADLK;
        return -1;
    }

    if(mutex->kind == PTHREAD_MUTEX_RECURSIVE)
        mutex->recursion += 1;

    return 0;
}


PUBLIC int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abstime) {
    if(abstime == NULL) {
        errno = EINVAL;
        return -1;
    }

    if(pthread_mutex_lock(mutex) == 0)
        mutex->time = time(NULL) + abstime->tv_sec;
    else
        return -1;

    return 0;
}


PUBLIC int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    if(!mutex) {
        errno = EINVAL;
        return -1;
    }

    if(mutex->owner != pthread_self()) {
        if(__locked_mtx(mutex)) {
            errno = EBUSY;
            return -1;
        }

        mutex->owner = pthread_self();
        mutex->recursion = 0;
        mutex->lock = 1;
    } else if(mutex->kind == PTHREAD_MUTEX_ERRORCHECK) {
        errno = EDEADLK;
        return -1;
    }

    if(mutex->kind == PTHREAD_MUTEX_RECURSIVE)
        mutex->recursion += 1;

    return 0;
}

PUBLIC int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    if(!mutex) {
        errno = EINVAL;
        return -1;
    }

    if(mutex->owner == pthread_self()) {
        if(mutex->kind == PTHREAD_MUTEX_RECURSIVE) {
            if(--(mutex->recursion))
                return 0;
        }

        mutex->owner = 0;
        mutex->lock = 0;
    } else if(mutex->kind == PTHREAD_MUTEX_ERRORCHECK) {
        errno = EPERM;
        return -1;
    }

    return 0;
}
