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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <aplus/events.h>
#include <aplus/input.h>
#include <aplus/ui.h>

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

    //? The shell running on the other side of the pty, and its process group -- it calls
    //? setpgrp(), so the two are the same number. Closing the window hangs up on that
    //? group rather than tearing this process down underneath it.
    pid_t child;

    struct tsm_screen* con;
    struct tsm_vte* vte;

    //? The window this terminal lives in. Its pixel buffer is ordinary heap memory: the
    //? server is a separate process and this kernel has no shared memory to put a surface
    //? in, so what gets committed travels down the socket as damaged rectangles.
    ui_connection_t* conn;
    ui_window_t* win;

    //? libtsm stamps every cell with the age of its last change. Keeping the age of the
    //? previous draw is what turns a repaint into "the cells that actually moved", which
    //? is the difference between shipping one cell per keystroke and shipping a frame.
    tsm_age_t age;

    unsigned int cols;
    unsigned int rows;

    //? The pty reader and the event reader both draw. They always did; with a commit on
    //? the end of a draw, two interleaved repaints would also interleave on the wire.
    pthread_mutex_t lock;

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

    pthread_t thr_ui;


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



/**
 * @brief Plots one pixel into the window surface, which is 32-bit whatever the framebuffer's depth is.
 *
 * @param x The column to plot.
 * @param y The row to plot.
 * @param r The red component.
 * @param g The green component.
 * @param b The blue component.
 */
static void win_plot(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {

    uint8_t* pixels = (uint8_t*)ui_window_pixels(context.win);

    *(uint32_t*)(pixels + ((size_t)y * ui_window_stride(context.win)) + ((size_t)x * sizeof(uint32_t))) = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}


static int fb_draw_cb(struct tsm_screen* con, uint32_t id, const uint32_t* ch, size_t len, uint32_t width, uint32_t posx, uint32_t posy, const struct tsm_screen_attr* attr, tsm_age_t age, void* data) {

    assert(con);
    assert(attr);
    assert(len < 2);


    if (age && context.age && age <= context.age) {
        return 0;
    }


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

    if ((int)(posx + ATERM_FONT_WIDTH) > ui_window_width(context.win) || (int)(posy + ATERM_FONT_HEIGHT) > ui_window_height(context.win))
        return 0;


    ui_window_damage(context.win, (int)posx, (int)posy, ATERM_FONT_WIDTH, ATERM_FONT_HEIGHT);


    if (gidx > 255) {
        gidx = 0;
    }

    const uint8_t* glyph = &builtin_fontdata[gidx * ATERM_FONT_PITCH];

    uint8_t* const base = (uint8_t*)ui_window_pixels(context.win);
    const size_t stride = ui_window_stride(context.win);

    const uint32_t fg32 = 0xFF000000u | ((uint32_t)fr << 16) | ((uint32_t)fg << 8) | fb;
    const uint32_t bg32 = 0xFF000000u | ((uint32_t)br << 16) | ((uint32_t)bg << 8) | bb;

    for (size_t i = 0; i < ATERM_FONT_HEIGHT; i++) {

        uint32_t* row      = (uint32_t*)(base + ((size_t)posy + i) * stride) + posx;
        const uint8_t bits = glyph[i];

        for (size_t j = 0; j < ATERM_FONT_WIDTH; j++) {

            const uint32_t m = 0u - ((bits >> (ATERM_FONT_WIDTH - 1 - j)) & 1u);

            row[j] = (fg32 & m) | (bg32 & ~m);
        }
    }

#else

    gidx = FT_Get_Char_Index(context.face, gidx);

    posx *= 8;
    posy *= 16;

    if ((int)(posx + 8) > ui_window_width(context.win) || (int)(posy + 16) > ui_window_height(context.win))
        return 0;


    ui_window_damage(context.win, (int)posx, (int)posy, 8, 16);


    for (size_t i = 0; i < 16; i++) {
        for (size_t j = 0; j < 8; j++) {
            context.plot(posx + j, posy + i, br, bg, bb);
        }
    }


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


    if (!context.face->glyph->bitmap.buffer) {
        return 0;
    }


    int bbox_ymax   = context.face->bbox.yMax / 64;
    int glyph_width = context.face->glyph->metrics.width / 64;
    int advance     = context.face->glyph->metrics.horiAdvance / 64;
    int x_off       = (advance - glyph_width) / 2;
    int y_off       = bbox_ymax - (context.face->glyph->metrics.horiBearingY / 64);


    for (size_t i = 0; i < context.face->glyph->bitmap.rows; i++) {
        for (size_t j = 0; j < context.face->glyph->bitmap.width; j++) {

            char ch = context.face->glyph->bitmap.buffer[i * context.face->glyph->bitmap.pitch + j / 8];

            if (ch & (1 << (7 - j % 8))) {
                context.plot(posx + j + x_off, posy + i + y_off, fr, fg, fb);
            }
        }
    }



#endif



    return 0;
}


/**
 * @brief Draws the screen with context.lock held, which guards the whole of libtsm rather than the drawing alone.
 *
 * Never held across a write() to the pty master, which can block when the shell is slow to read.
 */
static void tsm_draw_locked(void) {

    context.age = tsm_screen_draw(context.con, fb_draw_cb, NULL);

    if (ui_window_commit(context.win) < 0) {
        fprintf(stderr, "aplus-terminal: ui_window_commit() failed: %s\n", strerror(errno));
    }
}


static void tsm_update_screen() {

    pthread_mutex_lock(&context.lock);

    tsm_draw_locked();

    pthread_mutex_unlock(&context.lock);
}


/**
 * @brief Makes the terminal, the pty and the shell agree on what the window's size means in character cells.
 *
 * @param width The window width in pixels.
 * @param height The window height in pixels.
 */
static void tsm_resize(unsigned int width, unsigned int height) {

    unsigned int cols = width / ATERM_FONT_WIDTH;
    unsigned int rows = height / ATERM_FONT_HEIGHT;

    if (cols < 1) {
        cols = 1;
    }

    if (rows < 1) {
        rows = 1;
    }


    pthread_mutex_lock(&context.lock);

    if (ui_window_apply_configure(context.win) < 0) {
        fprintf(stderr, "aplus-terminal: ui_window_apply_configure() failed: %s\n", strerror(errno));
        pthread_mutex_unlock(&context.lock);
        return;
    }

    context.cols = cols;
    context.rows = rows;

    if (tsm_screen_resize(context.con, cols, rows) < 0) {
        fprintf(stderr, "aplus-terminal: tsm_screen_resize() failed\n");
    }

    context.age = 0;

    tsm_draw_locked();

    pthread_mutex_unlock(&context.lock);


    if (context.masterfd > 0) {

        struct winsize ws = {

            .ws_col    = (unsigned short)cols,
            .ws_row    = (unsigned short)rows,
            .ws_xpixel = (unsigned short)width,
            .ws_ypixel = (unsigned short)height,
        };

        if (ioctl(context.masterfd, TIOCSWINSZ, &ws) < 0) {
            fprintf(stderr, "aplus-terminal: ioctl(TIOCSWINSZ) failed: %s\n", strerror(errno));
        }
    }

    tsm_update_screen();
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

        pthread_mutex_lock(&context.lock);
        tsm_vte_input(context.vte, ascii, size);
        pthread_mutex_unlock(&context.lock);
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

static void* thr_ui_handler(void* arg) {

    (void)arg;

    for (;;) {

        ui_event_t ev;

        int e = ui_next_event(context.conn, &ev, -1);

        if (e < 0) {

            if (errno == EINTR) {
                continue;
            }

            fprintf(stderr, "aplus-terminal: lost the connection to the display server: %s\n", strerror(errno));
            exit(1);
        }

        if (e == 0) {
            continue;
        }


        switch (ev.type) {

            case UI_EVENT_KEY:

                if (!ev.key.down && !ev.key.vkey) {
                    break;
                }

                pthread_mutex_lock(&context.lock);

                if (tsm_vte_handle_keyboard(context.vte, ev.key.vkey, 0, 0, 0)) {
                    tsm_screen_sb_reset(context.con);
                }

                pthread_mutex_unlock(&context.lock);

                tsm_handle_key(context.masterfd, ev.key.vkey, ev.key.down);

                tsm_update_screen();

                break;

            case UI_EVENT_CONFIGURE:
                tsm_resize(ev.configure.width, ev.configure.height);
                break;

            case UI_EVENT_CLOSE: {

                if (context.child > 0) {

                    if (kill(-context.child, SIGHUP) < 0) {
                        fprintf(stderr, "aplus-terminal: cannot hang up the session: %s\n", strerror(errno));
                    }

                    usleep(150000);
                }

                exit(0);
            }

            case UI_EVENT_POINTER:
            case UI_EVENT_FOCUS:
            default:
                break;
        }
    }

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



    const int cols = 80;
    const int rows = 25;

    if (pthread_mutex_init(&context.lock, NULL) != 0) {
        fprintf(stderr, "aplus-terminal: pthread_mutex_init() failed: %s\n", strerror(errno));
        exit(1);
    }

    if ((context.conn = ui_connect(NULL, 5000)) == NULL) {
        fprintf(stderr, "aplus-terminal: cannot reach the display server on %s: %s\n", UI_DEFAULT_SOCKET, strerror(errno));
        exit(1);
    }

    if ((context.win = ui_window_create(context.conn, cols * ATERM_FONT_WIDTH, rows * ATERM_FONT_HEIGHT, "aplus-terminal")) == NULL) {
        fprintf(stderr, "aplus-terminal: ui_window_create() failed: %s\n", strerror(errno));
        exit(1);
    }

    context.cols = (unsigned int)(ui_window_width(context.win) / ATERM_FONT_WIDTH);
    context.rows = (unsigned int)(ui_window_height(context.win) / ATERM_FONT_HEIGHT);

    if (context.cols < 1) {
        context.cols = 1;
    }

    if (context.rows < 1) {
        context.rows = 1;
    }


    context.plot = win_plot;



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


    if (tsm_screen_resize(context.con, context.cols, context.rows) < 0) {
        fprintf(stderr, "aplus-terminal: tsm_screen_resize() failed\n");
        exit(1);
    }


    // Set Line Mode LNM
    tsm_vte_input(context.vte, "\e[20h", 5);

    // Draw first frame
    tsm_update_screen();



    //* 2. I/O initialization

    struct winsize ws;

    ws.ws_col    = (unsigned short)context.cols;
    ws.ws_row    = (unsigned short)context.rows;
    ws.ws_xpixel = (unsigned short)ui_window_width(context.win);
    ws.ws_ypixel = (unsigned short)ui_window_height(context.win);

    if (openpty(&context.masterfd, &context.slavefd, NULL, NULL, &ws) < 0) {
        fprintf(stderr, "aplus-terminal: cannot open pseudo-terminal: %s\n", strerror(errno));
        exit(1);
    }


    //* 4. Input initialization

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
    int thr_e = pthread_create(&context.thr_ui, NULL, thr_ui_handler, NULL);

    if (thr_e != 0) {
        fprintf(stderr, "aplus-terminal: pthread_create() failed: %s\n", strerror(thr_e));
        exit(1);
    }



    //* 5. Session initialization

    if (setsid() < 0 && errno != EPERM) {
        fprintf(stderr, "aplus-terminal: setsid() failed: %s\n", strerror(errno));
        exit(1);
    }

    if (ioctl(context.masterfd, TIOCSCTTY, 0) < 0) {
        fprintf(stderr, "aplus-terminal: warning: cannot claim the pty as a controlling terminal: %s\n", strerror(errno));
    }



    //* 5. Child initialization

    do {

        pid_t pid = fork();

        if (pid < 0) {

            fprintf(stderr, "aplus-terminal: fork() failed\n");
            exit(1);

        } else if (pid == 0) {

            close(ui_connection_fd(context.conn));
            close(context.masterfd);

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
                execl("/bin/dash", "/bin/dash", "-c", cmd, NULL);
            } else {
                execl("/bin/dash", "/bin/dash", NULL);
            }

            fprintf(stderr, "aplus-terminal: execl() failed: %s\n", strerror(errno));
            exit(1);


        } else {

            context.child = pid;


            char buf[BUFSIZ] = {0};

            do {

                ssize_t size;
                bool pending = false;

                while ((size = read(context.masterfd, buf, sizeof(buf))) > 0) {

                    pthread_mutex_lock(&context.lock);
                    tsm_vte_input(context.vte, buf, size);
                    pthread_mutex_unlock(&context.lock);

                    pending = true;

                    if (size < (ssize_t)sizeof(buf)) {

                        tsm_update_screen();
                        pending = false;
                    }
                }

                if (pending) {
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
