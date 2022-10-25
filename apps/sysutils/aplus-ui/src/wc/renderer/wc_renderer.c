#include <wc/wc.h>
#include <wc/wc_display.h>
#include <wc/wc_renderer.h>
#include <wc/wc_window.h>
#include <wc/wc_input.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <cairo/cairo.h>


int wc_renderer_create(struct wc_renderer** renderer, struct wc_display* display) {

    assert(display);
    assert(renderer);


    cairo_t* cr = NULL;
    cairo_surface_t* surface = NULL;

    surface = cairo_image_surface_create(
        cairo_image_surface_get_format(display->surface),
        cairo_image_surface_get_width(display->surface),
        cairo_image_surface_get_height(display->surface)
    );

    if(!surface) {
        goto fail;
    }

    if(!(cr = cairo_create(surface))) {
        goto fail;
    }

    if(!(*renderer = calloc(1, sizeof(struct wc_renderer)))) {
        goto fail;
    }


    (*renderer)->display = display;
    (*renderer)->surface = surface;
    (*renderer)->cr = cr;

    wc_ref_init(&(*renderer)->ref, wc_renderer_destroy, *renderer);


    return 0;

fail:

    if(surface) {
        cairo_surface_destroy(surface);
    }

    if(cr) {
        cairo_destroy(cr);
    }

    if(*renderer) {
        free(*renderer);
    }

    return -1;

}


int wc_renderer_destroy(struct wc_renderer* renderer) {

    assert(renderer);

    if(renderer->cr) {
        cairo_destroy(renderer->cr);
    }

    if(renderer->surface) {
        cairo_surface_destroy(renderer->surface);
    }

    if(renderer->display) {
        wc_ref_dec(&renderer->display->ref);
    }

    if(renderer->cursor) {
        wc_ref_dec(&renderer->cursor->ref);
    }

    free(renderer);

    return 0;

}


int wc_renderer_flush(struct wc_renderer* renderer) {

    assert(renderer);
    assert(renderer->surface);
    assert(renderer->display);
    assert(renderer->display->cr);
    assert(renderer->display->surface);

    cairo_set_operator(renderer->display->cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(renderer->display->cr, renderer->surface, 0, 0);
    cairo_paint(renderer->display->cr);


    if(renderer->cursor) {

        if(renderer->cursor->cursors[renderer->cursor->type].surface) {

            if(wc_input_cursor_x() >= renderer->display->offset_x && wc_input_cursor_x() < renderer->display->offset_x + cairo_image_surface_get_width(renderer->display->surface) &&
               wc_input_cursor_y() >= renderer->display->offset_y && wc_input_cursor_y() < renderer->display->offset_y + cairo_image_surface_get_height(renderer->display->surface)) {

                // Draw cursor
                cairo_save(renderer->display->cr);
                cairo_set_operator(renderer->display->cr, CAIRO_OPERATOR_OVER);
                cairo_set_source_surface(
                    renderer->display->cr,
                    renderer->cursor->cursors[renderer->cursor->type].surface,
                    wc_input_cursor_x() - renderer->display->offset_x,
                    wc_input_cursor_y() - renderer->display->offset_y
                );
                cairo_paint(renderer->display->cr);
                cairo_restore(renderer->display->cr);

            }

        }

    }


    cairo_surface_flush(renderer->display->surface);


    return 0;

}


int wc_renderer_clear(struct wc_renderer* renderer, double r, double g, double b) {

    assert(renderer);
    assert(renderer->cr);
    assert(renderer->surface);

    cairo_set_source_rgb(renderer->cr, 0, 0, 0);
    cairo_set_operator(renderer->cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(renderer->cr);
    cairo_set_operator(renderer->cr, CAIRO_OPERATOR_OVER);

    return 0;

}


int wc_renderer_set_cursor(struct wc_renderer* renderer, struct wc_cursor* cursor) {

    assert(renderer);

    if(renderer->cursor) {
        wc_ref_dec(&renderer->cursor->ref);
    }

    renderer->cursor = cursor;

    if(renderer->cursor) {
        wc_ref_inc(&renderer->cursor->ref);
    }

    return 0;

}

int wc_renderer_set_cursor_position(struct wc_renderer* renderer, int x, int y) {

    assert(renderer);

    if(renderer->cursor) {
        renderer->cursor->x = wc_clamp(renderer->cursor->x + x, 0, renderer->display->var.xres - cairo_image_surface_get_width(renderer->cursor->cursors[renderer->cursor->type].surface));
        renderer->cursor->y = wc_clamp(renderer->cursor->y - y, 0, renderer->display->var.yres - cairo_image_surface_get_height(renderer->cursor->cursors[renderer->cursor->type].surface));
    }

    return 0;

}

int wc_renderer_set_cursor_type(struct wc_renderer* renderer, uint16_t type) {

    assert(renderer);
    assert(type < WC_CURSOR_TYPE_LENGTH);

    if(renderer->cursor) {

        if(!renderer->cursor->cursors[type].surface) {
            return -1;
        }

        renderer->cursor->type = type;

    }

    return 0;

}
