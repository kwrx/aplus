#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include <aplus/base.h>
#include <aplus/fb.h>
#include <aplus/utils/list.h>

#include <wsbe_internal.h>



int main(int argc, char** argv, char** env) {

    static list(int, ls);
    memset(ls, 0, sizeof(ls));

    int i;
    for(i = 1; i <= 5; i++)
        list_push(ls, i);

    list_printf(ls, "%d\n");

    printf("\n get_at(%d): %d\n", 3, list_get_at(ls, 3));
    
    list_remove_at(ls, 3);
    list_printf(ls, "%d\n");
    return 0;

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

    window_t* w1 = window_new(&context, 10, 10, 300, 200);
    window_t* w2 = window_new(&context, 100, 150, 400, 400);
    window_t* w3 = window_new(&context, 200, 100, 200, 600);

    window_paint(w1);
    window_paint(w2);
    window_paint(w3);




    return 0;
} 

