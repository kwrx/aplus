#include "dmx.h"
#include <aplus/kmem.h>

static pid_t gcid = 0;

dmx_gc_t* dmx_gc_alloc(dmx_t* dmx, pid_t pid, double width, double height) {
    dmx_gc_t* gc = (dmx_gc_t*) kmem_alloc(sizeof(dmx_gc_t) + (width * height * (dmx->bpp / 8)));
    if(!gc)
        return NULL;
        
    gc->gcid = gcid++;
    gc->pid = pid;
    gc->window.w = width;
    gc->window.h = height;
    gc->window.x = 0;
    gc->window.y = 0;
    gc->window.alpha = 1.0;
    gc->window.font = DMX_FONT_TYPE_REGULAR | DMX_FONT_WEIGHT_REGULAR | DMX_FONT_STYLE_NORMAL;
    gc->window.title[0] = '\0';
    gc->window.frame = (void*) ((uintptr_t) gc + sizeof(dmx_gc_t));

    gc->screen.width = dmx->width;
    gc->screen.height = dmx->height;
    gc->screen.bpp = dmx->bpp;
    gc->screen.stride = dmx->stride;
    gc->screen.format = dmx->format;

    gc->flags = 0;
    gc->dirty = 0;
    return gc;
}

void dmx_gc_free(dmx_gc_t* gc) {
    kmem_free(gc);
}