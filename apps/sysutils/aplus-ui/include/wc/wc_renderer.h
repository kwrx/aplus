#ifndef _WC_RENDERER_H
#define _WC_RENDERER_H

#include <wc/wc.h>
#include <wc/wc_display.h>
#include <wc/wc_cursor.h>
#include <cairo/cairo.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct wc_renderer {

    struct wc_display* display;
    struct wc_cursor* cursor;
    // struct wc_font* font;

    cairo_surface_t* surface;
    cairo_t* cr;

    wc_ref_t ref;

} wc_renderer_t;


struct wc_window;

int wc_renderer_create(struct wc_renderer** renderer, struct wc_display* display);
int wc_renderer_destroy(struct wc_renderer* renderer);
int wc_renderer_flush(struct wc_renderer* renderer);
int wc_renderer_clear(struct wc_renderer* renderer, double r, double g, double b);

int wc_renderer_set_cursor(struct wc_renderer* renderer, struct wc_cursor* cursor);
int wc_renderer_set_cursor_position(struct wc_renderer* renderer, int x, int y);
int wc_renderer_set_cursor_type(struct wc_renderer* renderer, uint16_t type);

#ifdef __cplusplus
}
#endif

#endif