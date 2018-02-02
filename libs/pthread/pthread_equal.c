#include <pthread.h>
#include <sys/types.h>
#include <sched.h>
#include <errno.h>

#include "pthread_internal.h"

int pthread_equal(pthread_t t1, pthread_t t2) {
    return (t1 == t2);
}