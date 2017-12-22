#ifndef _CAIRO_CACHE_H
#define _CAIRO_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cairo/cairo.h>

cairo_surface_t* cairo_cache_obtain_resource(const char* path);    
void cairo_cache_put_resource(const char* path, cairo_surface_t* surface);
    
#ifdef __cplusplus
}
#endif
#endif