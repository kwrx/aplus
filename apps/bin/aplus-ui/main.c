/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2022 Antonino Natale
 * 
 * 
 * This file is part of aplus.
 * 
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>
#include <poll.h>
#include <sched.h>

#include <aplus/fb.h>
#include <aplus/input.h>
#include <aplus/events.h>
#include <aplus/utils/hashmap.h>

#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>
#include <cairo/cairo-features.h>



#define UI_WINDOW_FLAGS_HIDE                (1 << 0)
#define UI_WINDOW_FLAGS_MOVABLE             (1 << 1)
#define UI_WINDOW_FLAGS_RESIZABLE           (1 << 2)
#define UI_WINDOW_FLAGS_CLOSEABLE           (1 << 3)
#define UI_WINDOW_FLAGS_MINIMIZABLE         (1 << 4)
#define UI_WINDOW_FLAGS_MAXIMIZABLE         (1 << 5)
#define UI_WINDOW_FLAGS_NO_BORDER           (1 << 6)
#define UI_WINDOW_FLAGS_NO_TITLE            (1 << 7)




typedef struct window {

    uint16_t w;
    uint16_t h;
    uint16_t x;
    uint16_t y;

    int flags;

    void* framebuffer;
    char* title;

} window_t;



static struct {

    int kbd;
    int mouse;

    cairo_t* cr;
    cairo_surface_t* surface;
    
    struct {
        const void* surface;
        int width;
        int height;
    } bake;

    struct {
        uint16_t x;
        uint16_t y;
    } cursor;

    HASHMAP(char*, window_t*) windows;

    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;

} context;




static void show_usage(int argc, char** argv) {
    printf(
        "Use: aplus-ui [options]... [STRING]...\n"
        "User Interface Server.\n\n"
        "Options:\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aplus coreutils) 0.1\n"
        "Copyright (c) %s Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __DATE__ + 7, __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}



int main(int argc, char** argv) {
    
    
    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "vh", long_options, &idx)) != -1) {
        switch(c) {
            case 'v':
                show_version(argc, argv);
                break;
            case 'h':
            case '?':
                show_usage(argc, argv);
                break;
            default:
                abort();
        }
    }




    int fd;
    
    if((fd = open("/dev/fb0", O_RDWR)) < 0) {
        fprintf(stderr, "aplus-ui: open() failed: cannot open /dev/fb0: %s\n", strerror(errno));
        exit(1);
    }

    if(ioctl(fd, FBIOGET_VSCREENINFO, &context.var) < 0) {
        fprintf(stderr, "aplus-ui: ioctl() failed: %s\n", strerror(errno));
        exit(1);
    }

    if(ioctl(fd, FBIOGET_FSCREENINFO, &context.fix) < 0) {
        fprintf(stderr, "aplus-ui: ioctl() failed: %s\n", strerror(errno));
        exit(1);
    }

    if(!context.fix.smem_start || !context.var.xres_virtual || !context.var.yres_virtual) {
        fprintf(stderr, "aplus-ui: wrong framebuffer configuration\n");
        exit(1);
    }

    if(close(fd) < 0) {
        fprintf(stderr, "aplus-ui: close() failed: %s\n", strerror(errno));
        exit(1);
    }


    switch(context.var.bits_per_pixel) {

        case 8:
            context.surface = cairo_image_surface_create(CAIRO_FORMAT_A8, context.var.xres_virtual, context.var.yres_virtual);
            break;
        case 16:
            context.surface = cairo_image_surface_create(CAIRO_FORMAT_RGB16_565, context.var.xres_virtual, context.var.yres_virtual);
            break;
        case 24:
            context.surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, context.var.xres_virtual, context.var.yres_virtual);
            break;
        case 32:
            context.surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, context.var.xres_virtual, context.var.yres_virtual);
            break;

        default:
            fprintf(stderr, "aplus-ui: unsupported framebuffer depth\n");
            exit(1);

    }


    if(!context.surface) {
        fprintf(stderr, "aplus-ui: cairo_create_image_surface() failed\n");
        exit(1);
    }


    context.cr = cairo_create(context.surface);

    if(!context.cr) {
        fprintf(stderr, "aplus-ui: cairo_create() failed\n");
        exit(1);
    }




    context.kbd = open("/dev/kbd", O_RDONLY);

    if(context.kbd < 0) {
        fprintf(stderr, "aplus-ui: open() failed: cannot open /dev/kbd: %s\n", strerror(errno));
        exit(1);
    }

    context.mouse = open("/dev/mouse", O_RDONLY);

    if(context.mouse < 0) {
        fprintf(stderr, "aplus-ui: open() failed: cannot open /dev/mouse: %s\n", strerror(errno));
        exit(1);
    }


    
    do {

        struct pollfd pfd[2] = {
            { .fd = context.kbd,       .events = POLLIN },
            { .fd = context.mouse,     .events = POLLIN },
        };

        if(poll(pfd, 2, -1) < 0) {
            break;
        }


        for(size_t i = 0; i < (sizeof(pfd) / sizeof(pfd[0])); i++) {
            
            if(pfd[i].revents & POLLERR || pfd[i].revents & POLLHUP || pfd[i].revents & POLLNVAL) {
                break;
            }

            if(pfd[i].revents & POLLIN) {
                
                if(pfd[i].fd == context.kbd) {

                    event_t ev;

                    do {

                        if(read(context.kbd, &ev, sizeof(ev)) > 0) {

                            if(ev.ev_type == EV_KEY) {

                                // TODO ...

                            }

                        }

                    } while(errno == EINTR);

                }

                if(pfd[i].fd == context.mouse) {

                    event_t ev;

                    do {

                        while(read(context.mouse, &ev, sizeof(ev)) == sizeof(ev)) {

                            if(ev.ev_type == EV_REL) {

                                context.cursor.x += ev.ev_rel.x * 3;
                                context.cursor.y -= ev.ev_rel.y * 3;

                                if(context.cursor.x > context.var.xres_virtual) {
                                    context.cursor.x = context.var.xres_virtual;
                                }

                                if(context.cursor.y > context.var.yres_virtual) {
                                    context.cursor.y = context.var.yres_virtual;
                                }

                            }

                            if(ev.ev_type == EV_KEY) {

                                switch(ev.ev_key.vkey) {

                                    case BTN_LEFT:
                                        break;

                                    case BTN_RIGHT:
                                        break;

                                    case BTN_MIDDLE:
                                        break;

                                    default:
                                        break;

                                }

                            }

                        }

                    } while(errno == EINTR);

                }

            }

        }


    } while (1);    


    return 0;
    
}