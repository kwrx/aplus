#include <pthread.h>
#include <sys/types.h>
#include <sched.h>
#include <errno.h>

#include "pthread_internal.h"

pthread_t pthread_self(void) {
    struct p_context* tmp;
    for(tmp = __pthread_queue; tmp; tmp = tmp->next) {
        if(tmp->pid != getpid())
            continue;

        return (pthread_t) tmp;
    }

    errno = ESRCH;
    return -1;
}