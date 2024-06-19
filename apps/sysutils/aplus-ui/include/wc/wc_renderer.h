#ifndef _WC_RENDERER_H
#define _WC_RENDERER_H

#include <wc/wc.h>
#include <wc/wc_display.h>
#include <cairo/cairo.h>


#ifdef __cplusplus
extern "C" {
#endif


#define WC_RENDERER_MODE_MIRROR         0
#define WC_RENDERER_MODE_EXTEND         1


typedef uint8_t wc_renderer_mode_t;


typedef struct wc_renderer {

    struct wc_display* display;
    // struct wc_font* font;

    cairo_surface_t* surface;
    cairo_t* cr;

    wc_ref_t ref;

} wc_renderer_t;


int wc_renderer_create(struct wc_renderer** renderer, struct wc_display* display);
int wc_renderer_destroy(struct wc_renderer* renderer);
int wc_renderer_flush(struct wc_renderer* renderer);
int wc_renderer_clear(struct wc_renderer* renderer, double r, double g, double b);


#ifdef __cplusplus
}
#endif

#endif