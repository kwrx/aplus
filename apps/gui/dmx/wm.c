#include "dmx.h"
#include <math.h>

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
            v->dirty = 0;
            dmx->redraw = 1;
        }

        if(!dmx->redraw)
            goto done;

        dmx->redraw = 0;

        cairo_save(dmx->frontbuffer);
        cairo_set_operator(dmx->frontbuffer, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface(dmx->frontbuffer, cairo_get_target(dmx->backbuffer), 0, 0);
        cairo_paint(dmx->frontbuffer);
        cairo_set_source_surface(dmx->frontbuffer, dmx->cursors[dmx->cursor_index], dmx->cursor_x, dmx->cursor_y);
        cairo_paint(dmx->frontbuffer);
        cairo_restore(dmx->frontbuffer);



done:
        usleep(16666);
        (void) 0;   /* FIXME: Wait for V-Sync */
    }
}


void dmx_wm_draw_borders(dmx_t* dmx, dmx_gc_t* gc) {
    cairo_save(dmx->backbuffer);
    cairo_rectangle(dmx->backbuffer, gc->window.x - 3, gc->window.y - 33, gc->window.w + 6, gc->window.h + 36);
    cairo_set_source_rgb(dmx->backbuffer, 0.85, 0.85, 0.85);    
    cairo_set_line_width(dmx->backbuffer, 3.0);
    cairo_stroke(dmx->backbuffer);
    cairo_restore(dmx->backbuffer);

    cairo_save(dmx->backbuffer);
    cairo_font_face_t* ft = cairo_ft_font_face_create_for_ft_face(dmx->ft_cache[gc->window.font], 0);
    if(unlikely(!ft)) {
        fprintf(stderr, "dmx: WTF! %s:%d %s()\n", __FILE__, __LINE__, __func__);
        return;
    }

    cairo_set_font_face(dmx->backbuffer, ft);
    cairo_set_font_size(dmx->backbuffer, 14.0);
    cairo_set_source_rgb(dmx->backbuffer, 0.85, 0.85, 0.85);    
    cairo_move_to(dmx->backbuffer, gc->window.x + 10.0, gc->window.y + 20.0);
    cairo_show_text(dmx->backbuffer, gc->window.title);
    cairo_restore(dmx->backbuffer);

    cairo_save(dmx->backbuffer);    
    cairo_rectangle(dmx->backbuffer, gc->window.x, gc->window.y, gc->window.w, gc->window.h);
    cairo_set_source_rgb(dmx->backbuffer, 0.25, 0.25, 0.25);
    cairo_fill(dmx->backbuffer);
    cairo_restore(dmx->backbuffer);
    
    gc->dirty = 1;
}