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
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>
#include <libtsm.h>
#include <poll.h>
#include <sched.h>

#include <aplus/fb.h>
#include <aplus/events.h>

#if defined(CONFIG_ATERM_BUILTIN_FONT)
#include "lib/builtin_font.h"
#endif



static struct {

    void (*plot)(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b);

    int pipefd[2];

    int kbd;
    int mouse;

    struct {
        uint16_t x;
        uint16_t y;
    } cursor;

    struct tsm_screen* con;
    struct tsm_vte* vte;

    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;

} context;



static void show_usage(int argc, char** argv) {
    printf(
        "Use: aplus-terminal [options]... [STRING]...\n"
        "Print STRING(s) to standard output.\n\n"
        "   -c, --command               run command on shell\n"
        "   -w, --working-dir           set current working directory\n"
        "   -n                          no newline at end of output\n"
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





static void fb_plot_8(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {

    ((uint8_t*) (context.fix.smem_start))[(y * context.var.xres) + x] = (r >> 5) << 5 | (g >> 5) << 2 | (b >> 6);

}

static void fb_plot_16(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {

    ((uint16_t*) (context.fix.smem_start))[(y * context.var.xres) + x] = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3);

}

static void fb_plot_24(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {

    ((uint8_t*) (context.fix.smem_start))[(y * context.var.xres * 3) + (x * 3) + 0] = r;
    ((uint8_t*) (context.fix.smem_start))[(y * context.var.xres * 3) + (x * 3) + 1] = g;
    ((uint8_t*) (context.fix.smem_start))[(y * context.var.xres * 3) + (x * 3) + 2] = b;

}

static void fb_plot_32(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {

    ((uint32_t*) (context.fix.smem_start))[(y * context.var.xres) + x] = 0xFF000000 | (r << 16) | (g << 8) | b;

}


static int fb_draw_cb(struct tsm_screen* con, uint32_t id, const uint32_t* ch, size_t len, uint32_t width, uint32_t posx, uint32_t posy, const struct tsm_screen_attr* attr, tsm_age_t age, void* data) {
    
    assert(con);
    assert(attr);
    assert(len < 2);


#if defined(CONFIG_ATERM_BUILTIN_FONT)

    posx *= ATERM_FONT_WIDTH;
    posy *= ATERM_FONT_HEIGHT;

    if(posx + ATERM_FONT_WIDTH >= context.var.xres || posy + ATERM_FONT_HEIGHT >= context.var.yres)
        return 0;

#endif


    uint8_t fr;
    uint8_t fg;
    uint8_t fb;
    uint8_t br;
    uint8_t bg;
    uint8_t bb;

    if(attr->inverse) {
        fr = attr->br;
        fg = attr->bg;
        fb = attr->bb;
        br = attr->fr;
        bg = attr->fg;
        bb = attr->fb;
    } else {
        fr = attr->fr;
        fg = attr->fg;
        fb = attr->fb;
        br = attr->br;
        bg = attr->bg;
        bb = attr->bb;
    }


    uint32_t gidx = len ? *ch : 0;


#if defined(CONFIG_ATERM_BUILTIN_FONT)

    if(gidx > 255)
        gidx = 0;
    
    const uint8_t* glyph = &builtin_fontdata[gidx * ATERM_FONT_PITCH];

    for(size_t i = 0; i < ATERM_FONT_HEIGHT; i++) {
        for(size_t j = 0; j < ATERM_FONT_WIDTH; j++) {

            if(glyph[i] & (1 << (ATERM_FONT_WIDTH - j))) {
                context.plot(posx + j, posy + i, fr, fg, fb);
            } else {
                context.plot(posx + j, posy + i, br, bg, bb);
            }
            
        }
    }

#else
#error "Truetype not implemented yet"
#endif



    uint16_t x = context.cursor.x;
    uint16_t y = context.cursor.y;
    uint16_t w = context.var.xres_virtual > context.cursor.x + 16 ? context.cursor.x + 16 : context.var.xres_virtual;
    uint16_t h = context.var.yres_virtual > context.cursor.y + 16 ? context.cursor.y + 16 : context.var.yres_virtual;

    for(; x < w; x++) {
        for(; y < h; y++) {
            context.plot(x, y, fr, fg, fb);
        }
    }

    return 0;

}



static void term_write_cb(struct tsm_vte* vte, const char* bytes, size_t len, void* data) {
    fprintf(stderr, "aplus-terminal: term_write_cb(): vte(%p), bytes('%s' - %p - %d), size(%ld)\n", vte, bytes, (void*) bytes, bytes ? bytes[0] : -1, len);
}




int main(int argc, char** argv) {
    
    
    static struct option long_options[] = {
        { "command", required_argument, NULL, 'c'},
        { "working-dir", required_argument, NULL, 'w'},
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    

    
    char* cmd = NULL;
    char* pwd = NULL;
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "c:w:vh", long_options, &idx)) != -1) {
        switch(c) {
            case 'c':
                cmd = strdup(optarg);
                break;
            case 'w':
                pwd = strdup(optarg);
                break;
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



    //* 1. Framebuffer initialization

    int fd;
    
    if((fd = open("/dev/fb0", O_RDWR)) < 0) {
        fprintf(stderr, "aplus-terminal: open() failed: cannot open /dev/fb0: %s\n", strerror(errno));
        exit(1);
    }

    if(ioctl(fd, FBIOGET_VSCREENINFO, &context.var) < 0) {
        fprintf(stderr, "aplus-terminal: ioctl() failed: %s\n", strerror(errno));
        exit(1);
    }

    if(ioctl(fd, FBIOGET_FSCREENINFO, &context.fix) < 0) {
        fprintf(stderr, "aplus-terminal: ioctl() failed: %s\n", strerror(errno));
        exit(1);
    }

    if(!context.fix.smem_start || !context.var.xres_virtual || !context.var.yres_virtual) {
        fprintf(stderr, "aplus-terminal: wrong framebuffer configuration\n");
        exit(1);
    }

    if(close(fd) < 0) {
        fprintf(stderr, "aplus-terminal: close() failed: %s\n", strerror(errno));
        exit(1);
    }


    switch(context.var.bits_per_pixel) {

        case 8:
            context.plot = fb_plot_8;
            break;
        case 16:
            context.plot = fb_plot_16;
            break;
        case 24:
            context.plot = fb_plot_24;
            break;
        case 32:
            context.plot = fb_plot_32;
            break;

        default:
            fprintf(stderr, "aplus-terminal: unsupported framebuffer depth\n");
            exit(1);

    }


    //* 2. TSM initialization

    if(tsm_screen_new(&context.con, NULL, NULL) < 0) {
        fprintf(stderr, "aplus-terminal: tsm_screen_new() failed\n");
        exit(1);
    }

    assert(context.con);


    if(tsm_vte_new(&context.vte, context.con, &term_write_cb, NULL, NULL, NULL) < 0) {
        fprintf(stderr, "aplus-terminal: tsm_vte_new() failed\n");
        exit(1);
    }

    assert(context.vte);


    if(tsm_screen_resize(context.con, context.var.xres_virtual / 8, context.var.yres_virtual / 16) < 0) {
        fprintf(stderr, "aplus-terminal: tsm_screen_resize() failed\n");
        exit(1);
    }


    // Set Line Mode LNM
    tsm_vte_input(context.vte, "\e[20h", 5);

    // Draw first frame
    tsm_screen_draw(context.con, fb_draw_cb, NULL);



    //* 3. I/O initialization

    if(pipe2(context.pipefd, O_NONBLOCK) < 0) {
        fprintf(stderr, "aplus-terminal: pipe() failed\n");
        exit(1);
    }

    assert(context.pipefd[0] >= 0);
    assert(context.pipefd[1] >= 0);


    //* 4. Input initialization

    context.kbd = open("/dev/kbd", O_RDONLY);

    if(context.kbd < 0) {
        fprintf(stderr, "aplus-terminal: open() failed: cannot open /dev/kbd: %s\n", strerror(errno));
        exit(1);
    }


    context.mouse = open("/dev/mouse", O_RDONLY);

    if(context.mouse < 0) {
        fprintf(stderr, "aplus-terminal: open() failed: cannot open /dev/mouse: %s\n", strerror(errno));
        exit(1);
    }



    
    //* 4. Child initialization

    pid_t pid = fork();

    if(pid < 0) {

        fprintf(stderr, "aplus-terminal: fork() failed\n");
        exit(1);
    
    } else if(pid == 0) {

        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        setenv("TERMINFO", "/usr/share/terminfo/x/xterm-256color", 1);
        setenv("COLORFGBG", "7;0", 1);


        if(dup2(context.pipefd[1], STDOUT_FILENO) < 0) {
            fprintf(stderr, "aplus-terminal: dup2() failed\n");
            exit(1);
        }

        if(dup2(context.pipefd[1], STDERR_FILENO) < 0) {
            fprintf(stderr, "aplus-terminal: dup2() failed\n");
            exit(1);
        }


        if(pwd) {
            if(chdir(pwd) < 0) {
                fprintf(stderr, "aplus-terminal: chdir() failed: %s\n", strerror(errno));
                exit(1);
            }
        }


        if(cmd) {
            execl("/bin/sh", "/bin/sh", "-c", cmd, NULL);
        } else {
            execl("/bin/sh", "/bin/sh", NULL);
        }

        fprintf(stderr, "aplus-terminal: execl() failed: %s\n", strerror(errno));
        exit(1);


    } else {


        do {

            struct pollfd pfd[3] = {
                { .fd = context.kbd,       .events = POLLIN },
                { .fd = context.mouse,     .events = POLLIN },
                { .fd = context.pipefd[0], .events = POLLIN }
            };

            if(poll(pfd, 3, -1) < 0) {
                break;
            }


            for(size_t i = 0; i < (sizeof(pfd) / sizeof(pfd[0])); i++) {
                
                if(pfd[i].revents & POLLERR || pfd[i].revents & POLLHUP || pfd[i].revents & POLLNVAL) {
                    break;
                }

                if(pfd[i].revents & POLLIN) {

                    if(pfd[i].fd == context.pipefd[0]) {

                        char buf[BUFSIZ];
                        ssize_t size;

                        do {

                            while((size = read(context.pipefd[0], buf, sizeof(buf))) > 0) {

                                tsm_vte_input(context.vte, buf, size);
                                tsm_screen_draw(context.con, fb_draw_cb, NULL);

                            }


                        } while(errno == EINTR);

                    }
                    
                    if(pfd[i].fd == context.kbd) {

                        event_t ev;

                        do {

                            if(read(context.kbd, &ev, sizeof(ev)) > 0) {

                                if(ev.ev_type == EV_KEY) {

                                    if(tsm_vte_handle_keyboard(context.vte, ev.ev_key.vkey, 0, 0, 0)) {
                                        tsm_screen_sb_reset(context.con);
                                    }

                                    tsm_screen_draw(context.con, fb_draw_cb, NULL);

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

                                    tsm_screen_draw(context.con, fb_draw_cb, NULL);

                                }

                            }

                        } while(errno == EINTR);

                    }

                }

            }

        } while(true);

        
        if(close(context.pipefd[0]) < 0) {
            fprintf(stderr, "aplus-terminal: close() failed: %s\n", strerror(errno));
            exit(1);
        }

        if(close(context.pipefd[1]) < 0) {
            fprintf(stderr, "aplus-terminal: close() failed: %s\n", strerror(errno));
            exit(1);
        }

    }


    return 0;
    
}