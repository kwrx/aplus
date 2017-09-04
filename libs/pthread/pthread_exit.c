
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

#include "pthread_internal.h"

PUBLIC void pthread_exit(void* value_ptr) {
    pthread_t ptx = pthread_self();
    pthread_context_t* ctx = (pthread_context_t*) ptx;

    if(ptx) {
        ctx->exitval = value_ptr;
        pthread_detach(ptx);
    }else
        abort();

    for(;;);
}
