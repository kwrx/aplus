#include <aplus.h>
#include <aplus/base.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/mm.h>
#include <aplus/sysconfig.h>
#include <aplus/fb.h>
#include <libc.h>

#include "console-font.h"
#include "console-priv.h"

#include "console-output-8.h"
#include "console-output-16.h"
#include "console-output-24.h"
#include "console-output-32.h"


int fb_init(context_t* cx) {
    char* dev = (char*) sysconfig (
        "screen.device", 
        "/dev/fb0"
    );    
    
    

    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    struct winsize ws;    

    int fb = sys_open(dev, O_RDONLY, 0);
    if(fb < 0) {
        kprintf(ERROR "console: open framebuffer");
        return -1;
    }


    memset(&var, 0, sizeof(var));
    var.xres =
    var.xres_virtual = (int) sysconfig("screen.width", 640);
    var.yres =
    var.yres_virtual = (int) sysconfig("screen.height", 400);
    var.bits_per_pixel = (int) sysconfig("screen.bpp", 32);
    var.activate = FB_ACTIVATE_NOW;


    sys_ioctl(fb, FBIOPUT_VSCREENINFO, &var);
    sys_ioctl(fb, FBIOGET_VSCREENINFO, &var);
    sys_ioctl(fb, FBIOGET_FSCREENINFO, &fix);
    sys_close(fb);


    sys_ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
    ws.ws_row = var.yres_virtual / ROW;
    ws.ws_col = var.xres_virtual / COL;
    ws.ws_xpixel = var.xres_virtual;
    ws.ws_ypixel = var.yres_virtual;
    sys_ioctl(STDIN_FILENO, TIOCSWINSZ, &ws);


    cx->screen.framebuffer = (void*) fix.smem_start;
    cx->screen.width = var.xres_virtual;
    cx->screen.height = var.yres_virtual;
    cx->screen.bpp = var.bits_per_pixel / 8;
    cx->screen.stride = fix.line_length;

    cx->console.rows = cx->screen.height / ROW;
    cx->console.cols = cx->screen.width / COL;


    cx->screen.backbuffer = kcalloc(1, cx->screen.stride * cx->screen.height, GFP_KERNEL);
    if(!cx->screen.backbuffer) {
        kprintf(ERROR "console: no memory left\n");
        return -1;
    }



    switch(var.bits_per_pixel) {
        case 8:
            cx->fb.clear = console_output_clear_8;
            cx->fb.move = console_output_move_8;
            cx->fb.putc = console_output_putc_8;
            break;
        case 16:
            cx->fb.clear = console_output_clear_16;
            cx->fb.move = console_output_move_16;
            cx->fb.putc = console_output_putc_16;
            break;
        case 24:
            cx->fb.clear = console_output_clear_24;
            cx->fb.move = console_output_move_24;
            cx->fb.putc = console_output_putc_24;
            break;
        case 32:
            cx->fb.clear = console_output_clear_32;
            cx->fb.move = console_output_move_32;
            cx->fb.putc = console_output_putc_32;
            break;
    }


    kprintf(INFO "console: set graphics mode %dx%dx%d\n", var.xres, var.yres, var.bits_per_pixel);
    return 0;
}