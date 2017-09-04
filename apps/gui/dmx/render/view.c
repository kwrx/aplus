#include "../dmx.h"
#include <stdlib.h>

void dmx_blit_view(dmx_t* dmx, dmx_context_t* wnd) {
    if(unlikely(!wnd))
        return;

    if(unlikely(wnd->flags & DMXWF_HIDE))
        return;

    if(unlikely(!wnd->window.frame))
        return;
    
    cairo_surface_t* surface = cairo_image_surface_create_for_data (
        (unsigned char*) wnd->window.frame,
        dmx->format,
        wnd->window.w,
        wnd->window.h,
        wnd->window.w * (dmx->bpp / 8)
    );

    cairo_save(dmx->backbuffer);
    cairo_set_source_surface(dmx->backbuffer, surface, wnd->window.x, wnd->window.y);
    cairo_paint_with_alpha(dmx->backbuffer, wnd->window.alpha);
    cairo_restore(dmx->backbuffer);

    dmx->redraw = 1;
}

void dmx_mark_view(dmx_t* dmx, dmx_context_t* wnd, dmx_rect_t* subrect) {
    dmx_rect_t* r = (dmx_rect_t*) malloc(sizeof(dmx_rect_t));
    if(!r) {
        TRACE("no memory left\n");
        return;
    }

    if(subrect) {
        r->x = wnd->window.x + subrect->x;
        r->y = wnd->window.y + subrect->y;
        r->w = subrect->w;
        r->h = subrect->h;
    } else {
        r->x = wnd->window.x;
        r->y = wnd->window.y;
        r->w = wnd->window.w;
        r->h = wnd->window.h;
    }

    list_push(dmx->dirtyrects, r);
}