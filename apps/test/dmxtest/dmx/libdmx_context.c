#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/sched.h>
#include <errno.h>

#include <aplus/base.h>
#include <aplus/kmem.h>
#include <aplus/input.h>
#include <aplus/msg.h>

#include <aplus/utils/list.h>
#include "libdmx.h"

int dmx_context_sync(dmx_context_t* ctx) {
    time_t timeout = time(NULL) + LIBDMX_TIMEOUT;
    while(ctx->lock && (timeout > time(NULL)))
        sched_yield();

    if(ctx->lock) {
        LIBDMX_ERROR("Server does not responding", ETIMEDOUT);
        return -1;
    }

    ctx->cmd = DMX_PROTO_NULL;
    ctx->arg = 0;

    return 0;
}

int dmx_context_open(int fd, dmx_context_t** ctxp) {
    if(!ctxp) {
        LIBDMX_ERROR(NULL, EINVAL);
        return -1;
    }

    dmx_context_t* ctx = (dmx_context_t*) kmem_alloc(sizeof(dmx_context_t));
    if(!ctx)
        return -1;
    

    memset(ctx, 0, sizeof(dmx_context_t));
    ctx->pid = getpid();
    ctx->flags = DMXWF_HIDE;
    ctx->cmd = DMX_PROTO_CONNECT;
    ctx->arg = 0;
    ctx->lock++;


    *(ctxp) = ctx;
    if(write(fd, ctxp, sizeof(ctxp)) < 0) {
        LIBDMX_ERROR("Server I/O Error", 0);
        return -1;
    }

    if(dmx_context_sync(ctx) != 0)
        return -1;

    list_push(contexts, ctx);
    return 0; 
}

int dmx_context_close(dmx_context_t* ctx) {
    if(!ctx) {
        LIBDMX_ERROR(NULL, EINVAL);
        return -1;
    }

    if(ctx->lock) {
        LIBDMX_ERROR("Context already locked", EBUSY);
        return -1;
    }


    ctx->cmd = DMX_PROTO_CLOSE;
    ctx->arg = 0;
    ctx->lock++;

    list_remove(contexts, ctx);
    return 0;
}