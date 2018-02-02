#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <sched.h>
#include <errno.h>

#include "pthread_internal.h"

int pthread_detach(pthread_t th) {
    pthread_t th = pthread_self();
    if(th < 0)
        return -1;

    struct pthread_context* cc = (struct pthread_context*) th;

    if(kill(cc->pid, SIGKILL) != 0)
        return -1;

    return 0;
}