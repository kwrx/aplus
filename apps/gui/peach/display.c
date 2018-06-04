#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <aplus/base.h>
#include <aplus/fb.h>
#include "peach.h"


peach_display_t display[NR_DISPLAY];
static int init = 0;

static void __init_display() {
    memset(display, 0, sizeof(display));
    init++;
}

void init_display(uint16_t w, uint16_t h, uint16_t bpp, void* framebuffer, int doublebuffer) {
    if(!init)
        __init_display();

    int i;
    for(i = 0; i < NR_DISPLAY; i++)
        if(!display[i].d_active)
            break;

    if(i == NR_DISPLAY)
        die("peach: no display left!");    



    display[i].d_active = 1;
    display[i].d_width = w;
    display[i].d_height = h;
    display[i].d_bpp = bpp;
    display[i].d_stride = w * (bpp / 8);

    display[i].d_backbuffer = !doublebuffer
                                    ? (void*) framebuffer
                                    : malloc(w * h * (bpp / 8))
                                    ;

    display[i].d_framebuffer = !doublebuffer
                                    ? NULL 
                                    : framebuffer
                                    ;

    if(!display[i].d_backbuffer)
        die("peach: no backbuffer allocated");
}
