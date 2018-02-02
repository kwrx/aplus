#include <pthread.h>
#include <sys/types.h>
#include <sched.h>
#include <errno.h>

#include "pthread_internal.h"

__dead2
void pthread_exit(void* status) {
    pthread_t th = pthread_self();
    if(th < 0)
        return -1;

    struct pthread_context* cc = (struct pthread_context*) th;
    cc->status = status;

    __pthread_remove_queue(cc);
    _exit(0);
}