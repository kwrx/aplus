#include <stdio.h>
#include <stdlib.h>
#include <wsbe_internal.h>

void context_fillrect(framebuffer_t* cx, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color) {
    uint16_t mx = x + w;
    uint16_t my = y + h;

    if(mx > cx->w)
        mx = cx->w;
    
    if(my > cx->h)
        my = cx->h;

    int i;
    for(; y < my; y++)
        for(i = x; i < mx; i++)
            cx->buffer[y * cx->w + i] = color;
}
