#include "pthread_internal.h"

static int __pthread_initialized = 0;
struct p_context* __pthread_queue = NULL;

static void __pthread_atexit(void) {
    struct p_context* tmp;
    for(tmp = __pthread_queue; tmp; tmp = tmp->next)
        pthread_detach((pthread_t) tmp);
}

void __pthread_init(void) {
    if(__pthread_initialized)
        return;

    __pthread_initialized = 1;
    atexit(__pthread_atexit);
}

void __pthread_add_queue(struct p_context* cc) {
    cc->next = __pthread_queue;
    __pthread_queue = cc;
}

void __pthread_remove_queue(struct p_context* cc) {
    struct p_context* p = cc->next;

    if(cc == __pthread_queue)
        cc = p;
    else {
        struct p_context* tmp;
        for(tmp = __pthread_queue; tmp->next; tmp = tmp->next) {
            if(tmp->next != cc)
                continue;

            cc->next = p;
            break;
        } 
    }
}
