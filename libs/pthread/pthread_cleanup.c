#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>

#include "pthread_internal.h"


void pthread_cleanup_push(void (*routine) (void*), void* arg) {
    pthread_t ptx = pthread_self();
    pthread_context_t* ctx = (pthread_context_t*) ptx;
    
    struct pthread_cleanup* ct = calloc(sizeof(struct pthread_cleanup), 1);
    if(!ct) {
        errno = ENOMEM;
        return;
    }

    ct->routine = routine;
    ct->arg = arg;
    ct->next = ctx->cleanup_handlers;

    ctx->cleanup_handlers = ct;
}

void pthread_cleanup_pop(int ex) {
    pthread_t ptx = pthread_self();
    pthread_context_t* ctx = (pthread_context_t*) ptx;
    
    if(!ctx->cleanup_handlers)
        return;

    if(ex != 0)
        ctx->cleanup_handlers->routine(ctx->cleanup_handlers->arg);

    struct pthread_cleanup* tmp = ctx->cleanup_handlers;
    ctx->cleanup_handlers = ctx->cleanup_handlers->next;

    free(tmp);
}