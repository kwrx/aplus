#include "dmx.h"


void* th_render(void* arg) {
    dmx_t* dmx = (dmx_t*) arg;

    for(;;) {
        list_each(dmx->clients, v) {
            if(!v->dirty)
                continue;

            cairo_save(dmx->backbuffer);
            cairo_rectangle(dmx->backbuffer, v->window.x, v->window.y, v->window.w, v->window.h);
            cairo_clip(dmx->backbuffer);
            cairo_new_path(dmx->backbuffer);

            //* DRAW SURFACE

            cairo_restore(dmx->backbuffer);
            dmx->redraw = 1;
        }

        if(!dmx->redraw)
            goto done;

        dmx->redraw = 0;


        cairo_save(dmx->frontbuffer);
        cairo_set_operator(dmx->frontbuffer, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface(dmx->frontbuffer, cairo_get_target(dmx->backbuffer), 0, 0);
        cairo_paint(dmx->frontbuffer);
        cairo_restore(dmx->frontbuffer);
        
#if 0
        memcpy(
                cairo_image_surface_get_data(cairo_get_target(dmx->frontbuffer)),
                cairo_image_surface_get_data(cairo_get_target(dmx->backbuffer)),
                dmx->stride * dmx->height
        );
#endif

        cairo_save(dmx->frontbuffer);
        cairo_set_source_surface(dmx->frontbuffer, dmx->cursors[dmx->cursor_index], dmx->cursor_x, dmx->cursor_y);
        cairo_paint(dmx->frontbuffer);
        cairo_restore(dmx->frontbuffer);
done:
        usleep(16666);
        (void) 0;   /* FIXME: Wait for V-Sync */
    }
}