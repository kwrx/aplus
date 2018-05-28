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

int	pthread_mutexattr_init (pthread_mutexattr_t *__attr) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    if(__attr->is_initialized)
        return 0;

    __attr->is_initialized = 1;

#if defined(_POSIX_THREAD_PROCESS_SHARED)
    __attr->process_shared = 0;
#endif

#if defined(_POSIX_THREAD_PRIO_PROTECT)
    __attr->prio_ceiling =
    __attr->protocol = 0;
#endif

#if defined(_UNIX98_THREAD_MUTEX_ATTRIBUTES)
    __attr->type = PTHREAD_MUTEX_DEFAULT;
#endif

    __attr->recursive = 0;
}

int	pthread_mutexattr_destroy (pthread_mutexattr_t *__attr) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    __attr->is_initialized = 0;
    return 0;
}

int	pthread_mutexattr_getpshared (const pthread_mutexattr_t *__attr, int  *__pshared) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    if(__pshared)
#if defined(_POSIX_THREAD_PROCESS_SHARED)
        *__pshared = __attr->process_shared;
#endif
        *__pshared = 0;

    return 0;
}

int	pthread_mutexattr_setpshared (pthread_mutexattr_t *__attr, int __pshared) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

#if defined(_POSIX_THREAD_PROCESS_SHARED)
    __attr->process_shared = !!(__pshared);
#endif

    return 0;
}


#if defined(_UNIX98_THREAD_MUTEX_ATTRIBUTES)

int pthread_mutexattr_gettype (const pthread_mutexattr_t *__attr, int *__kind) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    if(__kind)
        *__kind = __attr->type;

    return 0;
}

int pthread_mutexattr_settype (pthread_mutexattr_t *__attr, int __kind) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    __attr->type = __kind;
    return 0;
}

#endif


int	pthread_mutex_init (pthread_mutex_t *__mutex, const pthread_mutexattr_t *__attr) {
    if(!__mutex) {
        errno = EINVAL;
        return -1;
    }

    struct p_mutex* p = (struct p_mutex*) calloc(1, sizeof(struct p_mutex));
    if(!p)
        return -1;

    if(__attr)
        memcpy(&p->attr, __attr, sizeof(pthread_mutexattr_t));

    p->lock = 0;
    p->owner = -1;
    p->refcount = 1;

    *__mutex = (pthread_mutex_t) p;
    return 0;
}

int	pthread_mutex_destroy (pthread_mutex_t *__mutex) {
    if(!__mutex) {
        errno = EINVAL;
        return -1;
    }

    free((void*) *__mutex);
    *__mutex = 0;
    return 0;
}


int	pthread_mutex_timedlock (pthread_mutex_t *__mutex, const struct timespec *__timeout) {
    if(!__mutex) {
        errno = EINVAL;
        return -1;
    }

    struct p_mutex* p = (struct p_mutex*) *__mutex;
    if(!p) {
        errno = EINVAL;
        return -1;
    }


#if defined(_UNIX98_THREAD_MUTEX_ATTRIBUTES)
    switch(p->attr.type) {
        case PTHREAD_MUTEX_ERRORCHECK:
            if(p->lock && p->owner == getpid()) {
                errno = EDEADLK;
                return -1;
            }
            break;
        case PTHREAD_MUTEX_RECURSIVE:
            if(p->lock && p->owner == getpid())
                p->refcount++;
            return 0;
        default:
            break;
    }
#endif

    uint64_t ts1;
    if(__timeout) {
        struct timespec tp;
        clock_gettime(CLOCK_MONOTONIC, &tp);
        
        ts1 = (tp.tv_sec + __timeout->tv_sec) * 1000000000 +
                        (tp.tv_nsec + __timeout->tv_nsec);
    }
    
    while(
        !__sync_bool_compare_and_swap(&p->lock, 0, 1) &&
        !__timeout ? 1 : ({
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);

            uint64_t ts0 = ts.tv_sec * 1000000000 + ts.tv_nsec;
            (ts0 < ts1);
        })
    )
        pthread_yield();

    p->owner = getpid();
    p->refcount = 1;    
    __sync_synchronize();
    
        
    return 0;
}


int	pthread_mutex_lock (pthread_mutex_t *__mutex) {
    return pthread_mutex_timedlock(__mutex, NULL);
}

int	pthread_mutex_trylock (pthread_mutex_t *__mutex) {
    if(!__mutex) {
        errno = EINVAL;
        return -1;
    }

    struct p_mutex* p = (struct p_mutex*) *__mutex;
    if(!p) {
        errno = EINVAL;
        return -1;
    }

    switch(p->attr.type) {
        case PTHREAD_MUTEX_RECURSIVE:
            if(p->lock && p->owner == getpid())
                p->refcount++;
            return 0;
        default:
            break;
    }

    if(p->lock) {
        errno = EBUSY;
        return -1;
    }

    return pthread_mutex_lock(__mutex);
}

int	pthread_mutex_unlock (pthread_mutex_t *__mutex) {
    if(!__mutex) {
        errno = EINVAL;
        return -1;
    }

    struct p_mutex* p = (struct p_mutex*) *__mutex;
    if(!p) {
        errno = EINVAL;
        return -1;
    }

    if(p->owner != getpid()) {
        errno = EPERM;
        return -1;
    }

    if(!p->lock) {
        errno = EINVAL;
        return -1;
    }
    
    switch(p->attr.type) {
        case PTHREAD_MUTEX_RECURSIVE:
            if(--p->refcount)
                return 0;    
        default:
            break;
    }


    
    __sync_lock_release(&p->lock);

    p->owner = -1;
    p->refcount = 1;    
    __sync_synchronize();
    
        
    return 0;
}


int	pthread_mutexattr_setprotocol (pthread_mutexattr_t *__attr, int __protocol) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    __attr->protocol = __protocol;
    return 0;
}

int	pthread_mutexattr_getprotocol (const pthread_mutexattr_t *__attr, int *__protocol) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    if(__protocol)
        *__protocol = __attr->protocol;

    return 0;
}

int	pthread_mutexattr_setprioceiling (pthread_mutexattr_t *__attr, int __prioceiling) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    __attr->prio_ceiling = __prioceiling;
    return 0;
}

int	pthread_mutexattr_getprioceiling (const pthread_mutexattr_t *__attr, int *__prioceiling) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    if(__prioceiling)
        *__prioceiling = __attr->prio_ceiling;

    return 0;
}


int	pthread_mutex_setprioceiling (pthread_mutex_t *__mutex, int __prioceiling, int *__old_ceiling) {
    if(!__mutex) {
        errno = EINVAL;
        return -1;
    }

    struct p_mutex* p = (struct p_mutex*) *__mutex;
    if(!p) {
        errno = EINVAL;
        return -1;
    }

    if(__old_ceiling)
        *__old_ceiling = p->attr.prio_ceiling;

    p->attr.prio_ceiling = __prioceiling;
    return 0;
}

int	pthread_mutex_getprioceiling (pthread_mutex_t *__mutex, int *__prioceiling) {
    if(!__mutex) {
        errno = EINVAL;
        return -1;
    }

    struct p_mutex* p = (struct p_mutex*) *__mutex;
    if(!p) {
        errno = EINVAL;
        return -1;
    }

    if(__prioceiling)
        *__prioceiling = p->attr.prio_ceiling;

    return 0;
}