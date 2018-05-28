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


#include "pthread_internal.h"



int pthread_condattr_init(pthread_condattr_t* attr) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    if(attr->is_initialized) {
        errno = EBUSY;
        return -1;
    }

    memset(attr, 0, sizeof(pthread_condattr_t));
    attr->is_initialized = 1;

    return 0;
}

int pthread_condattr_destroy(pthread_condattr_t* attr) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    memset(attr, 0, sizeof(pthread_condattr_t));
    return 0;
}

int	pthread_condattr_getclock (const pthread_condattr_t *__restrict __attr, clockid_t *__restrict __clock_id) {
    if(__clock_id)
        *__clock_id = CLOCK_MONOTONIC;

    return 0;
}

int	pthread_condattr_setclock (pthread_condattr_t *__attr, clockid_t __clock_id) {
    if(__clock_id != CLOCK_MONOTONIC) {
        errno = EINVAL;
        return -1;
    }

    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

int	pthread_condattr_getpshared (const pthread_condattr_t *__attr, int *__pshared) {
#if defined(_POSIX_THREAD_PROCESS_SHARED)
    if(__pshared && __attr)
        *__pshared = __attr->process_shared;

    return 0;
#endif

    errno = ENOSYS;
    return -1;
}

int	pthread_condattr_setpshared (pthread_condattr_t *__attr, int __pshared) {
#if defined(_POSIX_THREAD_PROCESS_SHARED)
    if(__attr)
        __attr->process_shared = __pshared;

    return 0;
#endif

    errno = ENOSYS;
    return -1;
}
 
int	pthread_cond_signal (pthread_cond_t *__cond) {
    if(!__cond) {
        errno = EINVAL;
        return -1;
    }

    struct p_cond* p = (struct p_cond*) *__cond;

    __sync_synchronize();
    p->lock--;
    __sync_synchronize();

    return 0;
}

int	pthread_cond_broadcast (pthread_cond_t *__cond) {
    if(!__cond) {
        errno = EINVAL;
        return -1;
    }

    struct p_cond* p = (struct p_cond*) *__cond;

    __sync_synchronize();
    p->lock = 0;
    __sync_synchronize();

    return 0;
}


int	pthread_cond_wait (pthread_cond_t *__cond, pthread_mutex_t *__mutex) {
    return pthread_cond_timedwait(__cond, __mutex, NULL);
}
 
int	pthread_cond_timedwait (pthread_cond_t *__cond, pthread_mutex_t *__mutex, const struct timespec *__timeout) {
    if(!__cond) {
        errno = EINVAL;
        return -1;
    }

    if(__mutex)
        pthread_mutex_unlock(__mutex);



    struct p_cond* p = (struct p_cond*) *__cond;

    uint64_t ts1;
    if(__timeout) {
        struct timespec tp;
        clock_gettime(CLOCK_MONOTONIC, &tp);
        
        ts1 = (tp.tv_sec + __timeout->tv_sec) * 1000000000 +
                        (tp.tv_nsec + __timeout->tv_nsec);
    }
    

    __sync_synchronize();
    int nlock = p->lock++;
    __sync_synchronize();


    while (
        p->lock > nlock &&
        !__timeout ? 1 : ({
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);

            uint64_t ts0 = ts.tv_sec * 1000000000 + ts.tv_nsec;
            (ts0 < ts1);
        })
    )
        pthread_yield();

    pthread_mutex_lock(__mutex);
    return 0;
}