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



/* One plot function now, not four: a window surface is always 32-bit, whatever depth the
   framebuffer underneath it happens to be. Converting to the display format is the
   server's job, and the only place that has to know about it.

   Indexed by the surface's own stride rather than by a separately tracked resolution: the
   framebuffer version used var.xres and ignored fix.line_length, which is only ever right when
   the two happen to agree, and the surface is now memory the server laid out rather than memory
   this process allocated. */
static void win_plot(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {

    uint8_t* pixels = (uint8_t*)ui_window_pixels(context.win);

    *(uint32_t*)(pixels + ((size_t)y * ui_window_stride(context.win)) + ((size_t)x * sizeof(uint32_t))) = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}


static int fb_draw_cb(struct tsm_screen* con, uint32_t id, const uint32_t* ch, size_t len, uint32_t width, uint32_t posx, uint32_t posy, const struct tsm_screen_attr* attr, tsm_age_t age, void* data) {

    assert(con);
    assert(attr);
    assert(len < 2);


    /* Skip cells that have not changed since the last draw. tsm_screen_draw() hands back
       the age of the frame and stamps each cell with the age of its last change, so a
       cell no older than the previous frame still holds exactly what was drawn then. An
       age of zero means "unknown", which is what a resize or a first draw produces. */
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


    /* Marking the cell rather than the whole surface is what keeps a keystroke to a few
       hundred bytes on the wire instead of a megabyte. */
    ui_window_damage(context.win, (int)posx, (int)posy, ATERM_FONT_WIDTH, ATERM_FONT_HEIGHT);


    if (gidx > 255) {
        gidx = 0;
    }

    const uint8_t* glyph = &builtin_fontdata[gidx * ATERM_FONT_PITCH];

    for (size_t i = 0; i < ATERM_FONT_HEIGHT; i++) {
        for (size_t j = 0; j < ATERM_FONT_WIDTH; j++) {

            if (glyph[i] & (1 << (ATERM_FONT_WIDTH - 1 - j))) {
                context.plot(posx + j, posy + i, fr, fg, fb);
            } else {
                context.plot(posx + j, posy + i, br, bg, bb);
            }
        }
    }

#else

    gidx = FT_Get_Char_Index(context.face, gidx);

    posx *= 8;
    posy *= 16;

    if ((int)(posx + 8) > ui_window_width(context.win) || (int)(posy + 16) > ui_window_height(context.win))
        return 0;


    ui_window_damage(context.win, (int)posx, (int)posy, 8, 16);


    /* Paint the cell background first, before any of the early returns below.
     *
     * A cell with no glyph still has a background colour. An erased cell in particular --
     * what ESC[J and friends leave behind -- arrives here with len == 0, so gidx is 0 and the
     * font has nothing to draw for it. Bailing out at that point left the previous frame's
     * pixels on screen. For ordinary text that is invisible, because almost every cell has a
     * glyph; for anything that paints with coloured blanks and erases between frames it means
     * most of the screen is never repainted, and the display fills up with streaks of stale
     * content. */
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


    /* A blank glyph (a space) renders to an empty bitmap with no buffer at all; the
       background painted above is the whole of its appearance. */
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


/* context.lock guards *all* of libtsm, not just drawing.
 *
 * The screen and the vte are one shared mutable structure, and tsm_screen_resize() frees
 * and reallocates the cell array. With tsm_vte_input() left outside the lock, a resize on
 * the event thread could pull the lines out from under the pty reader mid-write, which is
 * a use-after-free -- and on x86-64 a wild pointer usually lands non-canonical, so it
 * surfaces as a general protection fault rather than a page fault. Anything that streams
 * output continuously, nyancat being the obvious one, hits it almost every time.
 *
 * Deliberately not held across write() to the pty master: that can block when the shell is
 * slow to read, and the pty reader would then be stuck waiting for the lock instead of
 * draining the output the shell is blocked writing. */
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


/* Take the window's size in pixels and make the terminal, the pty and the shell all agree
   on what it means in character cells. The kernel turns TIOCSWINSZ into a SIGWINCH for the
   foreground process group, so a program that tracks its own size follows along. */
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

    /* Under the lock on purpose: this is what reallocates the pixel buffer, and the pty
       reader may be part way through a repaint into the old one. */
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

    /* The surface underneath is a fresh allocation, so nothing on it survived the resize
       and every cell has to be drawn again however old libtsm thinks it is. */
    context.age = 0;

    //? Repaint before dropping the lock: releasing it first leaves a window in which the
    //? pty reader draws the new grid at whatever size it last saw.
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

    /* Input no longer comes from /dev/kbd. The server owns the keyboard and routes it to
       whichever window has the focus, so what arrives here is already addressed to this
       terminal -- and carries the raw KEY_* code, which is exactly what the keymap below
       still expects. */
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

                //? Left outside the lock because it writes to the pty master, which can
                //? block; it takes the lock itself around the libtsm call it makes.
                tsm_handle_key(context.masterfd, ev.key.vkey, ev.key.down);

                tsm_update_screen();

                break;

            case UI_EVENT_CONFIGURE:
                tsm_resize(ev.configure.width, ev.configure.height);
                break;

            case UI_EVENT_CLOSE: {

                /* Hang up, the way a terminal does when its window goes away, rather than
                 * calling exit() here. exit() from this thread is exit_group(), which
                 * SIGKILLs the thread reading the pty in the middle of whatever it was
                 * doing and leaves the shell orphaned on a terminal nobody is reading.
                 *
                 * SIGHUP tells the session its terminal is gone, which is exactly what has
                 * happened; the shell exits, the read() on the master side ends, and this
                 * process winds down through its normal path with the window destroyed and
                 * the connection closed in order.
                 *
                 * A second request escalates: a shell that ignores SIGHUP, or a full-screen
                 * program that has trapped it, would otherwise leave the button doing
                 * nothing at all. That is what SIGKILL is for -- asking first, forcing only
                 * when asking did not work.
                 */
                /* Hang up on the session, then quit -- what a terminal does when its
                 * window goes away.
                 *
                 * SIGHUP rather than SIGKILL: the shell is being told its terminal is
                 * gone, which is true and which it knows how to act on. Killing it would
                 * take down whatever it happens to be running with no warning, and there
                 * is nothing to gain by it; SIGKILL belongs to a forced close.
                 *
                 * The pause gives the session a moment to act before the pty disappears
                 * underneath it. There is no waiting for it properly from here: this is
                 * not the thread that forked the shell, and sys_wait4() matches children
                 * against the task that is their parent rather than against the process,
                 * so waitpid() on this thread only ever answers ECHILD.
                 *
                 * Then exit, unconditionally. A shell that ignored the hangup is left
                 * orphaned on a pty nobody holds, exactly as it would be anywhere else --
                 * what must not happen is a close button that leaves the window up.
                 */
                if (context.child > 0) {

                    if (kill(-context.child, SIGHUP) < 0) {
                        fprintf(stderr, "aplus-terminal: cannot hang up the session: %s\n", strerror(errno));
                    }

                    usleep(150000);
                }

                /* Quitting is what closes the window: the connection dies with the
                   process, and the server drops a client's windows with its socket. */
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



    //* 1. Window initialization

    //? The window is asked for in character cells: a terminal has no opinion about pixels
    //? beyond what the font makes of them.
    const int cols = 80;
    const int rows = 25;

    if (pthread_mutex_init(&context.lock, NULL) != 0) {
        fprintf(stderr, "aplus-terminal: pthread_mutex_init() failed: %s\n", strerror(errno));
        exit(1);
    }

    /* The server is usually started a line earlier in the same boot script, so retry for a
       few seconds rather than racing it. */
    if ((context.conn = ui_connect(NULL, 5000)) == NULL) {
        fprintf(stderr, "aplus-terminal: cannot reach the display server on %s: %s\n", UI_DEFAULT_SOCKET, strerror(errno));
        exit(1);
    }

    if ((context.win = ui_window_create(context.conn, cols * ATERM_FONT_WIDTH, rows * ATERM_FONT_HEIGHT, "aplus-terminal")) == NULL) {
        fprintf(stderr, "aplus-terminal: ui_window_create() failed: %s\n", strerror(errno));
        exit(1);
    }

    /* The server is free to hand back a smaller window than was asked for, so the grid is
       derived from what actually came back rather than from what was requested. */
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

    /* Driven by the font metrics and the window, not by a hardcoded 8x16 and the screen.
       The "- 1" these lines used to carry existed to keep the last row and column off the
       edge of the framebuffer; inside a window there is nothing to fall off. */
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

    /* /dev/kbd and /dev/mouse belong to the display server now; input arrives as window
       events instead. */
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
    /* pthread_create() reports failure by returning the error number, not by returning -1
       and setting errno, so the "< 0" this used to test was never true. A terminal whose
       event thread failed to start looked completely normal and simply ignored the
       keyboard for the rest of its life. */
    int thr_e = pthread_create(&context.thr_ui, NULL, thr_ui_handler, NULL);

    if (thr_e != 0) {
        fprintf(stderr, "aplus-terminal: pthread_create() failed: %s\n", strerror(thr_e));
        exit(1);
    }



    //* 5. Session initialization

    /* Neither of these is fatal, and a terminal started from inside another one depends on
       that.
     *
       Only the first terminal of a session can lead one: an interactive shell puts each
       job in its own process group, so a terminal it launches is already a group leader
       and setsid() can only answer EPERM. Treating that as fatal is what made "run
       aplus-terminal from aplus-terminal" die on the spot. The session matters for job
       control in the shell below, which sets its own process group on the slave further
       down regardless, so carrying on without it costs the nested terminal nothing that
       was working before. */
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

            /* The shell must not inherit the connection to the display server, nor the
               master side of its own terminal.
             *
             * Leaving the socket open is what made a window outlive the process that owned
             * it: the close button asked this terminal to quit, it did, and the server saw
             * a connection that was still held open by the shell -- so it never learned the
             * client was gone and the window stayed on screen with nothing behind it. */
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

                    /* Repaint once the pending input is drained rather than once per chunk.
                       A full read means there is very likely more of the frame still queued,
                       and redrawing the whole grid at that point paints a screen made of
                       pieces of two different frames -- which is what the tearing looks
                       like when a program animates faster than the terminal can draw. */
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
