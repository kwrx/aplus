#ifndef _WSBE_INTERNAL_H
#define _WSBE_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint32_t* buffer;
    uint16_t w;
    uint16_t h;
} framebuffer_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    framebuffer_t* cx;
} window_t;

typedef struct {
    //list(children, window_t*);
    framebuffer_t* cx;
} desktop_t;


void context_fillrect(framebuffer_t* cx, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color);
void window_paint(window_t* w);
window_t* window_new(framebuffer_t* cx, uint16_t x, uint16_t y, uint16_t w, uint16_t h);

static void die(char* e) {
    perror(e);
    exit(-1);
}

#ifdef __cplusplus
}
#endif
#endif