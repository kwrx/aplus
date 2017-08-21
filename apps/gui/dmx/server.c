#include "dmx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>


static pid_t nextcid = 0;


int init_server(dmx_t* dmx) {
    TRACE("Initializing Server\n");
    
    if(access(DMX_PIPE, F_OK) == 0) {
        TRACE("server already running\n");
        return -1;
    }

    if(mkfifo(DMX_PIPE, 0666) != 0) {
        TRACE("mkfifo(" DMX_PIPE ") failed!\n");
        return -1;
    }


    dmx->fd = open(DMX_PIPE, O_RDWR);
    if(dmx->fd < 0) {
        TRACE("open(" DMX_PIPE ") failed!\n");
        return -1;
    }

    TRACE("Done!\n");
    return 0;
}


void* th_server(void* arg) {
    TRACE("Running\n");
    dmx_t* dmx = (dmx_t*) arg;

    for(;;) {
        dmx_context_t* ctx;
        if(read(dmx->fd, &ctx, sizeof(ctx)) != sizeof(ctx)) {
            TRACE("warning: I/O error on " DMX_PIPE "\n");
            continue;
        } 

        if(!ctx)
            continue;

        if(!ctx->lock)
            continue;

        switch(ctx->cmd) {
            case DMX_PROTO_NULL:
                break;

            case DMX_PROTO_CONNECT:
                list_push(dmx->clients, ctx);
                
                ctx->cid = nextcid++;
                TRACE("context #%d:%d registered! (%p, %d:%d %dx%d)\n", ctx->pid, ctx->cid, ctx, ctx->x, ctx->y, ctx->width, ctx->height);
                break;

            case DMX_PROTO_CLOSE:
                list_remove(dmx->clients, ctx);

                TRACE("client #%d:%d diconnected!\n", ctx->pid, ctx->cid);
                break;

            default:
                TRACE("client %d:%d sent invalid command\n", ctx->pid, ctx->cid);
                break;
        }

        ctx->lock = 0;
    }
}