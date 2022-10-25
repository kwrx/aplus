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


#define _GNU_SOURCE
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
#include <aplus/input.h>
#include <aplus/events.h>

#if defined(CONFIG_ATERM_BUILTIN_FONT)
#include "lib/builtin_font.h"
#endif



static struct {

    void (*plot)(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b);

    int ipipefd[2];
    int opipefd[2];

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

    struct {
        uint8_t ctrl;
        uint8_t alt;
        uint8_t shift;
        uint8_t meta;
    } modifiers;

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

    if(gidx > 255) {
        gidx = 0;
    }
    
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



    // // uint16_t x = context.cursor.x;
    // // uint16_t y = context.cursor.y;
    // // uint16_t w = context.var.xres_virtual > context.cursor.x + 16 ? context.cursor.x + 16 : context.var.xres_virtual;
    // // uint16_t h = context.var.yres_virtual > context.cursor.y + 16 ? context.cursor.y + 16 : context.var.yres_virtual;

    // // for(; x < w; x++) {
    // //     for(; y < h; y++) {
    // //         context.plot(x, y, fr, fg, fb);
    // //     }
    // // }

    return 0;

}



static void tsm_handle_key(int out, vkey_t keysym, uint8_t down) {

    fprintf(stderr, "key: %d %d\n", keysym, down);

    switch(keysym) {

        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
            context.modifiers.shift = down;
            break;

        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:
            context.modifiers.ctrl = down;
            break;

        case KEY_LEFTALT:
        case KEY_RIGHTALT:
            context.modifiers.alt = down;
            break;

        case KEY_LEFTMETA:
        case KEY_RIGHTMETA:
            context.modifiers.meta = down;
            break;

    }


    if(down) {

        switch(keysym) {

            case KEY_ESC:
                tsm_vte_input(context.vte, "\x1B", 1);
                break;

            case KEY_TAB:
                tsm_vte_input(context.vte, "\t", 1);
                break;

            case KEY_BACKSPACE:
                tsm_vte_input(context.vte, "\x7F", 1);
                break;

            case KEY_ENTER:
                tsm_vte_input(context.vte, "\n", 1);
                break;

            case KEY_UP:
                tsm_vte_input(context.vte, "\x1B[A", 3);
                break;

            case KEY_DOWN:
                tsm_vte_input(context.vte, "\x1B[B", 3);
                break;

            case KEY_RIGHT:
                tsm_vte_input(context.vte, "\x1B[C", 3);
                break;

            case KEY_LEFT:
                tsm_vte_input(context.vte, "\x1B[D", 3);
                break;

            case KEY_HOME:
                tsm_vte_input(context.vte, "\x1B[H", 3);
                break;
        
            case KEY_END:
                tsm_vte_input(context.vte, "\x1B[F", 3);
                break;

            case KEY_PAGEUP:
                tsm_vte_input(context.vte, "\x1B[5~", 4);
                break;

            case KEY_PAGEDOWN:
                tsm_vte_input(context.vte, "\x1B[6~", 4);
                break;

            case KEY_INSERT:
                tsm_vte_input(context.vte, "\x1B[2~", 4);
                break;

            case KEY_DELETE:
                tsm_vte_input(context.vte, "\x1B[3~", 4);
                break;

            case KEY_F1:
                tsm_vte_input(context.vte, "\x1BOP", 3);
                break;

            case KEY_F2:
                tsm_vte_input(context.vte, "\x1BOQ", 3);
                break;

            case KEY_F3:
                tsm_vte_input(context.vte, "\x1BOR", 3);
                break;

            case KEY_F4:
                tsm_vte_input(context.vte, "\x1BOS", 3);
                break;

            case KEY_F5:
                tsm_vte_input(context.vte, "\x1B[15~", 5);
                break;

            case KEY_F6:
                tsm_vte_input(context.vte, "\x1B[17~", 5);
                break;

            case KEY_F7:
                tsm_vte_input(context.vte, "\x1B[18~", 5);
                break;

            case KEY_F8:
                tsm_vte_input(context.vte, "\x1B[19~", 5);
                break;

            case KEY_F9:
                tsm_vte_input(context.vte, "\x1B[20~", 5);
                break;

            case KEY_F10:
                tsm_vte_input(context.vte, "\x1B[21~", 5);
                break;

            case KEY_F11:
                tsm_vte_input(context.vte, "\x1B[23~", 5);
                break;

            case KEY_F12:
                tsm_vte_input(context.vte, "\x1B[24~", 5);
                break;


            case KEY_0:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, ")", 1);
                } else {
                    tsm_vte_input(context.vte, "0", 1);
                }
                break;

            case KEY_1:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "!", 1);
                } else {
                    tsm_vte_input(context.vte, "1", 1);
                }
                break;

            case KEY_2:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "@", 1);
                } else {
                    tsm_vte_input(context.vte, "2", 1);
                }
                break;
            
            case KEY_3:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "#", 1);
                } else {
                    tsm_vte_input(context.vte, "3", 1);
                }
                break;
            
            case KEY_4:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "$", 1);
                } else {
                    tsm_vte_input(context.vte, "4", 1);
                }
                break;

            case KEY_5:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "%", 1);
                } else {
                    tsm_vte_input(context.vte, "5", 1);
                }
                break;
            
            case KEY_6:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "^", 1);
                } else {
                    tsm_vte_input(context.vte, "6", 1);
                }
                break;

            case KEY_7:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "&", 1);
                } else {
                    tsm_vte_input(context.vte, "7", 1);
                }
                break;

            case KEY_8:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "*", 1);
                } else {
                    tsm_vte_input(context.vte, "8", 1);
                }
                break;

            case KEY_9:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "(", 1);
                } else {
                    tsm_vte_input(context.vte, "9", 1);
                }
                break;

            case KEY_Q ... KEY_P:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, &"QWERTYUIOP"[keysym - KEY_Q], 1);
                } else {
                    tsm_vte_input(context.vte, &"qwertyuiop"[keysym - KEY_Q], 1);
                }
                break;

            case KEY_A ... KEY_L:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, &"ASDFGHJKL"[keysym - KEY_A], 1);
                } else {
                    tsm_vte_input(context.vte, &"asdfghjkl"[keysym - KEY_A], 1);
                }
                break;

            case KEY_Z ... KEY_M:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, &"ZXCVBNM"[keysym - KEY_Z], 1);
                } else {
                    tsm_vte_input(context.vte, &"zxcvbnm"[keysym - KEY_Z], 1);
                }
                break;

            case KEY_COMMA:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "<", 1);
                } else {
                    tsm_vte_input(context.vte, ",", 1);
                }
                break;

            case KEY_DOT:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, ">", 1);
                } else {
                    tsm_vte_input(context.vte, ".", 1);
                }
                break;

            case KEY_SLASH:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "?", 1);
                } else {
                    tsm_vte_input(context.vte, "/", 1);
                }
                break;

            case KEY_SEMICOLON:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, ":", 1);
                } else {
                    tsm_vte_input(context.vte, ";", 1);
                }
                break;

            case KEY_APOSTROPHE:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "\"", 1);
                } else {
                    tsm_vte_input(context.vte, "'", 1);
                }
                break;

            case KEY_LEFTBRACE:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "{", 1);
                } else {
                    tsm_vte_input(context.vte, "[", 1);
                }
                break;

            case KEY_RIGHTBRACE:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "}", 1);
                } else {
                    tsm_vte_input(context.vte, "]", 1);
                }
                break;
            
            case KEY_BACKSLASH:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "|", 1);
                } else {
                    tsm_vte_input(context.vte, "\\", 1);
                }
                break;

            case KEY_MINUS:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "_", 1);
                } else {
                    tsm_vte_input(context.vte, "-", 1);
                }
                break;

            case KEY_EQUAL:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "+", 1);
                } else {
                    tsm_vte_input(context.vte, "=", 1);
                }
                break;
            
            case KEY_GRAVE:
                if(context.modifiers.shift) {
                    tsm_vte_input(context.vte, "~", 1);
                } else {
                    tsm_vte_input(context.vte, "`", 1);
                }
                break;

            case KEY_SPACE:
                tsm_vte_input(context.vte, " ", 1);
                break;

            default:
                break;

        }

    }

}


static void tsm_write_cb(struct tsm_vte *vte, const char *u8, size_t len, void *data) {
    
    (void) vte;
    (void) data;

    fprintf(stderr, "tsm_write_cb: %ld bytes '%X'\n", len, *u8);

    //write(context.ipipefd[1], u8, len);

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


   
    //* 2. I/O initialization

    if(pipe2(context.ipipefd, 0) < 0) {
        fprintf(stderr, "aplus-terminal: pipe() failed\n");
        exit(1);
    }

    assert(context.ipipefd[0] >= 0);
    assert(context.ipipefd[1] >= 0);


    if(pipe2(context.opipefd, O_NONBLOCK) < 0) {
        fprintf(stderr, "aplus-terminal: pipe() failed\n");
        exit(1);
    }

    assert(context.opipefd[0] >= 0);
    assert(context.opipefd[1] >= 0);




    //* 3. TSM initialization

    if(tsm_screen_new(&context.con, NULL, NULL) < 0) {
        fprintf(stderr, "aplus-terminal: tsm_screen_new() failed\n");
        exit(1);
    }

    assert(context.con);


    if(tsm_vte_new(&context.vte, context.con, tsm_write_cb, NULL, NULL, NULL) < 0) {
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




    //* 4. Input initialization

    context.kbd = open("/dev/kbd", O_RDONLY);

    if(context.kbd < 0) {
        fprintf(stderr, "aplus-terminal: open() failed: cannot open /dev/kbd: %s\n", strerror(errno));
        exit(1);
    }

    if(fcntl(context.kbd, F_SETFL, O_NONBLOCK) < 0) {
        fprintf(stderr, "aplus-terminal: fcntl() failed: %s\n", strerror(errno));
        exit(1);
    }


    context.mouse = open("/dev/mouse", O_RDONLY);

    if(context.mouse < 0) {
        fprintf(stderr, "aplus-terminal: open() failed: cannot open /dev/mouse: %s\n", strerror(errno));
        exit(1);
    }

    if(fcntl(context.mouse, F_SETFL, O_NONBLOCK) < 0) {
        fprintf(stderr, "aplus-terminal: fcntl() failed: %s\n", strerror(errno));
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


        if(dup2(context.ipipefd[0], STDIN_FILENO) < 0) {
            fprintf(stderr, "aplus-terminal: dup2() failed\n");
            exit(1);
        }

        if(dup2(context.opipefd[1], STDOUT_FILENO) < 0) {
            fprintf(stderr, "aplus-terminal: dup2() failed\n");
            exit(1);
        }

        if(dup2(context.opipefd[1], STDERR_FILENO) < 0) {
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
            execl("/bin/bash", "/bin/bash", "-c", cmd, NULL);
        } else {
            execl("/bin/bash", "/bin/bash", NULL);
        }

        fprintf(stderr, "aplus-terminal: execl() failed: %s\n", strerror(errno));
        exit(1);


    } else {


        do {

            struct pollfd pfd[3] = {
                { .fd = context.kbd,        .events = POLLIN },
                { .fd = context.mouse,      .events = POLLIN },
                { .fd = context.opipefd[0], .events = POLLIN }
            };

            if(poll(pfd, 3, -1) < 0) {
                break;
            }


            for(size_t i = 0; i < (sizeof(pfd) / sizeof(pfd[0])); i++) {
                
                if(pfd[i].revents & POLLERR || pfd[i].revents & POLLHUP || pfd[i].revents & POLLNVAL) {
                    break;
                }

                if(pfd[i].revents & POLLIN) {

                    if(pfd[i].fd == context.opipefd[0]) {

                        char buf[BUFSIZ];
                        ssize_t size;

                        do {

                            while((size = read(context.opipefd[0], buf, sizeof(buf))) > 0) {

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
                                    
                                    tsm_handle_key(context.ipipefd[1], ev.ev_key.vkey, ev.ev_key.down);
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

        
        if(close(context.ipipefd[0]) < 0) {
            fprintf(stderr, "aplus-terminal: close() failed: %s\n", strerror(errno));
            exit(1);
        }

        if(close(context.ipipefd[1]) < 0) {
            fprintf(stderr, "aplus-terminal: close() failed: %s\n", strerror(errno));
            exit(1);
        }

        if(close(context.opipefd[0]) < 0) {
            fprintf(stderr, "aplus-terminal: close() failed: %s\n", strerror(errno));
            exit(1);
        }

        if(close(context.opipefd[1]) < 0) {
            fprintf(stderr, "aplus-terminal: close() failed: %s\n", strerror(errno));
            exit(1);
        }

    }


    return 0;
    
}