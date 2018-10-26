#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include <aplus/base.h>
#include <aplus/fb.h>


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



void die(char* e) {
    perror(e);
    exit(-1);
}


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



window_t* create_window(framebuffer_t* cx, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
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



int main(int argc, char** argv, char** env) {
    int fb = open("/dev/fb0", O_RDONLY);
    if(fb < 0)
        die("wsbe: screen-device");

    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    ioctl(fb, FBIOGET_VSCREENINFO, &var);
    ioctl(fb, FBIOGET_FSCREENINFO, &fix);
    close(fb);

    if(!fix.smem_start)
        die("wsbe: open-display");

    framebuffer_t context;
    context.w = var.xres;
    context.h = var.yres;
    context.buffer = (uint32_t*) fix.smem_start;

    window_t* w1 = create_window(&context, 10, 10, 300, 200);
    window_t* w2 = create_window(&context, 100, 150, 400, 400);
    window_t* w3 = create_window(&context, 200, 100, 200, 600);

    window_paint(w1);
    window_paint(w2);
    window_paint(w3);

    return 0;
} 

