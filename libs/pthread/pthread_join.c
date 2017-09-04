
#include <stdint.h>
#include <sched.h>
#include <pthread.h>

#include "pthread_internal.h"

PUBLIC int pthread_join(pthread_t thread, void **value_ptr) {
    if(!thread) {
        errno = EINVAL;
        return -1;
    }

    pthread_context_t* ctx = (pthread_context_t*) thread;
    while(ctx->once.done == 0)
        sched_yield();

    if(value_ptr)
        *value_ptr = ctx->exitval;

    return 0;
}
