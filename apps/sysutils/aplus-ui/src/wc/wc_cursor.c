#include <wc/wc.h>
#include <wc/wc_cursor.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <cairo/cairo.h>
#include <cairo/cairo-webp.h>


int wc_cursor_create(struct wc_cursor** cursor) {

    assert(cursor);

    if(!(*cursor = calloc(1, sizeof(struct wc_cursor)))) {
        return errno = ENOMEM, -1;
    }

    wc_ref_init(&(*cursor)->ref, wc_cursor_destroy, *cursor);

    return 0;

}

int wc_cursor_destroy(struct wc_cursor* cursor) {

    assert(cursor);

    for(size_t i = 0; i < WC_CURSOR_TYPE_LENGTH; i++) {

        if(cursor->cursors[i].surface) {
            cairo_surface_destroy(cursor->cursors[i].surface);
        }

    }

    free(cursor);

    return 0;

}

int wc_cursor_add_type(struct wc_cursor* cursor, uint16_t type, const char* path) {

    assert(cursor);
    assert(path);
    assert(type < WC_CURSOR_TYPE_LENGTH);

    if(cursor->cursors[type].surface) {
        cairo_surface_destroy(cursor->cursors[type].surface);
    }

    if(!(cursor->cursors[type].surface = cairo_image_surface_create_from_webp(path))) {
        return -1;
    }


    LOG("Added cursor type %d from %s\n", type, path);

    return 0;

}

