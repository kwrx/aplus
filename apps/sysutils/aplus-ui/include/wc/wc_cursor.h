#ifndef _WC_CURSOR_H
#define _WC_CURSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <cairo/cairo.h>
#include <wc/wc.h>

#define WC_CURSOR_TYPE_NONE     0
#define WC_CURSOR_TYPE_POINTER  1
#define WC_CURSOR_TYPE_TEXT     2
#define WC_CURSOR_TYPE_BUSY     3
#define WC_CURSOR_TYPE_RESIZE   4
#define WC_CURSOR_TYPE_MOVE     5
#define WC_CURSOR_TYPE_HAND     6
#define WC_CURSOR_TYPE_HELP     7
#define WC_CURSOR_TYPE_CUSTOM   8
#define WC_CURSOR_TYPE_LENGTH   9

typedef struct wc_cursor {

    uint16_t x;
    uint16_t y;
    uint16_t type;

    struct {
        cairo_surface_t* surface;
    } cursors[WC_CURSOR_TYPE_LENGTH];

    
    wc_ref_t ref;

} wc_cursor_t;


int wc_cursor_create(struct wc_cursor** cursor);
int wc_cursor_destroy(struct wc_cursor* cursor);
int wc_cursor_add_type(struct wc_cursor* cursor, uint16_t type, const char* path);
int wc_cursor_set_type(struct wc_cursor* cursor, uint16_t type);



#ifdef __cplusplus
}
#endif


#endif