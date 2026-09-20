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
#include <cairo/cairo.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libtsm.h>
#include <math.h>
#include <poll.h>
#include <pty.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <aplus/events.h>
#include <aplus/input.h>
#include <aplus/ui-draw.h>
#include <aplus/ui-keymap.h>
#include <aplus/ui.h>

#include <pthread.h>

/**
 * @brief The monospaced faces the terminal draws with, and the pixel size they are scaled to.
 */
#if !defined(CONFIG_ATERM_FONT_PATH)
    #define CONFIG_ATERM_FONT_PATH "/usr/share/fonts/ttf/UbuntuMono-R.ttf"
#endif

#if !defined(CONFIG_ATERM_FONT_BOLD_PATH)
    #define CONFIG_ATERM_FONT_BOLD_PATH "/usr/share/fonts/ttf/UbuntuMono-B.ttf"
#endif

#if !defined(CONFIG_ATERM_FONT_SIZE)
    #define CONFIG_ATERM_FONT_SIZE 16
#endif


/**
 * @brief How many codepoints keep the glyph they map to one array lookup away.
 */
#define ATERM_GLYPH_CACHE_MAX 512


/**
 * @brief A face scaled to the cell size, along with the glyph indices it has already been asked for.
 */
typedef struct {

    cairo_scaled_font_t* scaled;

    int32_t glyphs[ATERM_GLYPH_CACHE_MAX];

} aterm_font_t;


static struct {

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

    ui_keymap_t* keymap;

    struct {
        char* buffer;
        size_t size;
        size_t capacity;
    } input;

    pthread_t thr_ui;

    //? The window's buffer as cairo sees it. The buffer is the server's, so a configure hands
    //? over another one and everything bound to the old one has to be built again.
    cairo_surface_t* surface;
    cairo_t* cr;

    //? A cell is the regular face's widest advance by its line height, which is what makes a
    //? column of text line up: every glyph is drawn at a multiple of it, not at a pen position.
    struct {

        aterm_font_t regular;
        aterm_font_t bold;

        int width;
        int height;
        int baseline;

    } font;

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
 * @brief Binds a cairo context to the buffer the window is holding right now.
 *
 * @return 0 on success, or -1 with errno set.
 */
static int aterm_bind_surface(void) {

    if (context.cr) {
        cairo_destroy(context.cr);
        context.cr = NULL;
    }

    if (context.surface) {
        cairo_surface_destroy(context.surface);
        context.surface = NULL;
    }


    const int width  = ui_window_width(context.win);
    const int height = ui_window_height(context.win);

    unsigned char* pixels = (unsigned char*)ui_window_pixels(context.win);

    if (!pixels || width <= 0 || height <= 0) {
        errno = EINVAL;
        return -1;
    }


    const cairo_format_t format = ui_window_translucent(context.win) ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;

    context.surface = cairo_image_surface_create_for_data(pixels, format, width, height, (int)ui_window_stride(context.win));

    if (cairo_surface_status(context.surface) != CAIRO_STATUS_SUCCESS) {

        cairo_surface_destroy(context.surface);

        context.surface = NULL;
        errno           = ENOMEM;

        return -1;
    }


    context.cr = cairo_create(context.surface);

    if (cairo_status(context.cr) != CAIRO_STATUS_SUCCESS) {

        cairo_destroy(context.cr);
        cairo_surface_destroy(context.surface);

        context.cr      = NULL;
        context.surface = NULL;
        errno           = ENOMEM;

        return -1;
    }

    return 0;
}


/**
 * @brief Loads the faces the terminal draws with and takes the cell size from their metrics.
 *
 * @return 0 on success, or -1 with errno set.
 */
static int aterm_font_init(void) {

    cairo_font_face_t* regular = ui_font_face(CONFIG_ATERM_FONT_PATH);

    if (!regular) {
        errno = ENOENT;
        return -1;
    }

    cairo_font_face_t* bold = ui_font_face(CONFIG_ATERM_FONT_BOLD_PATH);

    if (!bold) {
        bold = regular;
    }


    context.font.regular.scaled = ui_font_scaled(regular, CONFIG_ATERM_FONT_SIZE);
    context.font.bold.scaled    = ui_font_scaled(bold, CONFIG_ATERM_FONT_SIZE);

    if (!context.font.regular.scaled || !context.font.bold.scaled) {
        errno = ENOMEM;
        return -1;
    }


    memset(context.font.regular.glyphs, 0xFF, sizeof(context.font.regular.glyphs));
    memset(context.font.bold.glyphs, 0xFF, sizeof(context.font.bold.glyphs));


    cairo_font_extents_t extents;

    cairo_scaled_font_extents(context.font.regular.scaled, &extents);

    context.font.width    = (int)ceil(extents.max_x_advance);
    context.font.height   = (int)ceil(extents.height);
    context.font.baseline = (int)ceil(extents.ascent);

    if (context.font.width < 1) {
        context.font.width = 1;
    }

    if (context.font.height < 1) {
        context.font.height = 1;
    }

    return 0;
}


/**
 * @brief Encodes one codepoint as UTF-8, which is the only thing cairo maps to a glyph.
 *
 * @param codepoint The codepoint.
 * @param out A buffer of at least four bytes.
 * @return How many bytes were written.
 */
static size_t aterm_utf8_encode(uint32_t codepoint, char* out) {

    if (codepoint < 0x80) {

        out[0] = (char)codepoint;

        return 1;
    }

    if (codepoint < 0x800) {

        out[0] = (char)(0xC0 | (codepoint >> 6));
        out[1] = (char)(0x80 | (codepoint & 0x3F));

        return 2;
    }

    if (codepoint < 0x10000) {

        out[0] = (char)(0xE0 | (codepoint >> 12));
        out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = (char)(0x80 | (codepoint & 0x3F));

        return 3;
    }

    out[0] = (char)(0xF0 | (codepoint >> 18));
    out[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    out[3] = (char)(0x80 | (codepoint & 0x3F));

    return 4;
}


/**
 * @brief Reports the glyph a codepoint stands for in a face, out of the face's own cache.
 *
 * Mapping a character costs a charmap lookup on the face behind the scaled font, and a
 * terminal asks for the same hundred or so characters for as long as it is open.
 *
 * @param font The face to look the codepoint up in.
 * @param codepoint The codepoint.
 * @return The glyph index, or 0 when the face has no glyph for it.
 */
static unsigned long aterm_glyph_index(aterm_font_t* font, uint32_t codepoint) {

    if (codepoint < ATERM_GLYPH_CACHE_MAX && font->glyphs[codepoint] >= 0) {
        return (unsigned long)font->glyphs[codepoint];
    }


    char utf8[4];

    const size_t size = aterm_utf8_encode(codepoint, utf8);

    cairo_glyph_t buffer[4];
    cairo_glyph_t* glyphs = buffer;

    int count = (int)(sizeof(buffer) / sizeof(buffer[0]));

    unsigned long index = 0;

    if (cairo_scaled_font_text_to_glyphs(font->scaled, 0.0, 0.0, utf8, (int)size, &glyphs, &count, NULL, NULL, NULL) == CAIRO_STATUS_SUCCESS) {

        if (count > 0) {
            index = glyphs[0].index;
        }
    }

    if (glyphs != buffer) {
        cairo_glyph_free(glyphs);
    }

    if (codepoint < ATERM_GLYPH_CACHE_MAX) {
        font->glyphs[codepoint] = (int32_t)index;
    }

    return index;
}


/**
 * @brief Draws one cell: its background, the glyph standing in it, and the underline under that.
 *
 * Clipped to the cell, because only the cells libtsm reports as changed are repainted and a
 * glyph reaching past its own advance would otherwise leave a piece of itself in a neighbour
 * that is never drawn again.
 */
static int fb_draw_cb(struct tsm_screen* con, uint32_t id, const uint32_t* ch, size_t len, uint32_t width, uint32_t posx, uint32_t posy, const struct tsm_screen_attr* attr, tsm_age_t age, void* data) {

    (void)id;
    (void)width;
    (void)data;

    assert(con);
    assert(attr);
    assert(len < 2);


    if (age && context.age && age <= context.age) {
        return 0;
    }


    const int x = (int)posx * context.font.width;
    const int y = (int)posy * context.font.height;

    if (x + context.font.width > ui_window_width(context.win) || y + context.font.height > ui_window_height(context.win)) {
        return 0;
    }


    const double fr = (attr->inverse ? attr->br : attr->fr) / 255.0;
    const double fg = (attr->inverse ? attr->bg : attr->fg) / 255.0;
    const double fb = (attr->inverse ? attr->bb : attr->fb) / 255.0;

    const double br = (attr->inverse ? attr->fr : attr->br) / 255.0;
    const double bg = (attr->inverse ? attr->fg : attr->bg) / 255.0;
    const double bb = (attr->inverse ? attr->fb : attr->bb) / 255.0;


    cairo_t* cr = context.cr;

    cairo_save(cr);

    cairo_rectangle(cr, x, y, context.font.width, context.font.height);
    cairo_clip(cr);

    cairo_set_source_rgb(cr, br, bg, bb);
    cairo_paint(cr);

    cairo_set_source_rgb(cr, fr, fg, fb);


    aterm_font_t* font = attr->bold ? &context.font.bold : &context.font.regular;

    const uint32_t codepoint = len ? *ch : 0;

    if (codepoint > ' ') {

        const unsigned long index = aterm_glyph_index(font, codepoint);

        if (index) {

            cairo_glyph_t glyph = {.index = index, .x = (double)x, .y = (double)(y + context.font.baseline)};

            cairo_set_scaled_font(cr, font->scaled);
            cairo_show_glyphs(cr, &glyph, 1);
        }
    }

    if (attr->underline) {

        cairo_rectangle(cr, x, y + context.font.baseline + 1, context.font.width, 1);
        cairo_fill(cr);
    }

    cairo_restore(cr);


    ui_window_damage(context.win, x, y, context.font.width, context.font.height);

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

    unsigned int cols = width / (unsigned int)context.font.width;
    unsigned int rows = height / (unsigned int)context.font.height;

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

    if (aterm_bind_surface() < 0) {
        fprintf(stderr, "aplus-terminal: cannot draw on the window surface: %s\n", strerror(errno));
        pthread_mutex_unlock(&context.lock);
        return;
    }

    context.cols = cols;
    context.rows = rows;

    int resized = tsm_screen_resize(context.con, cols, rows);

    if (resized < 0) {
        fprintf(stderr, "aplus-terminal: cannot resize the screen to %ux%u: %s\n", cols, rows, strerror(-resized));
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
}


static void tsm_handle_input(int out, char* ascii, size_t size, bool handle_internally) {

    assert(ascii);

    if (size == 0) {
        return;
    }

    if (write(out, ascii, size) != (ssize_t)size) {
        perror("write");
    }

    if (handle_internally) {

        pthread_mutex_lock(&context.lock);
        tsm_vte_input(context.vte, ascii, size);
        pthread_mutex_unlock(&context.lock);
    }
}


/**
 * @brief Sends what a key event stands for to the shell.
 *
 * @param out The pty master to write to.
 * @param keysym The key code the display server reported.
 * @param down Whether the key went down or came up.
 */
static void tsm_handle_key(int out, vkey_t keysym, uint8_t down) {

    const uint16_t key = ui_keymap_lookup(context.keymap, keysym);

    char produced[8];

    const size_t size = ui_keymap_translate(context.keymap, keysym, down, produced, sizeof(produced));

    if (size > 0) {
        tsm_handle_input(out, produced, size, false);
        return;
    }

    if (!down) {
        return;
    }

    switch (KTYP(key)) {

        case KT_SPEC:

            if (key == K_ENTER) {
                tsm_handle_input(out, "\n", 1, false);
            }

            break;

        case KT_CUR:

            switch (key) {

                case K_UP:
                    tsm_handle_input(out, "\e[A", 3, true);
                    break;

                case K_DOWN:
                    tsm_handle_input(out, "\e[B", 3, true);
                    break;

                case K_RIGHT:
                    tsm_handle_input(out, "\e[C", 3, true);
                    break;

                case K_LEFT:
                    tsm_handle_input(out, "\e[D", 3, true);
                    break;
            }

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

            case UI_EVENT_FOCUS:
                ui_keymap_reset(context.keymap);
                break;

            case UI_EVENT_POINTER:
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


    //* Font initialization

    if (aterm_font_init() < 0) {
        fprintf(stderr, "aplus-terminal: cannot load %s: %s\n", CONFIG_ATERM_FONT_PATH, strerror(errno));
        exit(1);
    }


    if ((context.win = ui_window_create(context.conn, cols * context.font.width, rows * context.font.height, "aplus-terminal")) == NULL) {
        fprintf(stderr, "aplus-terminal: ui_window_create() failed: %s\n", strerror(errno));
        exit(1);
    }

    if (aterm_bind_surface() < 0) {
        fprintf(stderr, "aplus-terminal: cannot draw on the window surface: %s\n", strerror(errno));
        exit(1);
    }

    context.cols = (unsigned int)(ui_window_width(context.win) / context.font.width);
    context.rows = (unsigned int)(ui_window_height(context.win) / context.font.height);

    if (context.cols < 1) {
        context.cols = 1;
    }

    if (context.rows < 1) {
        context.rows = 1;
    }


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


    int resized = tsm_screen_resize(context.con, context.cols, context.rows);

    if (resized < 0) {
        fprintf(stderr, "aplus-terminal: cannot resize the screen to %ux%u: %s\n", context.cols, context.rows, strerror(-resized));
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

    context.keymap = ui_keymap_open(NULL);

    if (!context.keymap) {
        fprintf(stderr, "aplus-terminal: cannot load the keymap: %s\n", strerror(errno));
        exit(1);
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
