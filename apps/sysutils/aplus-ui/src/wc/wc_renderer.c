#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <wc/wc.h>
#include <wc/wc_display.h>
#include <wc/wc_input.h>
#include <wc/wc_renderer.h>
#include <wc/wc_window.h>

#include <cairo/cairo.h>



// static int wc_renderer_mirror_create() {
//     // TODO: implement
//     errno = ENOSYS;
//     return -1;
// }


// static int wc_renderer_extend_create() {

//     cairo_t* cr;
//     cairo_surface_t* surface;


//     uint32_t width = 0;
//     uint32_t height = 0;

//     for(wc_display_t* d = wc_display_primary(); d; d = wc_display_next(d)) {

//         width = wc_max(width, d->offset_x + d->width);
//         height = wc_max(height, d->offset_y + d->height);

//         wc_ref_dec(&d->ref);

//     }

//     surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

//     if(!surface) {
//         errno = ENOMEM;
//         return -1;
//     }

//     cr = cairo_create(surface);

//     if(!cr) {
//         cairo_surface_destroy(surface);
//         errno = ENOMEM;
//         return -1;
//     }


//     cairo_save(cr);
//     cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
//     cairo_rectangle(cr, 0, 0, width, height);
//     cairo_set_source_rgb(cr, 0, 0, 0);
//     cairo_fill(cr);
//     cairo_restore(cr);

//     return -1;

// }



// int wc_renderer_create(wc_renderer_mode_t mode) {

//     switch(mode) {

//             case WC_RENDERER_MODE_MIRROR:
//                 return wc_renderer_mirror_create();

//             case WC_RENDERER_MODE_EXTEND:
//                 return wc_renderer_extend_create();

//             default:
//                 return errno = EINVAL, -1;

//     }

// }


int wc_renderer_create(struct wc_renderer **renderer, struct wc_display *display) {

    assert(display);
    assert(renderer);


    cairo_t *cr              = NULL;
    cairo_surface_t *surface = NULL;

    surface = cairo_image_surface_create(cairo_image_surface_get_format(display->surface), cairo_image_surface_get_width(display->surface), cairo_image_surface_get_height(display->surface));

    if (!surface) {
        goto fail;
    }

    if (!(cr = cairo_create(surface))) {
        goto fail;
    }

    if (!(*renderer = calloc(1, sizeof(struct wc_renderer)))) {
        goto fail;
    }


    (*renderer)->display = display;
    (*renderer)->surface = surface;
    (*renderer)->cr      = cr;

    wc_ref_init(&(*renderer)->ref, wc_renderer_destroy, *renderer);


    return 0;

fail:

    if (surface) {
        cairo_surface_destroy(surface);
    }

    if (cr) {
        cairo_destroy(cr);
    }

    if (*renderer) {
        free(*renderer);
    }

    return -1;
}


int wc_renderer_destroy(struct wc_renderer *renderer) {

    assert(renderer);

    if (renderer->cr) {
        cairo_destroy(renderer->cr);
    }

    if (renderer->surface) {
        cairo_surface_destroy(renderer->surface);
    }

    if (renderer->display) {
        wc_ref_dec(&renderer->display->ref);
    }

    free(renderer);

    return 0;
}


int wc_renderer_flush(struct wc_renderer *renderer) {

    assert(renderer);
    assert(renderer->surface);
    assert(renderer->display);
    assert(renderer->display->cr);
    assert(renderer->display->surface);


    wc_cursor_render(renderer);

    cairo_set_operator(renderer->display->cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(renderer->display->cr, renderer->surface, 0, 0);
    cairo_paint(renderer->display->cr);



    cairo_surface_flush(renderer->display->surface);


    return 0;
}


int wc_renderer_clear(struct wc_renderer *renderer, double r, double g, double b) {

    assert(renderer);
    assert(renderer->cr);
    assert(renderer->surface);

    cairo_set_source_rgb(renderer->cr, 0, 0, 0);
    cairo_set_operator(renderer->cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(renderer->cr);
    cairo_set_operator(renderer->cr, CAIRO_OPERATOR_OVER);

    return 0;
}
