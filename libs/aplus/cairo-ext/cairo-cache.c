#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


#include <cairo/cairo.h>

#include <aplus/base.h>
#include <aplus/utils/hashmap.h>
#include <aplus/cairo-ext/cairo-cache.h>

hashmap_t hm_cache = NULL;

#define __init() {                              \
        if(!hm_cache)                           \
            hm_cache = hashmap_new();           \
    }


cairo_surface_t* cairo_cache_obtain_resource(const char* path) {
    __init();

    cairo_surface_t* R;
    if(hashmap_get(hm_cache, (char*) path, (any_t*) &R) != HM_OK)
        return NULL;

    void* out = (void*) __libaplus_malloc (
        cairo_image_surface_get_stride(R) * cairo_image_surface_get_height(R)
    );

    if(!out)
        return NULL;

    memcpy (
        out, 
        cairo_image_surface_get_data(R), 
        cairo_image_surface_get_stride(R) * cairo_image_surface_get_height(R));


    return cairo_image_surface_create_for_data (
        out,
        cairo_image_surface_get_format(R),
        cairo_image_surface_get_width(R),
        cairo_image_surface_get_height(R),
        cairo_image_surface_get_stride(R)   
    );
}


void cairo_cache_put_resource(const char* path, cairo_surface_t* surface) {
    __init();

    hashmap_put(hm_cache, (char*) path, (any_t) cairo_surface_reference(surface));
}
