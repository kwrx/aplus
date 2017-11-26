#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cairo/cairo.h>

#include <webp/decode.h>
#include <webp/encode.h>
#include <webp/types.h>

#include <aplus/base.h>
#include <aplus/cairo-ext/cairo-webp.h>
#include <aplus/cairo-ext/cairo-cache.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif


extern cairo_surface_t* _cairo_surface_create_in_error(cairo_status_t);


static cairo_surface_t* _cairo_image_surface_decode_webp(void* src, size_t size) {

    int w, h;
    if(!WebPGetInfo((const uint8_t*) src, size, &w, &h))
        return _cairo_surface_create_in_error(CAIRO_STATUS_READ_ERROR); 


    uint8_t* frame;
    if(!(frame = WebPDecodeBGRA (
        (const uint8_t*) src, size,
        &w, &h
    ))) {
        return _cairo_surface_create_in_error(CAIRO_STATUS_READ_ERROR);
    }


    cairo_surface_t* surface = cairo_image_surface_create_for_data (
        frame,
        CAIRO_FORMAT_ARGB32,
        w,
        h,
        w * 4
    );

    if(!surface)
        WebPFree(frame);

    return surface;
}

__attribute__((weak))
cairo_surface_t* cairo_image_surface_create_from_webp(const char* filename) {
    cairo_surface_t* surface;
    if((surface = cairo_cache_obtain_resource(filename)))
        return surface;

    
    int fd = open(filename, O_RDONLY | O_BINARY);
    if(fd < 0)
        return _cairo_surface_create_in_error(CAIRO_STATUS_FILE_NOT_FOUND);


    struct stat st;
    fstat(fd, &st);

    void* buf = (void*) __libaplus_malloc(st.st_size);
    if(!buf) {
        close(fd);
        return _cairo_surface_create_in_error(CAIRO_STATUS_NO_MEMORY);
    }

    if(read(fd, buf, st.st_size) != st.st_size) {
        close(fd);
        free(buf);
        return _cairo_surface_create_in_error(CAIRO_STATUS_READ_ERROR);
    }

    close(fd);


    surface = _cairo_image_surface_decode_webp(buf, st.st_size);
    free(buf);

    cairo_cache_put_resource(filename, surface);
    return surface;
}


__attribute__((weak))
cairo_surface_t* cairo_image_surface_create_from_webp_stream(cairo_read_func_t read, void* arg) {
    return NULL;
}   
