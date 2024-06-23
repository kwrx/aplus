#ifndef _CAIRO_WEBP_H
#define _CAIRO_WEBP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cairo/cairo.h>

cairo_surface_t *cairo_image_surface_create_from_webp(const char *filename);

#ifdef __cplusplus
}
#endif

#endif
