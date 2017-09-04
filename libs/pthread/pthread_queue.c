#include "pthread_internal.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


PRIVATE pthread_context_t __current_thread;

PUBLIC pthread_context_t* __pthread_queue = &__current_thread;
PUBLIC uint32_t __pthread_counts = 1;


PRIVATE void __pthread_dnit() {
    pthread_context_t* tmp = __pthread_queue;

    while(tmp) {
        if(tmp != &__current_thread)
            pthread_detach((pthread_t) tmp);
        
        tmp = tmp->next;
    }
}

PUBLIC void __pthread_init_queue() {
    static int init = 0;
    if(init)
        return;

    init++;

    __current_thread.tid = getpid();
    __current_thread.entry = NULL;
    __current_thread.param = NULL;
    __current_thread.exitval = NULL;
    __current_thread.once.done = 0;
    __current_thread.once.started = -1;
    __current_thread.cond = NULL;
    __current_thread.next = NULL;

    memset(&__current_thread.attr, 0, sizeof(pthread_attr_t));
    atexit(__pthread_dnit);
}
