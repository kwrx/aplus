#pragma once

#include <stdint.h>
#include <stdio.h>
#include <pthread.h>


#include <aplus/base.h>
#include <aplus/input.h>
#include <aplus/fb.h>
#include <aplus/dmx.h>
#include <aplus/sysconfig.h>
#include <aplus/utils/list.h>
#include <aplus/cairo-ext/cairo-webp.h>
#include <aplus/cairo-ext/cairo-rounded-rectangle.h>

#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>



#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H

#define VERBOSE 1



typedef struct dmx_font {
    char family[64];
    char path[256];
    void* cache;
    size_t cachesize;
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

    dmx_gc_t* window_top;
    dmx_gc_t* window_focused;

    pthread_t th_render;
    pthread_t th_input;

    FT_Library ft_library;
    FT_Face ft_cache[32];

    list(dmx_font_t*, fonts);
    list(dmx_gc_t*, clients);

    int redraw;
} dmx_t;







void* th_render(void* arg);
void* th_input(void* arg);


void dmx_proto_ack(int type, pid_t pid, void* arg);
void dmx_proto_disconnect_client(dmx_t* dmx, dmx_packet_connection_t* pk);
void dmx_proto_create_gc(dmx_t* dmx, dmx_packet_gc_t* pk);
void dmx_proto_destroy_gc(dmx_t* dmx, dmx_packet_gc_t* pk);

dmx_gc_t* dmx_gc_alloc(dmx_t* dmx, pid_t pid, double width, double height);
void dmx_gc_free(dmx_gc_t* gc);

void dmx_wm_draw_borders(dmx_t* dmx, dmx_gc_t* gc);    