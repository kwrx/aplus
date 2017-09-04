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


void dmx_view_set_size(dmx_context_t* ctx, double w, double h) {
    ctx->window.w = w;
    ctx->window.h = h;

    if(ctx->window.frame)
        kmem_free(ctx->window.frame);

    ctx->window.frame = (void*) kmem_alloc(w * h * (ctx->screen.bpp / 8));
    if(!ctx->window.frame) {
        LIBDMX_ERROR("No System Memory available", ENOMEM);
        return;
    }

    dmx_view_invalidate(ctx);
}

void dmx_view_set_position(dmx_context_t* ctx, double x, double y) {
    ctx->window.x = x;
    ctx->window.y = y;
}

void dmx_view_set_alpha(dmx_context_t* ctx, double alpha) {
    ctx->window.alpha = alpha;
}

void dmx_view_set_font(dmx_context_t* ctx, uint16_t font_index) {
    ctx->window.font = font_index;
}

void dmx_view_show(dmx_context_t* ctx) {
    ctx->flags &= ~DMXWF_HIDE;
}

void dmx_view_hide(dmx_context_t* ctx) {
    ctx->flags |= DMXWF_HIDE;
}

void dmx_view_set_widget(dmx_context_t* ctx, dmx_widget_t* root_widget) {
    
}

int dmx_view_invalidate(dmx_context_t* ctx) {
    if(ctx->lock) {
        LIBDMX_ERROR("Context already locked", EBUSY);
        return -1;
    }

    
    ctx->cmd = DMX_PROTO_INVALIDATE;
    ctx->arg = 0;
    ctx->lock++;

    if(dmx_context_sync(ctx) != 0)
        return -1;

    return 0;
}