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


#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libtsm.h>
#include <poll.h>
#include <pty.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <aplus/events.h>
#include <aplus/fb.h>
#include <aplus/input.h>

#include <pthread.h>

#define CONFIG_ATERM_BUILTIN_FONT 1

#if defined(CONFIG_ATERM_BUILTIN_FONT)
    #include "lib/builtin_font.h"
#else
    #include <freetype2/ft2build.h>
    #include FT_FREETYPE_H
#endif


static struct {

        void (*plot)(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b);

        int masterfd;
        int slavefd;

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

                struct {

                        struct {
                                union {
                                        struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
                                                uint8_t val;
                                                uint8_t typ;
#else
                                                uint8_t typ;
                                                uint8_t val;
#endif
                                        } __attribute__((packed));

                                        uint16_t raw;
                                };
                        } __attribute__((packed)) keys[NR_KEYS];

                } __attribute__((packed)) maps[256];

                uint8_t modifiers;

        } keymap;

        struct {
                char* buffer;
                size_t size;
                size_t capacity;
        } input;

        pthread_t thr_kbd;
        pthread_t thr_mouse;


#if !defined(CONFIG_ATERM_BUILTIN_FONT)
        FT_Library ft;
        FT_Face face;
#endif

} context = {0};



static void show_usage(int argc, char** argv) {
    printf("Use: aplus-terminal [options]... [STRING]...\n"
           "Print STRING(s) to standard output.\n\n"
           "   -c, --command               run command on shell\n"
           "   -w, --working-dir           set current working directory\n"
           "   -n                          no newline at end of output\n"
           "       --help                  show this help\n"
           "       --version               print version info and exit\n");

    exit(0);
}

static void show_version(int argc, char** argv) {
    printf("%s (aplus coreutils) 0.1\n"
           "Copyright (c) %s Antonino Natale.\n"
           "Built with gcc %s (%s)\n",

           argv[0], &__DATE__[7], __VERSION__, __TIMESTAMP__);

    exit(0);
}



static void fb_plot_8(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {

    ((uint8_t*)(context.fix.smem_start))[(y * context.var.xres) + x] = (r >> 5) << 5 | (g >> 5) << 2 | (b >> 6);
}

static void fb_plot_16(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {

    ((uint16_t*)(context.fix.smem_start))[(y * context.var.xres) + x] = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3);
}

static void fb_plot_24(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {

    ((uint8_t*)(context.fix.smem_start))[(y * context.var.xres * 3) + (x * 3) + 0] = r;
    ((uint8_t*)(context.fix.smem_start))[(y * context.var.xres * 3) + (x * 3) + 1] = g;
    ((uint8_t*)(context.fix.smem_start))[(y * context.var.xres * 3) + (x * 3) + 2] = b;
}

static void fb_plot_32(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {

    ((uint32_t*)(context.fix.smem_start))[(y * context.var.xres) + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
}


static int fb_draw_cb(struct tsm_screen* con, uint32_t id, const uint32_t* ch, size_t len, uint32_t width, uint32_t posx, uint32_t posy, const struct tsm_screen_attr* attr, tsm_age_t age, void* data) {

    assert(con);
    assert(attr);
    assert(len < 2);


    uint8_t fr;
    uint8_t fg;
    uint8_t fb;
    uint8_t br;
    uint8_t bg;
    uint8_t bb;

    if (attr->inverse) {
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

    posx *= ATERM_FONT_WIDTH;
    posy *= ATERM_FONT_HEIGHT;

    if (posx + ATERM_FONT_WIDTH >= context.var.xres || posy + ATERM_FONT_HEIGHT >= context.var.yres)
        return 0;


    if (gidx > 255) {
        gidx = 0;
    }

    const uint8_t* glyph = &builtin_fontdata[gidx * ATERM_FONT_PITCH];

    for (size_t i = 0; i < ATERM_FONT_HEIGHT; i++) {
        for (size_t j = 0; j < ATERM_FONT_WIDTH; j++) {

            if (glyph[i] & (1 << (ATERM_FONT_WIDTH - j))) {
                context.plot(posx + j, posy + i, fr, fg, fb);
            } else {
                context.plot(posx + j, posy + i, br, bg, bb);
            }
        }
    }

#else

    gidx = FT_Get_Char_Index(context.face, gidx);

    if (gidx == 0) {
        return 0;
    }

    // draw with freetype
    if (FT_Load_Glyph(context.face, gidx, FT_LOAD_DEFAULT)) {
        return 0;
    }

    if (FT_Render_Glyph(context.face->glyph, FT_RENDER_MODE_MONO)) {
        return 0;
    }


    assert(context.face);
    assert(context.face->glyph);
    assert(context.face->glyph->bitmap.buffer);


    int bbox_ymax   = context.face->bbox.yMax / 64;
    int glyph_width = context.face->glyph->metrics.width / 64;
    int advance     = context.face->glyph->metrics.horiAdvance / 64;
    int x_off       = (advance - glyph_width) / 2;
    int y_off       = bbox_ymax - (context.face->glyph->metrics.horiBearingY / 64);



    posx *= 8;
    posy *= 16;

    if (posx + context.face->glyph->bitmap.width >= context.var.xres || posy + context.face->glyph->bitmap.rows >= context.var.yres)
        return 0;


    for (size_t i = 0; i < 16; i++) {
        for (size_t j = 0; j < 8; j++) {
            context.plot(posx + j, posy + i, br, bg, bb);
        }
    }

    for (size_t i = 0; i < context.face->glyph->bitmap.rows; i++) {
        for (size_t j = 0; j < context.face->glyph->bitmap.width; j++) {

            char ch = context.face->glyph->bitmap.buffer[i * context.face->glyph->bitmap.pitch + j / 8];

            if (ch & (1 << (7 - j % 8))) {
                context.plot(posx + j + x_off, posy + i + y_off, fr, fg, fb);
            }
        }
    }



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


static void tsm_update_screen() {
    tsm_screen_draw(context.con, fb_draw_cb, NULL);
}


static void tsm_handle_input(int out, char* ascii, size_t size, bool handle_internally) {

    assert(ascii);

    if (size == 0) {
        return;
    }

    if (write(out, ascii, size) != size) {
        perror("write");
    }

    if (handle_internally) {
        tsm_vte_input(context.vte, ascii, size);
    }
}


static void tsm_handle_key(int out, vkey_t keysym, uint8_t down) {

    if (keysym > NR_KEYS) {
        return;
    }


#define KEY context.keymap.maps[context.keymap.modifiers].keys[keysym]


    switch (KEY.typ) {

        case KT_LATIN:

            if (down) {
                tsm_handle_input(out, (char*)&KEY.val, 1, false);
            }

            break;

        case KT_FN:

            break;

        case KT_SPEC:

            switch (KEY.raw) {

                case K_ENTER:

                    if (down) {
                        tsm_handle_input(out, "\n", 1, false);
                    }

                    break;

                default:

                    break;
            }

            break;

        case KT_PAD:

            break;

        case KT_DEAD:

            break;

        case KT_CONS:

            break;

        case KT_CUR:

            switch (KEY.raw) {

                case K_UP:

                    if (down) {
                        tsm_handle_input(out, "\e[A", 3, true);
                    }

                    break;

                case K_DOWN:

                    if (down) {
                        tsm_handle_input(out, "\e[B", 3, true);
                    }

                    break;

                case K_RIGHT:

                    if (down) {
                        tsm_handle_input(out, "\e[C", 3, true);
                    }

                    break;

                case K_LEFT:

                    if (down) {
                        tsm_handle_input(out, "\e[D", 3, true);
                    }

                    break;
            }

            break;

        case KT_SHIFT:

            if (down) {
                context.keymap.modifiers |= (1 << (KEY.val));
            } else {
                context.keymap.modifiers &= ~(1 << (KEY.val));
            }

            break;

        case KT_LOCK:

            if (down) {
                context.keymap.modifiers ^= 1 << (KEY.val);
            }

            break;

        case KT_ASCII:

            if (down) {
                tsm_handle_input(out, (char*)&KEY.val, 1, false);
            }

            break;

        case KT_LETTER:

            if (down) {
                tsm_handle_input(out, (char*)&KEY.val, 1, false);
            }

            break;

        case KT_META:

            break;

        case KT_SLOCK:

            break;

        case KT_BRL:

            break;

        default:

            fprintf(stderr, "Unknown key type: %04X (%d)\n", KEY.raw, keysym);

            break;
    }
}


static void tsm_write_cb(struct tsm_vte* vte, const char* u8, size_t len, void* data) {

    (void)vte;
    (void)data;

    // fprintf(stderr, "tsm_write_cb: %ld bytes '%X'\n", len, *u8);

    // write(context.ipipefd[1], u8, len);
}

static void* thr_kbd_handler(void* arg) {

    (void)arg;

    do {

        event_t ev = {0};

        do {

            if (read(context.kbd, &ev, sizeof(ev)) > 0) {

                if (ev.ev_type == EV_KEY) {

                    if (tsm_vte_handle_keyboard(context.vte, ev.ev_key.vkey, 0, 0, 0)) {
                        tsm_screen_sb_reset(context.con);
                    }

                    tsm_handle_key(context.masterfd, ev.ev_key.vkey, ev.ev_key.down);
                    tsm_update_screen();
                }
            }

        } while (errno == EINTR);

    } while (true);


    return NULL;
}

static void* thr_mouse_handler(void* arg) {

    (void)arg;


    do {

        event_t ev = {0};

        do {

            while (read(context.mouse, &ev, sizeof(ev)) == sizeof(ev)) {

                if (ev.ev_type == EV_REL) {

                    context.cursor.x += ev.ev_rel.x * 3;
                    context.cursor.y -= ev.ev_rel.y * 3;

                    if (context.cursor.x > context.var.xres_virtual) {
                        context.cursor.x = context.var.xres_virtual;
                    }

                    if (context.cursor.y > context.var.yres_virtual) {
                        context.cursor.y = context.var.yres_virtual;
                    }

                    tsm_update_screen();
                }
            }

        } while (errno == EINTR);

    } while (true);


    return NULL;
}



int main(int argc, char** argv) {


    static struct option long_options[] = {
        {"command",     required_argument, NULL, 'c'},
        {"working-dir", required_argument, NULL, 'w'},
        {"help",        no_argument,       NULL, 'h'},
        {"version",     no_argument,       NULL, 'v'},
        {NULL,          0,                 NULL, 0  }
    };



    char* cmd = NULL;
    char* pwd = NULL;


    int c, idx;
    while ((c = getopt_long(argc, argv, "c:w:vh", long_options, &idx)) != -1) {
        switch (c) {
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

    if ((fd = open("/dev/fb0", O_RDWR)) < 0) {
        fprintf(stderr, "aplus-terminal: open() failed: cannot open /dev/fb0: %s\n", strerror(errno));
        exit(1);
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &context.var) < 0) {
        fprintf(stderr, "aplus-terminal: ioctl() failed: %s\n", strerror(errno));
        exit(1);
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &context.fix) < 0) {
        fprintf(stderr, "aplus-terminal: ioctl() failed: %s\n", strerror(errno));
        exit(1);
    }

    if (!context.fix.smem_start || !context.var.xres_virtual || !context.var.yres_virtual) {
        fprintf(stderr, "aplus-terminal: wrong framebuffer configuration\n");
        exit(1);
    }

    if (close(fd) < 0) {
        fprintf(stderr, "aplus-terminal: close() failed: %s\n", strerror(errno));
        exit(1);
    }


    switch (context.var.bits_per_pixel) {

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



        //* Font initialization

#if !defined(CONFIG_ATERM_BUILTIN_FONT)

    if (FT_Init_FreeType(&context.ft)) {
        fprintf(stderr, "aplus-terminal: cannot initialize freetype library\n");
        exit(1);
    }


    struct stat st;

    if (stat("/usr/share/fonts/ttf/UbuntuMono-R.ttf", &st) < 0) {
        fprintf(stderr, "aplus-terminal: cannot stat() font file: %s\n", strerror(errno));
        exit(1);
    }

    void* font = malloc(st.st_size);

    if (!font) {
        fprintf(stderr, "aplus-terminal: cannot allocate memory for font file\n");
        exit(1);
    }

    int ffd = open("/usr/share/fonts/ttf/UbuntuMono-R.ttf", O_RDONLY);

    if (ffd < 0) {
        fprintf(stderr, "aplus-terminal: cannot open font file: %s\n", strerror(errno));
        exit(1);
    }

    if (read(ffd, font, st.st_size) != st.st_size) {
        fprintf(stderr, "aplus-terminal: cannot read font file: %s\n", strerror(errno));
        exit(1);
    }

    if (close(ffd) < 0) {
        fprintf(stderr, "aplus-terminal: cannot close font file: %s\n", strerror(errno));
        exit(1);
    }

    if (FT_New_Memory_Face(context.ft, (const FT_Byte*)font, st.st_size, 0, &context.face)) {
        fprintf(stderr, "aplus-terminal: cannot load font\n");
        exit(1);
    }

    FT_Set_Pixel_Sizes(context.face, 0, 16);

#endif



    //* 3. TSM initialization

    if (tsm_screen_new(&context.con, NULL, NULL) < 0) {
        fprintf(stderr, "aplus-terminal: tsm_screen_new() failed\n");
        exit(1);
    }

    assert(context.con);


    if (tsm_vte_new(&context.vte, context.con, tsm_write_cb, NULL, NULL, NULL) < 0) {
        fprintf(stderr, "aplus-terminal: tsm_vte_new() failed\n");
        exit(1);
    }

    assert(context.vte);


    if (tsm_screen_resize(context.con, context.var.xres_virtual / 8 - 1, context.var.yres_virtual / 16 - 1) < 0) {
        fprintf(stderr, "aplus-terminal: tsm_screen_resize() failed\n");
        exit(1);
    }


    // Set Line Mode LNM
    tsm_vte_input(context.vte, "\e[20h", 5);

    // Draw first frame
    tsm_update_screen();



    //* 2. I/O initialization

    struct winsize ws;

    ws.ws_col    = context.var.xres_virtual / 8 - 1;
    ws.ws_row    = context.var.yres_virtual / 16 - 1;
    ws.ws_xpixel = context.var.xres_virtual;
    ws.ws_ypixel = context.var.yres_virtual;

    if (openpty(&context.masterfd, &context.slavefd, NULL, NULL, &ws) < 0) {
        fprintf(stderr, "aplus-terminal: cannot open pseudo-terminal: %s\n", strerror(errno));
        exit(1);
    }


    //* 4. Input initialization

    context.kbd = open("/dev/kbd", O_RDONLY);

    if (context.kbd < 0) {
        fprintf(stderr, "aplus-terminal: open() failed: cannot open /dev/kbd: %s\n", strerror(errno));
        exit(1);
    }


    context.mouse = open("/dev/mouse", O_RDONLY);

    if (context.mouse < 0) {
        fprintf(stderr, "aplus-terminal: open() failed: cannot open /dev/mouse: %s\n", strerror(errno));
        exit(1);
    }


    memset(&context.input, 0, sizeof(context.input));



    //* Load Keymap

    {

#define KEYMAP_LANG "it"

        int fd = open("/usr/share/keymaps/" KEYMAP_LANG ".map", O_RDONLY);

        if (fd < 0) {
            fprintf(stderr, "aplus-terminal: open() failed: cannot open /usr/share/keymaps/" KEYMAP_LANG ".map: %s\n", strerror(errno));
            exit(1);
        }

        char magic[8];

        if (read(fd, magic, 8) != 8) {
            fprintf(stderr, "aplus-terminal: read() failed: cannot read /usr/share/keymaps/" KEYMAP_LANG ".map: %s\n", strerror(errno));
            exit(1);
        }

        if (memcmp(magic, "KMAP\x00\x00\x00\x00", 8) != 0) {
            fprintf(stderr, "aplus-terminal: wrong keymap format\n");
            exit(1);
        }

        if (read(fd, &context.keymap.maps, sizeof(context.keymap.maps)) < 0) {
            fprintf(stderr, "aplus-terminal: read() failed: %s\n", strerror(errno));
            exit(1);
        }

        if (close(fd) < 0) {
            fprintf(stderr, "aplus-terminal: close() failed: %s\n", strerror(errno));
            exit(1);
        }
    }

    //* 4. Input initialization
    if (pthread_create(&context.thr_kbd, NULL, thr_kbd_handler, NULL) < 0) {
        fprintf(stderr, "aplus-terminal: pthread_create() failed: %s\n", strerror(errno));
        exit(1);
    }

    // if(pthread_create(&context.thr_mouse, NULL, thr_mouse_handler, NULL) < 0) {
    //     fprintf(stderr, "aplus-terminal: pthread_create() failed: %s\n", strerror(errno));
    //     exit(1);
    // }

    (void)thr_mouse_handler;



    //* 5. Session initialization

    if (setsid() < 0) {
        fprintf(stderr, "aplus-terminal: setsid() failed: %s\n", strerror(errno));
        exit(1);
    }

    if (ioctl(context.masterfd, TIOCSCTTY, 0) < 0) {
        fprintf(stderr, "aplus-terminal: ioctl() failed: %s\n", strerror(errno));
        exit(1);
    }



    //* 5. Child initialization

    do {

        pid_t pid = fork();

        if (pid < 0) {

            fprintf(stderr, "aplus-terminal: fork() failed\n");
            exit(1);

        } else if (pid == 0) {

            setenv("TERM", "xterm-256color", 1);
            setenv("COLORTERM", "truecolor", 1);
            setenv("TERMINFO", "/usr/share/terminfo/x/xterm-256color", 1);
            setenv("COLORFGBG", "7;0", 1);


            pid_t pgrp = getpid();

            if (setpgrp() < 0) {
                fprintf(stderr, "aplus-terminal: setpgrp() failed: %s\n", strerror(errno));
                exit(1);
            }

            if (ioctl(context.slavefd, TIOCSPGRP, &pgrp) < 0) {
                fprintf(stderr, "aplus-terminal: ioctl() failed: %s\n", strerror(errno));
                exit(1);
            }

            if (dup2(context.slavefd, STDIN_FILENO) < 0) {
                fprintf(stderr, "aplus-terminal: dup2() failed\n");
                exit(1);
            }

            if (dup2(context.slavefd, STDOUT_FILENO) < 0) {
                fprintf(stderr, "aplus-terminal: dup2() failed\n");
                exit(1);
            }

            if (dup2(context.slavefd, STDERR_FILENO) < 0) {
                fprintf(stderr, "aplus-terminal: dup2() failed\n");
                exit(1);
            }


            if (pwd) {
                if (chdir(pwd) < 0) {
                    fprintf(stderr, "aplus-terminal: chdir() failed: %s\n", strerror(errno));
                    exit(1);
                }
            }


            if (cmd) {
                execl("/bin/bash", "/bin/bash", "-c", cmd, NULL);
            } else {
                execl("/bin/bash", "/bin/bash", NULL);
            }

            fprintf(stderr, "aplus-terminal: execl() failed: %s\n", strerror(errno));
            exit(1);


        } else {

            char buf[BUFSIZ] = {0};

            do {

                ssize_t size;

                while ((size = read(context.masterfd, buf, sizeof(buf))) > 0) {

                    tsm_vte_input(context.vte, buf, size);
                    tsm_update_screen();
                }

            } while (errno == EINTR);

            if (waitpid(pid, NULL, 0) < 0) {
                fprintf(stderr, "aplus-terminal: waitpid() failed: %s\n", strerror(errno));
                exit(1);
            }
        }

    } while (true);



    if (close(context.slavefd) < 0) {
        fprintf(stderr, "aplus-terminal: close() failed: %s\n", strerror(errno));
        exit(1);
    }

    if (close(context.masterfd) < 0) {
        fprintf(stderr, "aplus-terminal: close() failed: %s\n", strerror(errno));
        exit(1);
    }

    return 0;
}
