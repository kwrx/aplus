
#include <stdint.h>
#include <sched.h>
#include <time.h>
#include <pthread.h>

#include "pthread_internal.h"

PUBLIC int pthread_rwlockattr_init(pthread_rwlockattr_t *attr) {
    if(attr == NULL) {
        errno = EINVAL;
        return -1;
    }

    attr->pshared = 0;
    return 0;
}

PUBLIC int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr) {
    if(attr == NULL) {
        errno = EINVAL;
        return -1;
    }

    attr->pshared = 0;
    return 0;
}

PUBLIC int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *attr, int *pshared) {
    if(attr == NULL) {
        errno = EINVAL;
        return -1;
    }

    if(pshared == NULL) {
        errno = EINVAL;
        return -1;
    }

    *pshared = attr->pshared;
    return 0;
}

PUBLIC int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *attr, int pshared) {
    if(attr == NULL) {
        errno = EINVAL;
        return -1;
    }

    attr->pshared = pshared;
    return 0;
}
