#ifndef _WC_BACKEND_H
#define _WC_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <wc/color.h>
#include <wc/surface.h>

typedef struct wc_backend {

    const char* identifier;
    const char* version;

    int (*init)(void);
    int (*deinit)(void);

    struct {
        int (*clear) (wc_color_t color);
        int (*swap_buffers) (wc_surface_t* surface);
    } output;

    struct {

    } renderer;
        
    struct {

    } cursor;



} wc_backend_t;

#ifdef __cplusplus
}
#endif

#endif