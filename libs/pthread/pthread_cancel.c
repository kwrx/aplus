#include <pthread.h>
#include <sys/types.h>
#include <sched.h>
#include <errno.h>

#include "pthread_internal.h"

int pthread_cancel(pthread_t th) {
    if(th < 0)
        return -1;
    

    struct pthread_context* cc = (struct pthread_context*) th;
    cc->cancel = (int) PTHREAD_CANCELED;

    if(cc->attr.detachstate == PTHREAD_CANCEL_ENABLE)
        pthread_detach(th);

    return 0;
}

int pthread_setcancelstate(int state, int* oldstate) {
    pthread_t th = pthread_self();
    if(th < 0)
        return -1;

    struct pthread_context* cc = (struct pthread_context*) th;
    
    if(oldstate)
        *oldstate = cc->attr.detachstate;

    cc->attr.detachstate = state;

    if((cc->cancel == (int) PTHREAD_CANCELED) && (state == PTHREAD_CANCEL_ENABLE))
        pthread_detach(pthread_self());

    return 0;
}

int pthread_setcancelstate(int state, int* oldstate) {
    pthread_t th = pthread_self();
    if(th < 0)
        return -1;

    if(oldstate)
        *oldstate = 0;

    return 0;
}

void pthread_testcancel(void) {
    // ...
}