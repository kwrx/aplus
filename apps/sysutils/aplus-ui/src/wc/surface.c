#include <stdlib.h>
#include <errno.h>
#include <wc/surface.h>
#include <stb_image.h>



int wc_surface_from_image(wc_surface_t** surface, const char* path) {
    
    int w;
    int h;
    int c;

    void* image;

    if((image = stbi_load(path, &w, &h, &c, 4)) == NULL) {
        return errno = EIO, -1;
    }

    if(wc_surface_create(surface, image, w * h * c, w, h, w * c, c) == -1) {
        return stbi_image_free(image), -1;
    }

    return 0;

}

int wc_surface_from_image_from_memory(wc_surface_t** surface, const void* data, size_t size) {
    
    int w;
    int h;
    int c;

    void* image;

    if((image = stbi_load_from_memory(data, size, &w, &h, &c, 4) == NULL)) {
        return errno = EIO, -1;
    }

    if(wc_surface_create(surface, image, w * h * c, w, h, w * c, c) == -1) {
        return stbi_image_free(image), -1;
    }

    return 0;

}

int wc_surface_create(wc_surface_t** surface, const void* data uint32_t width, uint32_t height, uint32_t bpp) {
    
    static uint64_t __wc_surface_id = 1;

    assert(surface);
    assert(data);
    assert(width * height * bpp);


    wc_surface_t* s = calloc(1, sizeof(wc_surface_t));

    if(!s) {
        return errno = ENOMEM, -1;
    }

    s->data = data;
    s->width = width;
    s->height = height;
    s->bpp = bpp;
    s->pitch = width * bpp;
    s->size = width * height * bpp;
    
    s->refs = 1;
    s->id = __wc_surface_id++;

    return *surface = s, 0;


}

int wc_surface_destroy(wc_surface_t* surface) {
    
    assert(surface);

    if(--surface->refs <= 0) {

        if(surface->data) {
            free(surface->data);
        }

        free(surface);

    }

    return 0;

}

wc_surface_t* wc_surface_ref(wc_surface_t* surface) {
    
    assert(surface);

    surface->refs++;

    return surface;

}
