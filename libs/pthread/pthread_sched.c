
#include <stdint.h>
#include <sched.h>
#include <pthread.h>

#include "pthread_internal.h"

EXTERN int sched_setparam(pid_t pid, const struct sched_param *param);
EXTERN int sched_getparam(pid_t pid, const struct sched_param *param);

PUBLIC int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param *param) {
    if(!thread) {
        errno = EINVAL;
        return -1;
    }

    if(param == NULL) {
        errno = EINVAL;
        return -1;
    }

    pthread_context_t* ctx = (pthread_context_t*) thread;
    return sched_setparam(ctx->tid, param);
}

PUBLIC int pthread_getschedparam(pthread_t thread, int *policy, struct sched_param *param) {
    if(!thread) {
        errno = EINVAL;
        return -1;
    }

    if(param == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    pthread_context_t* ctx = (pthread_context_t*) thread;
    if((*policy = sched_getparam(ctx->tid, param)) != -1)
        return 0;
    
    return -1;
}

PUBLIC int pthread_setconcurrency(int level) {
    pthread_t thread = pthread_self();
    if(!thread) {
        errno = ESRCH;
        return -1;
    }

    pthread_context_t* ctx = (pthread_context_t*) thread;
    ctx->attr.param.sched_priority = level;

    return 0;
}

PUBLIC int pthread_getconcurrency(void) {
    pthread_t thread = pthread_self();
    if(!thread) {
        errno = ESRCH;
        return -1;
    }

    pthread_context_t* ctx = (pthread_context_t*) thread;
    return ctx->attr.param.sched_priority;
}
