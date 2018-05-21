#include "pthread_internal.h"


int	pthread_join (pthread_t th, void** __value_ptr) {
    if(th <= 0)
        return -1;

    struct p_context* cc = (struct p_context*) th;
    return waitpid(cc->pid, (int*) __value_ptr, 0);
}