#pragma once

#include <stdint.h>
#include <stdio.h>
#include <aplus/base.h>
#include <aplus/input.h>
#include <aplus/dmx.h>
#include <cairo/cairo.h>
#include <pthread.h>
#include <list.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H

#define VERBOSE 1

typedef struct dmx_rect {
    double x, y, w, h;
} dmx_rect_t;

typedef struct dmx_window {
    double x, y;
    double w, h;
    double alpha;

    cairo_surface_t* surface;
    int flags;
} dmx_window_t;

typedef struct dmx_font {
    char family[64];
    char subfamily[64];
    void* buffer;
    size_t bufsiz;
} dmx_font_t;


typedef struct {
    int fd;
    uint16_t width;
    uint16_t height;
    uint16_t bpp;
    uint32_t stride;
    
    cairo_format_t format;
    cairo_t* backbuffer;
    cairo_t* frontbuffer;

    cairo_surface_t* cursors[DMX_CURSOR_COUNT];
    uint16_t cursor_index;
    uint16_t cursor_x;
    uint16_t cursor_y;

    dmx_window_t* window_top;
    dmx_window_t* window_focused;

    pthread_t th_server;
    pthread_t th_render;
    pthread_t th_cursor;

    FT_Library ft_library;
    FT_Face ft_cache[32];

    list(dmx_font_t*, ft_fonts);
    list(dmx_context_t*, clients);
    list(dmx_window_t*, windows);
    list(dmx_rect_t*, dirtyrects);

    int redraw;
} dmx_t;






int init_render(dmx_t* dmx);
int init_cursor(dmx_t* dmx);
int init_fontengine(dmx_t* dmx);
int init_server(dmx_t* dmx);

void* th_render(void* arg);
void* th_cursor(void* arg);
void* th_server(void* arg);
void* th_main(void* arg);


void dmx_mark_window(dmx_t* dmx, dmx_window_t* wnd, dmx_rect_t* subrect);
void dmx_blit_window(dmx_t* dmx, dmx_window_t* wnd);

int dmx_font_obtain(dmx_t* dmx, FT_Face* face, char* family, char* style);

#if VERBOSE
#define TRACE(x...) {                                           \
    fprintf(stdout, "dmx: #%02d %s() ", getpid(), __func__);     \
    fprintf(stdout, x);                                         \
}
#else
#define TRACE(x...) ((void) 0)
#endif
