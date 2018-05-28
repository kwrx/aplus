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

int	pthread_attr_setscope (pthread_attr_t *__attr, int __contentionscope) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    __attr->contentionscope = __contentionscope;
    return 0;
}

int	pthread_attr_getscope (const pthread_attr_t *__attr, int *__contentionscope) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    if(__contentionscope)
        *__contentionscope = __attr->contentionscope;

    return 0;
}

int	pthread_attr_setinheritsched (pthread_attr_t *__attr, int __inheritsched) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    __attr->inheritsched = __inheritsched;
    return 0;
}

int	pthread_attr_getinheritsched (const pthread_attr_t *__attr, int *__inheritsched) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    if(__inheritsched)
        *__inheritsched = __attr->inheritsched;

    return 0;
}

int	pthread_attr_setschedpolicy (pthread_attr_t *__attr, int __policy) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    __attr->schedpolicy = __policy;
    return 0;
}

int	pthread_attr_getschedpolicy (const pthread_attr_t *__attr, int *__policy) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    if(__policy)
        *__policy = __attr->schedpolicy;

    return 0;
}

int	pthread_attr_setschedparam (pthread_attr_t *__attr, const struct sched_param *__param) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    if(__param)
        memcpy(&__attr->schedparam, __param, sizeof(struct sched_param));

    return 0;
}

int	pthread_attr_getschedparam (const pthread_attr_t *__attr, struct sched_param *__param) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    if(__param)
        memcpy(__param, &__attr->schedparam, sizeof(struct sched_param));

    return 0;
}

int	pthread_getschedparam (pthread_t th, int *__policy, struct sched_param *__param) {
    if(th <= 0)
        return -1;

    struct p_context* cc = (struct p_context*) th;
    
    if(__policy)
        *__policy = cc->attr.schedpolicy;
    
    if(__param)
        memcpy(__param, &cc->attr.schedparam, sizeof(struct sched_param));

    return 0;
}

int	pthread_setschedparam (pthread_t th, int __policy, struct sched_param *__param) {
    if(th <= 0)
        return -1;

    struct p_context* cc = (struct p_context*) th;
    cc->attr.schedpolicy == __policy;
    
    if(__param)
        memcpy(&cc->attr.schedparam, __param, sizeof(struct sched_param));

    return 0;
}

int	pthread_setschedprio (pthread_t thread, int prio) {
    errno = ENOSYS;
    return -1;
}

int	pthread_getname_np(pthread_t th, char* name, size_t size) {
    if(th <= 0)
        return -1;

    struct p_context* cc = (struct p_context*) th;
    strncpy(name, cc->name, size);
    
    return 0;
}

int	pthread_setname_np(pthread_t th, const char* name) {
    if(th <= 0)
        return -1;

    struct p_context* cc = (struct p_context*) th;
    strncpy(cc->name, name, BUFSIZ);
    
    return 0;
}


int	pthread_attr_init (pthread_attr_t *__attr) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    if(__attr->is_initialized) {
        errno = EBUSY;
        return -1;
    }

    memset(__attr, 0, sizeof(pthread_condattr_t));
    __attr->is_initialized = 1;

    return 0;
}

int	pthread_attr_destroy (pthread_attr_t *__attr) {
    if(!__attr) {
        errno = EINVAL;
        return -1;
    }

    memset(__attr, 0, sizeof(pthread_condattr_t));
    return 0;
}

int	pthread_attr_setstack (pthread_attr_t *attr, void *__stackaddr, size_t __stacksize) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    attr->stackaddr = __stackaddr;
    attr->stacksize = __stacksize;

    return 0;
}

int	pthread_attr_getstack (const pthread_attr_t *attr, void **__stackaddr, size_t *__stacksize) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    if(__stackaddr)
        *__stackaddr = attr->stackaddr;

    if(__stacksize)
        *__stacksize = attr->stacksize;

    return 0;
}

int	pthread_attr_getstacksize (const pthread_attr_t *attr, size_t *__stacksize) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    if(__stacksize)
        *__stacksize = attr->stacksize;

    return 0;
}

int	pthread_attr_setstacksize (pthread_attr_t *attr, size_t __stacksize) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    attr->stacksize = __stacksize;
    return 0;
}

int	pthread_attr_getstackaddr (const pthread_attr_t *attr, void **__stackaddr) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    if(__stackaddr)
        *__stackaddr = attr->stackaddr;

    return 0;
}

int	pthread_attr_setstackaddr (pthread_attr_t  *attr, void *__stackaddr) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    attr->stackaddr = __stackaddr;
    return 0;
}

int	pthread_attr_getdetachstate (const pthread_attr_t *attr, int *__detachstate) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    if(__detachstate)
        *__detachstate = attr->detachstate;

    return 0;
}

int	pthread_attr_setdetachstate (pthread_attr_t *attr, int __detachstate) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    attr->detachstate = __detachstate;
    return 0;
}

int	pthread_attr_getguardsize (const pthread_attr_t *__attr, size_t *__guardsize) {
    errno = ENOSYS;
    return -1;
}

int	pthread_attr_setguardsize (pthread_attr_t *__attr, size_t __guardsize) {
    errno = ENOSYS;
    return -1;
}