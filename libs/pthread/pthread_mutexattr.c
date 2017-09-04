
#include <stdint.h>
#include <pthread.h>

#include "pthread_internal.h"


PUBLIC int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    attr->pshared = 0;
    attr->kind = PTHREAD_MUTEX_DEFAULT;
    
    return 0;
}

PUBLIC int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    attr->pshared = 0;
    attr->kind = 0;
    
    return 0;
}

PUBLIC int pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    *pshared = attr->pshared;
    return 0;
}

PUBLIC int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    attr->pshared = pshared;
    return 0;
}

PUBLIC int pthread_mutexattr_gettype(pthread_mutexattr_t *attr, int *kind) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    *kind = attr->kind;
    return 0;
}

PUBLIC int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int kind) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    attr->kind = kind;
    return 0;
}
