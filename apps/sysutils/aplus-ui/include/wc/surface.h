#ifndef _WC_RESOURCE_H
#define _WC_RESOURCE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wc_surface_t {

    uint64_t id;
    uint64_t refs;

    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t size;
    uint32_t* data;

};


int wc_surface_from_image(wc_surface_t** surface, const char* path);
int wc_surface_from_image_from_memory(wc_surface_t** surface, const void* data, size_t size);
int wc_surface_create(wc_surface_t** surface, const void* data, size_t size, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp);
int wc_surface_destroy(wc_surface_t* surface);

wc_surface_t* wc_surface_ref(wc_surface_t* surface);

#ifdef __cplusplus
}
#endif

#endif