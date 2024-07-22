#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <wc/wc.h>
#include <wc/wc_cursor.h>
#include <wc/wc_input.h>

#include <cairo/cairo-webp.h>
#include <cairo/cairo.h>


static struct {

    uint16_t type;
    uint16_t fallback;

    struct {
        cairo_surface_t* surface;
    } types[WC_CURSOR_TYPE_LENGTH];

} cursor;


int wc_cursor_initialize(void) {

    cursor.type = WC_CURSOR_TYPE_NONE;

    for (int i = 0; i < WC_CURSOR_TYPE_LENGTH; i++) {
        cursor.types[i].surface = NULL;
    }


    LOG("cursor subsystem initialized\n");

    return 0;
}


int wc_cursor_set_fallback(uint16_t fallback) {

    if (fallback >= WC_CURSOR_TYPE_LENGTH) {
        return errno = EINVAL, -1;
    }

    if (cursor.types[fallback].surface == NULL) {
        return errno = EINVAL, -1;
    }

    cursor.fallback = fallback;

    return 0;
}


int wc_cursor_set_type(uint16_t type) {

    if (type >= WC_CURSOR_TYPE_LENGTH) {
        return errno = EINVAL, -1;
    }

    if (cursor.types[type].surface == NULL) {
        type = cursor.fallback;
    }

    cursor.type = type;

    return 0;
}


int wc_cursor_load(uint16_t type, const char* path) {

    assert(path);
    assert(type < WC_CURSOR_TYPE_LENGTH);

    if (cursor.types[type].surface) {
        cairo_surface_destroy(cursor.types[type].surface);
    }

    if (!(cursor.types[type].surface = cairo_image_surface_create_from_webp(path))) {
        return -1;
    }


    LOG("Added cursor type %d from %s\n", type, path);

    return 0;
}


int wc_cursor_render(wc_renderer_t* renderer) {

    assert(renderer);

    if (cursor.type == WC_CURSOR_TYPE_NONE) {
        return 0;
    }

    cairo_surface_t* surface = cursor.types[cursor.type].surface;

    if (!surface) {
        return 0;
    }

    cairo_save(renderer->cr);
    cairo_set_operator(renderer->cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_surface(renderer->cr, surface, wc_input_cursor_x(), wc_input_cursor_y());
    cairo_paint(renderer->cr);
    cairo_restore(renderer->cr);

    return 0;
}
