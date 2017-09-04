
#include <stdint.h>
#include <pthread.h>

#include "pthread_internal.h"

PUBLIC int pthread_condattr_init(pthread_condattr_t *attr) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    attr->pshared = 0;
    return 0;
}


PUBLIC int pthread_condattr_destroy(pthread_condattr_t *attr) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    attr->pshared = 0;
    return 0;
}


PUBLIC int pthread_condattr_getpshared(const pthread_condattr_t *attr, int *pshared) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    
    *pshared = attr->pshared;
    return 0;
}


PUBLIC int pthread_condattr_setpshared(pthread_condattr_t *attr, int pshared) {
    if(!attr) {
        errno = EINVAL;
        return -1;
    }

    attr->pshared = pshared;
    return 0;
}
