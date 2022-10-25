#ifndef _WC_DISPLAY_H
#define _WC_DISPLAY_H

#include <wc/wc.h>
#include <cairo/cairo.h>
#include <aplus/fb.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WC_DISPLAY_MAX                  16

typedef struct wc_display {

    int fd;
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;

    size_t offset_x;
    size_t offset_y;
    size_t width;
    size_t height;

    cairo_surface_t* surface;
    cairo_t* cr;

    wc_ref_t ref;


    struct wc_display* next;

} wc_display_t;


int wc_display_initialize(void);

struct wc_display* wc_display_primary();
struct wc_display* wc_display_at_position(uint16_t x, uint16_t y);


#ifdef __cplusplus
}
#endif

#endif