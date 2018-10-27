#include <stdio.h>
#include <stdlib.h>
#include <wsbe_internal.h>

window_t* window_new(framebuffer_t* cx, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    window_t* win;
    if(!(win = (window_t*) malloc(sizeof(w))))
        die("wsbe: create-window");

    win->cx = cx;
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;

    return win;
}

void window_paint(window_t* w) {
    uint32_t bg = 0xFF000000                |
                  ((rand() % 255) << 16)    |
                  ((rand() % 255) << 8)     |
                  (rand() % 255);

    context_fillrect(w->cx, w->x, w->y, w->w, w->h, bg);
}

