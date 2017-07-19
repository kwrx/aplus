#include "../dmx.h"
#include <stdlib.h>

void dmx_blit_window(dmx_t* dmx, dmx_window_t* wnd) {
    if(unlikely(!wnd))
        return;

    if(unlikely(wnd->flags & DMXWF_HIDE))
        return;

    cairo_save(dmx->backbuffer);
    cairo_set_source_surface(dmx->backbuffer, wnd->surface, wnd->x, wnd->y);
    cairo_paint_with_alpha(dmx->backbuffer, wnd->alpha);
    cairo_restore(dmx->backbuffer);

    dmx->redraw = 1;
}

void dmx_mark_window(dmx_t* dmx, dmx_window_t* wnd, dmx_rect_t* subrect) {
    dmx_rect_t* r = (dmx_rect_t*) malloc(sizeof(dmx_rect_t));
    if(!r) {
        TRACE("no memory left\n");
        return;
    }

    if(subrect) {
        r->x = wnd->x + subrect->x;
        r->y = wnd->y + subrect->y;
        r->w = subrect->w;
        r->h = subrect->h;
    } else {
        r->x = wnd->x;
        r->y = wnd->y;
        r->w = wnd->w;
        r->h = wnd->h;
    }

    r->next = dmx->dirty;
    dmx->dirty = r;
}