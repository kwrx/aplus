/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <aplus.h>
#include <aplus/base.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/kd.h>
#include <aplus/fb.h>
#include <aplus/sysconfig.h>
#include <libc.h>

#include <sys/ioctl.h>
#include <termios.h>

#include <wchar.h>
#include <aplus/utils/unicode.h>

#include "console-font.h"

MODULE_NAME("video/console");
MODULE_DEPS("video/fb,pc/video/bga"); /* FIXME */
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



#define CONSOLE_WIDTH               80
#define CONSOLE_HEIGHT              25



struct cc {
    int p;
    int escape;
    int escape_offset;
    int escape_saved_cursor;
    int style;
    int colors;
    char escape_buffer[32];

    void* frame;
    int width;
    int height;
    int vmode;

    __fastcall
    void (*output) (struct cc* cc, int pos, uint8_t style, int32_t ch);

    __fastcall
    void (*scroll) (struct cc* cc, int pos);
} __packed;



#include "console-output-32.h"
#include "console-output-24.h"
#include "console-output-16.h"
#include "console-output-8.h"



__fastcall
static inline uint32_t __C(struct cc* cc, uint32_t p) {
    if(unlikely(p > cc->width * cc->height))
        p = cc->width * cc->height;
 
    return p;
}

static void plot_value(struct cc* cc, int32_t uvalue) {
    if(unlikely(!cc->output))
        return;

    char value = uvalue & 0xFF;

    if(cc->escape) {
        if(likely(value == '[')) {
            cc->escape++;
            return;
        }

        if(cc->escape != 2) {
            cc->escape = 0;
            return;
        }


        if(isdigit(value) || value == ';')
            cc->escape_buffer[cc->escape_offset++] = value;
        else {
            int x = 0, y = 0;
            char buf[32];
            switch(value) {
                case '@':
                    y = atoi(cc->escape_buffer);
                    for(x = 0; x < y; x++)
                        cc->output(cc, __C(cc, cc->p + x), cc->style, u' ');
                    break;
                case 'A':
                    y = atoi(cc->escape_buffer);
                    for(x = 0; x < y; x++)
                        cc->p -= cc->width;
                    break;
                case 'B':
                case 'e':
                    y = atoi(cc->escape_buffer);
                    for(x = 0; x < y; x++)
                        cc->p += cc->width;
                    break;
                case 'C':
                case 'a':
                    cc->p += atoi(cc->escape_buffer);
                    break;
                case 'D':
                    cc->p -= atoi(cc->escape_buffer);
                    break;
                case 'E':
                    y = atoi(cc->escape_buffer);
                    for(x = 0; x < y; x++)
                        cc->p += cc->width - (cc->p % cc->width);
                    break;
                case 'F':
                    y = atoi(cc->escape_buffer);
                    for(x = 0; x < y; x++) 
                        cc->p -= cc->width + (cc->p % cc->width);
                    break;
                case 'G':
                    cc->p -= (cc->p % cc->width);
                    cc->p += atoi(cc->escape_buffer);
                    break;
                case 'H':
                case 'f':
                    sscanf(cc->escape_buffer, "%d;%d", &y, &x);
                    cc->p = ((y ? y - 1 : 0)) * cc->width + (x ? x - 1 : 0);
                    break;
                case 'J':
                    y = atoi(cc->escape_buffer);
                    switch(y) {
                        case 1:
                            x = cc->p;
                            while(x < cc->width * cc->height)
                                cc->output(cc, x++, cc->style, u' ');
                            break;
                        default:
                            for(x = 0; x < cc->width * cc->height; x++)
                                cc->output(cc, x, cc->style, u' ');
                            break;
                    }
                    break;
                case 'K':
                    y = atoi(cc->escape_buffer);
                    switch(y) {
                        case 0:
                            x = cc->p + (cc->width - (cc->p % cc->width));
                            while(x >= cc->p)
                                cc->output(cc, __C(cc, x--), cc->style, u' ');
                            break;
                        case 1:
                            x = cc->p - (cc->p % cc->width);
                            while(x < cc->p)
                                cc->output(cc, __C(cc, x++), cc->style, u' ');
                            break;
                        case 2:
                            for(x = 0; x < cc->width; x++)
                                cc->output(cc, __C(cc, cc->p - (cc->p % cc->width) + x), cc->style, u' ');
                            break;
                    }
                    break;
                case 'L':
                    y = atoi(cc->escape_buffer) * cc->width;
                    y -= (cc->p % cc->width);

                    x = cc->p - (cc->p % cc->width);
                    while(x < y)
                        cc->output(cc, __C(cc, x++), cc->style, u' ');
                    
                    break;
                case 'M':
                    y = atoi(cc->escape_buffer) * cc->width;
                    y += (cc->p % cc->width);

                    x = cc->p + (cc->width - (cc->p % cc->width));
                    while(x > y)
                        cc->output(cc, __C(cc, x--), cc->style, u' ');
                    
                    break;
                case 'P':
                    y = atoi(cc->escape_buffer);
                    while(y--)
                        cc->output(cc, __C(cc, cc->p--), cc->style, u' ');
                    break;
                case 'X':
                    y = atoi(cc->escape_buffer);
                    while(y)
                        cc->output(cc, __C(cc, cc->p - y--), cc->style, u' ');
                    break;
                case 'd':
                    y = atoi(cc->escape_buffer);
                    cc->p = (cc->width * y) + (cc->p % cc->width);
                    break;
                case 's':
                    cc->escape_saved_cursor = cc->p;
                    break;
                case 'u':
                    cc->p = cc->escape_saved_cursor;
                    break;
                case 'm':
                    for(char* p = strtok(cc->escape_buffer, ";"); p; p = strtok(NULL, ";")) {
                        char colors[2][8] = {
                            { 0x0, 0x4, 0x2, 0x6, 0x1, 0x5, 0x3, 0x7 },
                            { 0x8, 0xC, 0xA, 0xE, 0x9, 0xD, 0xB, 0xF }
                        };

                        x = atoi(p);
                        switch(x) {
                            case 0:
                                cc->style = 0x07;
                                cc->colors = 1;
                                break;
                            case 2:
                                cc->colors = 0;
                                break;
                            case 22:
                                cc->colors = 1;
                                break; 
                            case 30 ... 37:
                                cc->style = (cc->style & 0xF0) | (colors[cc->colors][x - 30] & 0x0F);
                                break;
                            case 40 ... 47:
                                cc->style = (cc->style & 0x0F) | (colors[cc->colors][x - 40] << 4);
                                break;
                            case 38:
                            case 39:
                                cc->style = (cc->style & 0xF0) | 0x07;
                                break;
                            case 49:
                                cc->style = (cc->style & 0x0F);
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                default:
                    break;
            }

            cc->escape = 0;
            cc->escape_offset = 0;

            memset(cc->escape_buffer, 0, 32);
        }

    } else {
        if(uvalue > 0x7F)
            cc->output(cc, __C(cc, cc->p++), cc->style, uvalue);
        else {
            switch(value) {
                case '\n':
                    cc->p += cc->width - (cc->p % cc->width);
                    break;
                case '\v':
                    cc->p += cc->width;
                    break;
                case '\r':
                    cc->p -= (cc->p % cc->width);
                    break;
                case '\t':
                    cc->p += 4 - ((cc->p % cc->width) % 4);
                    break;
                case '\b':
                    cc->output(cc, __C(cc, --cc->p), cc->style, u' ');
                    break;
                case '\e':
                    cc->escape = 1;
                    break;
                default:
                    cc->output(cc, __C(cc, cc->p++), cc->style, uvalue);
                    break;
            }
        }
    }



    if(cc->p > (cc->width * cc->height)) {
        cc->scroll(cc, 1);
        cc->p -= cc->width;

        int x;
        for(x = 0; x < cc->width; x++)
            cc->output(cc, __C(cc, cc->p + x), cc->style, u' ');

    }
}




static int console_init_graphics(struct cc* cc) {
    char* dev = (char*) sysconfig("screen.device", "/dev/fb0");    
    
    
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    struct winsize ws;    

    int fb = sys_open(dev, O_RDONLY, 0);
    if(fb < 0) {
        kprintf(ERROR "console: open framebuffer");
        return -1;
    }


    if(cc->vmode == KD_TEXT) {
        memset(&var, 0, sizeof(var));
        var.xres =
        var.xres_virtual = (int) sysconfig("screen.width", 800);
        var.yres =
        var.yres_virtual = (int) sysconfig("screen.height", 600);
        var.bits_per_pixel = (int) sysconfig("screen.bpp", 32);
        var.activate = FB_ACTIVATE_NOW;

        sys_ioctl(fb, FBIOPUT_VSCREENINFO, &var);
    }


    sys_ioctl(fb, FBIOGET_VSCREENINFO, &var);
    sys_ioctl(fb, FBIOGET_FSCREENINFO, &fix);
    sys_close(fb);


    sys_ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
    ws.ws_row = var.yres_virtual / 16;
    ws.ws_col = var.xres_virtual / 8;
    ws.ws_xpixel = var.xres_virtual;
    ws.ws_ypixel = var.yres_virtual;
    sys_ioctl(STDIN_FILENO, TIOCSWINSZ, &ws);


    cc->frame = (void*) fix.smem_start;
    cc->width = var.xres_virtual / 8;
    cc->height = var.yres_virtual / 16;
    cc->p = 0;
    

    switch(var.bits_per_pixel) {
        case 8:
            cc->output = console_output_8;
            cc->scroll = console_scroll_8;
            break;
        case 16:
            cc->output = console_output_16;
            cc->scroll = console_scroll_16;
            break;
        case 24:
            cc->output = console_output_24;
            cc->scroll = console_scroll_24;
            break;
        case 32:
            cc->output = console_output_32;
            cc->scroll = console_scroll_32;
            break;
    }

    kprintf(INFO "console: set graphics mode %dx%dx%d\n", var.xres, var.yres, var.bits_per_pixel);
    return 0;
}

static int console_write(struct inode* inode, void* buf, off_t pos, size_t size) {
    if(unlikely(!inode || !buf || !inode->userdata)) {
        errno = EINVAL;
        return -1;
    }

    if(unlikely(!size))
        return 0;


    struct cc* cc = inode->userdata;

    const uint8_t* ch = (const uint8_t*) buf;
    int i;
    for(i = 0; i < size;) {     
        i += utf8_bytes(*ch);

        int32_t wch;
        if((wch = utf8_to_ucs2(ch, &ch)) < 0)
            break;

        plot_value(cc, wch);
    }

    return size;
}


static int console_ioctl(struct inode* inode, int req, void* arg) {
    if(unlikely(!inode || !inode->userdata)) {
        errno = EINVAL;
        return -1;
    }


    struct cc* cc = inode->userdata;
    switch(req) {
        case KDGETLED:
        case KDSETLED:
        case KDGKBLED:
        case KDSKBLED:
            errno = ENOSYS;
            return -1;
        case KDGKBTYPE:
            return 0x02; /* KB_101 */
        case KDADDIO:
        case KDDELIO:
        case KDENABIO:
            errno = ENOSYS;
            return -1;
        case KDSETMODE:
            switch((int) arg) {
                case KD_TEXT:
                    errno = ENOSYS;
                    return -1;
                case KD_GRAPHICS:
                    if(console_init_graphics(cc) != 0)
                        return -1;
                    
                    cc->vmode = KD_GRAPHICS;
                    return 0;
                default:
                    errno = EINVAL;
                    return -1;
            }
        case KDGETMODE:
            return cc->vmode;
        case KDMKTONE:
        case KIOCSOUND:
            errno = ENOSYS;
            return -1;
    }   
    
    
    errno = EINVAL;
    return -1;
}

int init(void) {
    struct cc* cc = (void*) kmalloc(sizeof(struct cc), GFP_KERNEL);
    if(unlikely(!cc)) {
        errno = ENOMEM;
        return -1;
    }

    memset(cc, 0, sizeof(struct cc));
    cc->style = 0x0F;
    cc->colors = 1;
    cc->vmode = KD_TEXT;
    cc->width = CONSOLE_WIDTH;
    cc->height = CONSOLE_HEIGHT;
    cc->output = NULL;
    cc->scroll = NULL;

    console_init_graphics(cc);


    int i;
    for(i = 0; i < cc->width * cc->height; i++)
        cc->output(cc, i, cc->style, u' ');


    inode_t* ino = vfs_mkdev("console", -1, S_IFCHR | 0222);
    ino->ioctl = console_ioctl;
    ino->write = console_write;
    ino->userdata = cc;

    return 0;
}


int dnit(void) {
    return 0;
}
