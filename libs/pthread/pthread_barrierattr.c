
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "pthread_internal.h"

PUBLIC int pthread_barrierattr_init(pthread_barrierattr_t *attr) {
    if(attr == NULL) {
        errno = EINVAL;
        return -1;
    }

    attr->pshared = 0;
    return 0;
}


PUBLIC int pthread_barrierattr_destroy(pthread_barrierattr_t *attr) {
    if(attr == NULL) {
        errno = EINVAL;
        return -1;
    }

    attr->pshared = 0;
    return 0;
}


PUBLIC int pthread_barrierattr_getpshared(const pthread_barrierattr_t *attr, int *pshared) {
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


PUBLIC int pthread_barrierattr_setpshared(pthread_barrierattr_t *attr, int pshared) {
    if(attr == NULL) {
        errno = EINVAL;
        return -1;
    }

    attr->pshared = pshared;
    return 0;
}
