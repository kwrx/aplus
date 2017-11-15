#ifndef _CAIRO_WEBP_EXT_H
#define _CAIRO_WEBP_EXT_H

#define CAIRO_HAS_WEBP_FUNCTIONS    1

#ifdef __cplusplus
extern "C" {
#endif


cairo_surface_t* cairo_image_surface_create_from_webp(const char* filename);
cairo_surface_t* cairo_image_surface_create_from_webp_stream(cairo_read_func_t read, void* arg);    

#ifdef __cplusplus
}
#endif

#endif