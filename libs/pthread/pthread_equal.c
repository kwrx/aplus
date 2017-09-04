
#include <stdint.h>
#include <pthread.h>

#include "pthread_internal.h"

PUBLIC int pthread_equal(pthread_t t1, pthread_t t2) {
    pthread_context_t* c1 = (pthread_context_t*) t1;
    pthread_context_t* c2 = (pthread_context_t*) t2;

    return (c1->tid == c2->tid);
}
