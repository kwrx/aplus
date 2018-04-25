#include "pthread_internal.h"

int pthread_equal(pthread_t t1, pthread_t t2) {
    return (t1 == t2);
}

int	pthread_getcpuclockid (pthread_t thread, clockid_t *clock_id) {
    if(clock_id)
        *clock_id = CLOCK_MONOTONIC;

    return 0;
}


int	pthread_setconcurrency (int new_level) {
    return 0;
}

int	pthread_getconcurrency (void) {
    return 0;
}

void pthread_yield (void) {
    sched_yield();
}