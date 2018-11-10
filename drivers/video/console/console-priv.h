#pragma once

#include <vterm.h>

#define ROW     16
#define COL      8


typedef struct context {
    struct {
        uint16_t width;
        uint16_t height;
        uint16_t bpp;
        uint32_t stride;
        void* framebuffer;
        void* backbuffer;
    } screen;

    struct {
        uint16_t rows;
        uint16_t cols;
    } console;

    struct {
        uint16_t row;
        uint16_t col;
    } cursor;

    struct {
        __attribute__((fastcall))
        void (*clear) (struct context*, uint16_t, uint16_t, uint16_t, uint16_t);

        __attribute__((fastcall))
        void (*move) (struct context*, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t);
        
        __attribute__((fastcall))
        void (*putc) (struct context*, uint16_t, uint16_t, uint32_t, uint32_t, uint32_t);
    } fb;


    VTerm* vt;
    VTermScreen* vs;
} context_t;


int fb_init(context_t* cx);

extern VTermScreenCallbacks cbs;
int console_cbs_damage(VTermRect, void*);