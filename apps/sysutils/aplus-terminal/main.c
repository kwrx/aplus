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
#include <ghostty/vt.h>
#include <math.h>
#include <poll.h>
#include <pty.h>
#include <pwd.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <aplus/fb.h>
#include <aplus/events.h>
#include <aplus/input.h>
#include <aplus/ui-draw.h>
#include <aplus/ui-keymap.h>
#include <aplus/ui-widgets.h>
#include <aplus/ui.h>
#include <aplus/ulog.h>


#include <pthread.h>

/**
 * @brief The monospaced faces the terminal draws with, and the pixel size they are scaled to, when the theme names none.
 */
#if !defined(CONFIG_ATERM_FONT_PATH)
    #define CONFIG_ATERM_FONT_PATH "/usr/share/fonts/ttf/JetBrainsMono-Regular.ttf"
#endif

#if !defined(CONFIG_ATERM_FONT_BOLD_PATH)
    #define CONFIG_ATERM_FONT_BOLD_PATH "/usr/share/fonts/ttf/JetBrainsMono-Bold.ttf"
#endif

#if !defined(CONFIG_ATERM_FONT_SIZE)
    #define CONFIG_ATERM_FONT_SIZE 14
#endif

#if !defined(CONFIG_DEFAULT_SHELL)
    #define CONFIG_DEFAULT_SHELL "/bin/bash"
#endif


/**
 * @brief How many codepoints keep the glyph they map to one array lookup away.
 */
#define ATERM_GLYPH_CACHE_MAX 512

/**
 * @brief The space between the window's edge and the character cells.
 */
#define ATERM_PADDING 10

/**
 * @brief The window's name, and the suffix put after any title the shell sets.
 */
#define ATERM_TITLE        "Console"
#define ATERM_TITLE_SUFFIX " - " ATERM_TITLE


/**
 * @brief The sixteen ANSI colours, normal then bright, drawn instead of libghostty-vt's own.
 */
static const uint8_t aterm_palette[16][3] = {
    {0x24, 0x1F, 0x31},
    {0xC0, 0x1C, 0x28},
    {0x2E, 0xC2, 0x7E},
    {0xF5, 0xC2, 0x11},
    {0x1E, 0x78, 0xE4},
    {0x98, 0x41, 0xBB},
    {0x0A, 0xB9, 0xDC},
    {0xC0, 0xBF, 0xBC},
    {0x5E, 0x5C, 0x64},
    {0xED, 0x33, 0x3B},
    {0x57, 0xE3, 0x89},
    {0xF8, 0xE4, 0x5C},
    {0x51, 0xA1, 0xFF},
    {0xC0, 0x61, 0xCB},
    {0x4F, 0xD2, 0xFD},
    {0xF6, 0xF5, 0xF4},
};


/**
 * @brief A face scaled to the cell size, along with the glyph indices it has already been asked for.
 */
typedef struct {

    cairo_scaled_font_t* scaled;

    int32_t glyphs[ATERM_GLYPH_CACHE_MAX];

} aterm_font_t;


typedef struct {

    int x;
    int y;

    bool visible;

    GhosttyCursorStyle style;

} aterm_cursor_t;


typedef struct {

    GhosttyRenderStateColors colors;

    aterm_cursor_t cursor;

} aterm_frame_t;


static struct {

    int masterfd;
    int slavefd;

    pid_t child;

    int sigchld[2];

    GhosttyTerminal term;
    GhosttyRenderState render;
    GhosttyRenderStateRowIterator rows_iter;
    GhosttyRenderStateRowCells cells;
    GhosttyKeyEncoder key_encoder;
    GhosttyKeyEvent key_event;

    struct {
        uint8_t* data;
        size_t size;
        size_t capacity;
    } reply;

    ui_connection_t* conn;
    ui_window_t* win;

    bool redraw;
    aterm_cursor_t cursor;

    unsigned int cols;
    unsigned int rows;

    pthread_mutex_t lock;

    ui_keymap_t* keymap;

    pthread_t thr_ui;

    cairo_surface_t* surface;
    cairo_t* cr;

    struct {

        aterm_font_t regular;
        aterm_font_t bold;

        int width;
        int height;
        int baseline;

    } font;

    struct {
        int fd;
        struct fb_var_screeninfo var;
        struct fb_fix_screeninfo fix;
    } fb;

    struct {
        int fd;
        pthread_t thr;
    } kbd;

    bool windowed;

} context = {0};


static void show_usage(int argc, char** argv) {
    printf("Use: aplus-terminal [options]... [STRING]...\n"
           "Print STRING(s) to standard output.\n\n"
           "   -c, --command               run command on shell\n"
           "   -w, --working-dir           set current working directory\n"
           "   -f, --framebuffer           use framebuffer instead of window\n"
           "   -h, --help                  show this help\n"
           "   -v, --version               print version info and exit\n");

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
 * @brief Returns the width of the window's pixel buffer.
 *
 * @return The width in pixels.
 */
size_t aterm_window_width(void) {
    if (context.windowed) {
        return ui_window_width(context.win);
    } else {
        return context.fb.var.xres;
    }
}


/**
 * @brief Returns the height of the window's pixel buffer.
 *
 * @return The height in pixels.
 */
size_t aterm_window_height(void) {
    if (context.windowed) {
        return ui_window_height(context.win);
    } else {
        return context.fb.var.yres;
    }
}


/**
 * @brief Returns a pointer to the window's pixel buffer.
 *
 * @return A pointer to the pixel buffer.
 */
void* aterm_window_pixels(void) {
    if (context.windowed) {
        return ui_window_pixels(context.win);
    } else {
        return (void*)(uintptr_t)context.fb.fix.smem_start;
    }
}


/**
 * @brief Returns the stride of the window's pixel buffer.
 *
 * @return The stride in bytes.
 */
size_t aterm_window_stride(void) {
    if (context.windowed) {
        return ui_window_stride(context.win);
    } else {
        return context.fb.fix.line_length;
    }
}


/**
 * @brief Returns the Cairo format of the window's pixel buffer.
 *
 * @return The Cairo format.
 */
cairo_format_t aterm_window_cairo_format() {
    if (context.windowed) {
        return ui_window_translucent(context.win) ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
    } else {
        switch (context.fb.var.bits_per_pixel) {
            case 16:
                return CAIRO_FORMAT_RGB16_565;
            case 24:
                return CAIRO_FORMAT_INVALID;
            case 32:
                return CAIRO_FORMAT_RGB24;
            default:
                return CAIRO_FORMAT_INVALID;
        }
    }
}

/**
 * @brief Damages the whole window, so it will be repainted on the next commit.
 */
void aterm_window_damage_all(void) {
    if (context.windowed) {
        ui_window_damage_all(context.win);
    }
}


/**
 * @brief Damages a rectangle of the window, so it will be repainted on the next commit.
 *
 * @param x The x coordinate of the rectangle.
 * @param y The y coordinate of the rectangle.
 * @param width The width of the rectangle.
 * @param height The height of the rectangle.
 */
void aterm_window_damage(int x, int y, int width, int height) {
    if (context.windowed) {
        ui_window_damage(context.win, x, y, width, height);
    }
}


/**
 * @brief Commits the window's pixel buffer to the screen.
 */
int aterm_window_commit(void) {
    if (context.windowed) {
        return ui_window_commit(context.win);
    } else {
        struct fb_rect damage;
        damage.x = 0;
        damage.y = 0;
        damage.width = (uint32_t)aterm_window_width();
        damage.height = (uint32_t)aterm_window_height();
        if (ioctl(context.fb.fd, FBIO_FLUSH, &damage) < 0) {
            return -1;
        }
        uint32_t crtc = 0;
        return ioctl(context.fb.fd, FBIO_WAITFORVSYNC, &crtc);
    }
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


    const int width  = aterm_window_width();
    const int height = aterm_window_height();

    unsigned char* pixels = (unsigned char*)aterm_window_pixels();

    if (!pixels || width <= 0 || height <= 0) {
        errno = EINVAL;
        return -1;
    }


    const cairo_format_t format = aterm_window_cairo_format();

    context.surface = cairo_image_surface_create_for_data(pixels, format, width, height, (int)aterm_window_stride());

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
 * @brief Loads the face a theme names, or the configured one when it names none or that one cannot be loaded.
 *
 * @param path The path the theme names, or NULL.
 * @param fallback The configured path.
 * @return The face, or NULL when neither loads.
 */
static cairo_font_face_t* aterm_font_face(const char* path, const char* fallback) {

    cairo_font_face_t* face = path ? ui_font_face(path) : NULL;

    return face ? face : ui_font_face(fallback);
}


/**
 * @brief Loads the theme's monospaced faces at its size and takes the cell size from their metrics, keeping the faces in use on failure.
 *
 * @return 1 when the faces changed, 0 when they were already in use, or -1 with errno set.
 */
static int aterm_font_init(void) {

    const ui_theme_t* theme = ui_theme_system();

    const double size = theme->font_mono_size > 0.0 ? theme->font_mono_size : CONFIG_ATERM_FONT_SIZE;

    cairo_font_face_t* regular = aterm_font_face(theme->font_mono, CONFIG_ATERM_FONT_PATH);

    if (!regular) {
        errno = ENOENT;
        return -1;
    }

    cairo_font_face_t* bold = aterm_font_face(theme->font_mono_bold, CONFIG_ATERM_FONT_BOLD_PATH);

    if (!bold) {
        bold = regular;
    }


    cairo_scaled_font_t* regular_scaled = ui_font_scaled(regular, size);
    cairo_scaled_font_t* bold_scaled    = ui_font_scaled(bold, size);

    if (!regular_scaled || !bold_scaled) {
        errno = ENOMEM;
        return -1;
    }

    if (regular_scaled == context.font.regular.scaled && bold_scaled == context.font.bold.scaled) {
        return 0;
    }

    if (context.font.regular.scaled) {
        cairo_scaled_font_destroy(context.font.regular.scaled);
    }

    if (context.font.bold.scaled) {
        cairo_scaled_font_destroy(context.font.bold.scaled);
    }

    context.font.regular.scaled = cairo_scaled_font_reference(regular_scaled);
    context.font.bold.scaled    = cairo_scaled_font_reference(bold_scaled);


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

    return 1;
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


static GhosttyColorRgb aterm_rgb(ui_color_t color) {

    return (GhosttyColorRgb){
        .r = (uint8_t)lround(color.r * 255.0),
        .g = (uint8_t)lround(color.g * 255.0),
        .b = (uint8_t)lround(color.b * 255.0),
    };
}


static void aterm_set_source(cairo_t* cr, GhosttyColorRgb color) {
    cairo_set_source_rgb(cr, color.r / 255.0, color.g / 255.0, color.b / 255.0);
}


static void aterm_apply_colors(void) {

    const GhosttyColorRgb fg = aterm_rgb(ui_theme_dark()->text);
    const GhosttyColorRgb bg = aterm_rgb(ui_theme_dark()->surface_sunken);

    if (ghostty_terminal_set(context.term, GHOSTTY_TERMINAL_OPT_COLOR_FOREGROUND, &fg) != GHOSTTY_SUCCESS) {
        ulog_warnf("cannot set the default foreground");
    }

    if (ghostty_terminal_set(context.term, GHOSTTY_TERMINAL_OPT_COLOR_BACKGROUND, &bg) != GHOSTTY_SUCCESS) {
        ulog_warnf("cannot set the default background");
    }

    context.redraw = true;
}


static void aterm_apply_palette(void) {

    GhosttyColorRgb palette[256];

    if (ghostty_terminal_get(context.term, GHOSTTY_TERMINAL_DATA_COLOR_PALETTE_DEFAULT, &palette) != GHOSTTY_SUCCESS) {
        ulog_warnf("cannot read the default palette");
        return;
    }

    for (size_t i = 0; i < 16; i++) {
        palette[i] = (GhosttyColorRgb){.r = aterm_palette[i][0], .g = aterm_palette[i][1], .b = aterm_palette[i][2]};
    }

    if (ghostty_terminal_set(context.term, GHOSTTY_TERMINAL_OPT_COLOR_PALETTE, &palette) != GHOSTTY_SUCCESS) {
        ulog_warnf("cannot set the palette");
    }
}


/**
 * @brief Paints the whole surface the default background, padding included.
 */
static void aterm_clear(GhosttyColorRgb bg) {

    cairo_save(context.cr);

    cairo_set_operator(context.cr, CAIRO_OPERATOR_SOURCE);
    aterm_set_source(context.cr, bg);
    cairo_paint(context.cr);

    cairo_restore(context.cr);

    aterm_window_damage_all();
}


/**
 * @brief Draws one cell: its background, the glyph standing in it, and the underline under that.
 *
 * Clipped to the cell, because only the rows libghostty-vt reports as changed are repainted and a
 * glyph reaching past its own advance would otherwise leave a piece of itself in a neighbour
 * that is never drawn again.
 */
static void aterm_draw_cell(const aterm_frame_t* frame, int col, int row) {

    GhosttyCell raw      = 0;
    GhosttyCellWide wide = GHOSTTY_CELL_WIDE_NARROW;

    if (ghostty_render_state_row_cells_get(context.cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_RAW, &raw) == GHOSTTY_SUCCESS) {
        ghostty_cell_get(raw, GHOSTTY_CELL_DATA_WIDE, &wide);
    }

    if (wide == GHOSTTY_CELL_WIDE_SPACER_TAIL) {
        return;
    }


    const int x = ATERM_PADDING + col * context.font.width;
    const int y = ATERM_PADDING + row * context.font.height;

    const int window_width  = (int)aterm_window_width();
    const int window_height = (int)aterm_window_height();

    int width = wide == GHOSTTY_CELL_WIDE_WIDE ? 2 * context.font.width : context.font.width;

    if (x + width > window_width) {
        width = context.font.width;
    }

    if (x + width > window_width || y + context.font.height > window_height) {
        return;
    }


    GhosttyStyle style;
    ghostty_style_default(&style);
    ghostty_render_state_row_cells_get(context.cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_STYLE, &style);

    GhosttyColorRgb front = frame->colors.foreground;
    GhosttyColorRgb back  = frame->colors.background;
    GhosttyColorRgb color;

    if (ghostty_render_state_row_cells_get(context.cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_FG_COLOR, &color) == GHOSTTY_SUCCESS) {
        front = color;
    }

    if (ghostty_render_state_row_cells_get(context.cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR, &color) == GHOSTTY_SUCCESS) {
        back = color;
    }

    if (style.flags & GHOSTTY_STYLE_FLAG_INVERSE) {
        color = front;
        front = back;
        back  = color;
    }


    const aterm_cursor_t* cursor = &frame->cursor;

    const bool on_cursor = cursor->visible && cursor->x == col && cursor->y == row;

    if (on_cursor && cursor->style == GHOSTTY_CURSOR_STYLE_BLOCK) {

        if (frame->colors.cursor_has_value) {
            back  = frame->colors.cursor;
            front = frame->colors.background;
        } else {
            color = front;
            front = back;
            back  = color;
        }
    }


    cairo_t* cr = context.cr;

    cairo_save(cr);

    cairo_rectangle(cr, x, y, width, context.font.height);
    cairo_clip(cr);

    aterm_set_source(cr, back);
    cairo_paint(cr);

    aterm_set_source(cr, front);


    uint32_t codepoints[4];
    GhosttyCodepointBuffer graphemes = {.ptr = codepoints, .cap = 4, .len = 0};

    const GhosttyResult result = ghostty_render_state_row_cells_get(context.cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_BUF, &graphemes);

    const uint32_t codepoint = (result == GHOSTTY_SUCCESS || result == GHOSTTY_OUT_OF_SPACE) && graphemes.len > 0 ? codepoints[0] : 0;

    if (codepoint > ' ' && !(style.flags & GHOSTTY_STYLE_FLAG_INVISIBLE)) {

        aterm_font_t* font = (style.flags & GHOSTTY_STYLE_FLAG_BOLD) ? &context.font.bold : &context.font.regular;

        const unsigned long index = aterm_glyph_index(font, codepoint);

        if (index) {

            cairo_glyph_t glyph = {.index = index, .x = (double)x, .y = (double)(y + context.font.baseline)};

            cairo_set_scaled_font(cr, font->scaled);
            cairo_show_glyphs(cr, &glyph, 1);
        }
    }

    if (GHOSTTY_STYLE_UNDERLINE(style) != 0) {
        cairo_rectangle(cr, x, y + context.font.baseline + 1, width, 1);
        cairo_fill(cr);
    }

    if (style.flags & GHOSTTY_STYLE_FLAG_STRIKETHROUGH) {
        cairo_rectangle(cr, x, y + context.font.height / 2, width, 1);
        cairo_fill(cr);
    }


    if (on_cursor && cursor->style != GHOSTTY_CURSOR_STYLE_BLOCK) {

        if (frame->colors.cursor_has_value) {
            aterm_set_source(cr, frame->colors.cursor);
        }

        switch (cursor->style) {

            case GHOSTTY_CURSOR_STYLE_BAR:
                cairo_rectangle(cr, x, y, 2, context.font.height);
                cairo_fill(cr);
                break;

            case GHOSTTY_CURSOR_STYLE_UNDERLINE:
                cairo_rectangle(cr, x, y + context.font.height - 2, width, 2);
                cairo_fill(cr);
                break;

            default:
                cairo_set_line_width(cr, 1.0);
                cairo_rectangle(cr, x + 0.5, y + 0.5, width - 1, context.font.height - 1);
                cairo_stroke(cr);
                break;
        }
    }

    cairo_restore(cr);


    aterm_window_damage(x, y, width, context.font.height);
}


static void aterm_draw_row(const aterm_frame_t* frame, int row) {

    if (ghostty_render_state_row_get(context.rows_iter, GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &context.cells) != GHOSTTY_SUCCESS) {
        return;
    }

    for (int col = 0; ghostty_render_state_row_cells_next(context.cells); col++) {
        aterm_draw_cell(frame, col, row);
    }
}


/**
 * @brief Tells the shell on the other side of the pty how big the terminal is now.
 *
 * @param width The drawing area width in pixels.
 * @param height The drawing area height in pixels.
 */
static void aterm_pty_winsize(unsigned int width, unsigned int height) {

    if (context.masterfd <= 0) {
        return;
    }

    struct winsize ws = {

        .ws_col    = (unsigned short)context.cols,
        .ws_row    = (unsigned short)context.rows,
        .ws_xpixel = (unsigned short)width,
        .ws_ypixel = (unsigned short)height,
    };

    if (ioctl(context.masterfd, TIOCSWINSZ, &ws) < 0) {
        ulog_warnf("ioctl(TIOCSWINSZ) failed: %s", strerror(errno));
    }
}


/**
 * @brief Fits as many cells as a drawing area holds and resizes the screen to match, with context.lock held.
 *
 * @param width The drawing area width in pixels.
 * @param height The drawing area height in pixels.
 */
static void aterm_fit(unsigned int width, unsigned int height) {

    const unsigned int inner_width  = width > 2 * ATERM_PADDING ? width - 2 * ATERM_PADDING : 0;
    const unsigned int inner_height = height > 2 * ATERM_PADDING ? height - 2 * ATERM_PADDING : 0;

    context.cols = inner_width / (unsigned int)context.font.width;
    context.rows = inner_height / (unsigned int)context.font.height;

    if (context.cols < 1) {
        context.cols = 1;
    }

    if (context.rows < 1) {
        context.rows = 1;
    }

    if (ghostty_terminal_resize(context.term, (uint16_t)context.cols, (uint16_t)context.rows, (uint32_t)context.font.width, (uint32_t)context.font.height) != GHOSTTY_SUCCESS) {
        ulog_warnf("cannot resize the screen to %ux%u", context.cols, context.rows);
    }

    context.redraw = true;
}


/**
 * @brief Follows the framebuffer into the mode it switched to, with context.lock held.
 *
 * @return 0 on success, or -1 with errno set.
 */
static int aterm_fb_resize(void) {

    if (ioctl(context.fb.fd, FBIOGET_VSCREENINFO, &context.fb.var) < 0) {
        return -1;
    }

    if (ioctl(context.fb.fd, FBIOGET_FSCREENINFO, &context.fb.fix) < 0) {
        return -1;
    }

    if (aterm_bind_surface() < 0) {
        return -1;
    }

    const unsigned int width  = (unsigned int)aterm_window_width();
    const unsigned int height = (unsigned int)aterm_window_height();

    aterm_fit(width, height);

    ulog_infof("framebuffer is now %ux%ux%u, %ux%u cells", width, height, context.fb.var.bits_per_pixel, context.cols, context.rows);

    aterm_pty_winsize(width, height);

    return 0;
}


static bool aterm_paint_locked(void) {

    if (ghostty_render_state_update(context.render, context.term) != GHOSTTY_SUCCESS) {
        ulog_errorf("cannot update the render state");
        return false;
    }


    GhosttyRenderStateDirty dirty = GHOSTTY_RENDER_STATE_DIRTY_FULL;
    ghostty_render_state_get(context.render, GHOSTTY_RENDER_STATE_DATA_DIRTY, &dirty);

    aterm_frame_t frame = {
        .colors = GHOSTTY_INIT_SIZED(GhosttyRenderStateColors),
        .cursor = {.x = -1, .y = -1},
    };

    if (ghostty_render_state_get(context.render, GHOSTTY_RENDER_STATE_DATA_COLORS, &frame.colors) != GHOSTTY_SUCCESS) {
        ulog_errorf("cannot read the render state colours");
        return false;
    }

    GhosttyRenderStateCursor cursor = GHOSTTY_INIT_SIZED(GhosttyRenderStateCursor);

    if (ghostty_render_state_get(context.render, GHOSTTY_RENDER_STATE_DATA_CURSOR, &cursor) == GHOSTTY_SUCCESS && cursor.visible && cursor.viewport_has_value) {
        frame.cursor.x       = cursor.wide_tail && cursor.viewport_x > 0 ? cursor.viewport_x - 1 : cursor.viewport_x;
        frame.cursor.y       = cursor.viewport_y;
        frame.cursor.visible = true;
        frame.cursor.style   = cursor.visual_style;
    }

    const bool cursor_moved = frame.cursor.x != context.cursor.x || frame.cursor.y != context.cursor.y || frame.cursor.visible != context.cursor.visible || frame.cursor.style != context.cursor.style;

    const bool full = context.redraw || dirty == GHOSTTY_RENDER_STATE_DIRTY_FULL;

    if (!full && dirty == GHOSTTY_RENDER_STATE_DIRTY_FALSE && !cursor_moved) {
        return false;
    }


    if (full) {
        aterm_clear(frame.colors.background);
    }

    if (ghostty_render_state_get(context.render, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &context.rows_iter) != GHOSTTY_SUCCESS) {
        ulog_errorf("cannot read the render state rows");
        return false;
    }

    for (int row = 0; ghostty_render_state_row_iterator_next(context.rows_iter); row++) {

        bool row_dirty = false;
        ghostty_render_state_row_get(context.rows_iter, GHOSTTY_RENDER_STATE_ROW_DATA_DIRTY, &row_dirty);

        if (full || row_dirty || (cursor_moved && (row == context.cursor.y || row == frame.cursor.y))) {
            aterm_draw_row(&frame, row);
        }
    }

    ghostty_render_state_clean(context.render);

    context.cursor = frame.cursor;
    context.redraw = false;

    return true;
}


/**
 * @brief Draws the screen with context.lock held, which guards the whole of libghostty-vt rather than the drawing alone.
 *
 * Never held across a write() to the pty master, which can block when the shell is slow to read.
 */
static void aterm_draw_locked(void) {

    if (!aterm_paint_locked()) {
        return;
    }

    while (aterm_window_commit() < 0) {

        if (context.windowed || errno != ESTALE) {
            ulog_errorf("aterm_window_commit() failed: %s", strerror(errno));
            return;
        }

        if (aterm_fb_resize() < 0) {
            ulog_errorf("cannot follow the framebuffer mode change: %s", strerror(errno));
            return;
        }

        context.redraw = true;

        aterm_paint_locked();
    }
}


static void aterm_write(const void* data, size_t size) {

    if (context.masterfd <= 0) {
        return;
    }

    const uint8_t* bytes = data;

    while (size > 0) {

        const ssize_t written = write(context.masterfd, bytes, size);

        if (written < 0) {

            if (errno == EINTR) {
                continue;
            }

            ulog_errorf("write(): %s", strerror(errno));
            return;
        }

        bytes += written;
        size -= (size_t)written;
    }
}


static void aterm_reply_flush(void) {

    pthread_mutex_lock(&context.lock);

    uint8_t* data = context.reply.data;
    size_t size   = context.reply.size;

    context.reply.data     = NULL;
    context.reply.size     = 0;
    context.reply.capacity = 0;

    pthread_mutex_unlock(&context.lock);


    if (data) {
        aterm_write(data, size);
        free(data);
    }
}


static void aterm_update_screen(void) {

    pthread_mutex_lock(&context.lock);

    aterm_draw_locked();

    pthread_mutex_unlock(&context.lock);

    aterm_reply_flush();
}


/**
 * @brief Makes the terminal, the pty and the shell agree on what the window's size means in character cells.
 *
 * @param width The window width in pixels.
 * @param height The window height in pixels.
 */
static void aterm_resize(unsigned int width, unsigned int height) {

    if (!context.windowed) {
        return;
    }


    pthread_mutex_lock(&context.lock);

    if (ui_window_apply_configure(context.win) < 0) {
        ulog_errorf("ui_window_apply_configure() failed: %s", strerror(errno));
        pthread_mutex_unlock(&context.lock);
        return;
    }

    if (aterm_bind_surface() < 0) {
        ulog_errorf("cannot draw on the window surface: %s", strerror(errno));
        pthread_mutex_unlock(&context.lock);
        return;
    }

    aterm_fit(width, height);

    aterm_draw_locked();

    pthread_mutex_unlock(&context.lock);


    aterm_reply_flush();
    aterm_pty_winsize(width, height);
}


/**
 * @brief Adopts the theme the server sent and, when it names other monospaced faces, refits the cells to the window.
 */
static void aterm_restyle(void) {

    ui_theme_update(context.conn);


    pthread_mutex_lock(&context.lock);

    aterm_apply_colors();

    const int changed = aterm_font_init();

    if (changed <= 0) {

        if (changed < 0) {
            ulog_warnf("keeping the faces in use: %s", strerror(errno));
        }

        aterm_draw_locked();

        pthread_mutex_unlock(&context.lock);
        return;
    }

    const unsigned int width  = (unsigned int)aterm_window_width();
    const unsigned int height = (unsigned int)aterm_window_height();

    aterm_fit(width, height);

    aterm_draw_locked();

    pthread_mutex_unlock(&context.lock);


    aterm_reply_flush();
    aterm_pty_winsize(width, height);
}


/**
 * @brief Names the window after a title the shell sets, followed by the terminal's own name.
 * @param title The UTF-8 title; an empty one names the window after the terminal alone.
 */
static void aterm_set_title(const char* title, size_t len) {

    if (!context.windowed) {
        return;
    }

    char buf[UI_TITLE_MAX];

    size_t max = sizeof(buf) - sizeof(ATERM_TITLE_SUFFIX);

    if (len > max) {
        len = max;
        while (len > 0 && ((unsigned char)title[len] & 0xC0) == 0x80) {
            len--;
        }
    }

    if (len == 0) {
        ui_window_set_title(context.win, ATERM_TITLE);
        return;
    }

    snprintf(buf, sizeof(buf), "%.*s" ATERM_TITLE_SUFFIX, (int)len, title);
    ui_window_set_title(context.win, buf);
}


static GhosttyKey aterm_ghostty_key(uint16_t key) {

    switch (KTYP(key)) {

        case KT_CUR:

            switch (key) {
                case K_UP:
                    return GHOSTTY_KEY_ARROW_UP;
                case K_DOWN:
                    return GHOSTTY_KEY_ARROW_DOWN;
                case K_RIGHT:
                    return GHOSTTY_KEY_ARROW_RIGHT;
                case K_LEFT:
                    return GHOSTTY_KEY_ARROW_LEFT;
            }

            break;

        case KT_FN:

            if (KVAL(key) < 12) {
                return (GhosttyKey)(GHOSTTY_KEY_F1 + KVAL(key));
            }

            switch (key) {
                case K_FIND:
                    return GHOSTTY_KEY_HOME;
                case K_SELECT:
                    return GHOSTTY_KEY_END;
                case K_INSERT:
                    return GHOSTTY_KEY_INSERT;
                case K_REMOVE:
                    return GHOSTTY_KEY_DELETE;
                case K_PGUP:
                    return GHOSTTY_KEY_PAGE_UP;
                case K_PGDN:
                    return GHOSTTY_KEY_PAGE_DOWN;
            }

            break;
    }

    return GHOSTTY_KEY_UNIDENTIFIED;
}


static GhosttyMods aterm_ghostty_mods(uint8_t modifiers) {

    GhosttyMods mods = 0;

    if (modifiers & ((1 << KG_SHIFT) | (1 << KG_SHIFTL) | (1 << KG_SHIFTR))) {
        mods |= GHOSTTY_MODS_SHIFT;
    }

    if (modifiers & ((1 << KG_CTRL) | (1 << KG_CTRLL) | (1 << KG_CTRLR))) {
        mods |= GHOSTTY_MODS_CTRL;
    }

    if (modifiers & (1 << KG_ALT)) {
        mods |= GHOSTTY_MODS_ALT;
    }

    return mods;
}


static size_t aterm_encode_key(uint16_t key, char* out, size_t size) {

    if (key == K_ENTER || key == K_PENTER) {
        out[0] = '\n';
        return 1;
    }

    const GhosttyKey ghostty_key = aterm_ghostty_key(key);

    if (ghostty_key == GHOSTTY_KEY_UNIDENTIFIED) {
        return 0;
    }


    size_t written = 0;

    pthread_mutex_lock(&context.lock);

    ghostty_key_encoder_set_from_terminal(context.key_encoder, context.term);

    ghostty_key_event_set_action(context.key_event, GHOSTTY_KEY_ACTION_PRESS);
    ghostty_key_event_set_key(context.key_event, ghostty_key);
    ghostty_key_event_set_mods(context.key_event, aterm_ghostty_mods(ui_keymap_modifiers(context.keymap)));

    const GhosttyResult result = ghostty_key_encoder_encode(context.key_encoder, context.key_event, (uint8_t*)out, size, &written);

    pthread_mutex_unlock(&context.lock);

    return result == GHOSTTY_SUCCESS ? written : 0;
}


/**
 * @brief Sends what a key event stands for to the shell.
 *
 * @param keysym The key code the display server reported.
 * @param down Whether the key went down or came up.
 */
static void aterm_handle_key(vkey_t keysym, uint8_t down) {

    const uint16_t key = ui_keymap_lookup(context.keymap, keysym);

    char produced[32];

    size_t size = ui_keymap_translate(context.keymap, keysym, down, produced, sizeof(produced));

    if (size == 0 && down) {
        size = aterm_encode_key(key, produced, sizeof(produced));
    }

    if (size == 0) {
        return;
    }


    const GhosttyTerminalScrollViewport bottom = {.tag = GHOSTTY_TERMINAL_SCROLL_VIEWPORT_BOTTOM};

    pthread_mutex_lock(&context.lock);
    ghostty_terminal_scroll_viewport(context.term, bottom);
    pthread_mutex_unlock(&context.lock);

    aterm_write(produced, size);
}


/**
 * @brief Hangs up on the shell's process group and ends the terminal, which closes its window.
 */
__attribute__((noreturn)) static void aterm_quit(void) {

    if (context.child > 0) {

        if (kill(-context.child, SIGHUP) == 0) {
            usleep(150000);
        } else if (errno != ESRCH) {
            ulog_warnf("cannot hang up the session: %s", strerror(errno));
        }
    }

    exit(0);
}


/**
 * @brief Wakes the main loop through the SIGCHLD pipe, whichever thread the signal landed on.
 *
 * @param signum The signal, SIGCHLD.
 */
static void aterm_on_sigchld(int signum) {

    (void)signum;

    const int saved = errno;
    const char byte = 0;

    (void)write(context.sigchld[1], &byte, 1);

    errno = saved;
}


/**
 * @brief Opens the SIGCHLD pipe and installs the handler that feeds it.
 *
 * @return 0 on success, or -1 with errno set.
 */
static int aterm_watch_child(void) {

    if (pipe2(context.sigchld, O_CLOEXEC | O_NONBLOCK) < 0) {
        return -1;
    }

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = aterm_on_sigchld;
    sa.sa_flags   = SA_RESTART | SA_NOCLDSTOP;

    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        return -1;
    }

    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);

    return sigprocmask(SIG_UNBLOCK, &set, NULL);
}


/**
 * @brief Opens the framebuffer device and queries its fixed and variable information.
 *
 * @return 0 on success, or -1 with errno set.
 */
int aterm_fb_connect(void) {

    if (context.windowed) {
        errno = EINVAL;
        return -1;
    }

    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        return -1;
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &context.fb.var) < 0) {
        close(fd);
        return -1;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &context.fb.fix) < 0) {
        close(fd);
        return -1;
    }

    ulog_infof("framebuffer %dx%dx%d [ptr(%p), size(%p)]", context.fb.var.xres, context.fb.var.yres, context.fb.var.bits_per_pixel, (void*)((uintptr_t)context.fb.fix.smem_start), (void*)((uintptr_t)context.fb.fix.smem_len));

    context.fb.fd = fd;
    return 0;
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

            ulog_errorf("lost the connection to the display server: %s", strerror(errno));
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

                aterm_handle_key(ev.key.vkey, ev.key.down);
                aterm_update_screen();

                break;

            case UI_EVENT_CONFIGURE:
                aterm_resize(ev.configure.width, ev.configure.height);
                break;

            case UI_EVENT_THEME:
                aterm_restyle();
                break;

            case UI_EVENT_CLOSE:
                aterm_quit();

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


static void* thr_kbd_handler(void* arg) {

    (void)arg;

    do {

        event_t ev = { 0 };

        do {

            ssize_t e = read(context.kbd.fd, &ev, sizeof(ev));

            if (e < 0) {

                if (errno == EINTR) {
                    continue;
                }

                ulog_errorf("cannot read from the keyboard: %s", strerror(errno));
                exit(1);
            }

            else if(e > 0) {

                switch(ev.ev_type) {
                    case EV_KEY:
                        aterm_handle_key(ev.ev_key.vkey, ev.ev_key.down);
                        aterm_update_screen();
                        break;

                    default:
                        break;
                }
            }

        } while(errno == EINTR);

    } while(true);

    return NULL;
}


static void aterm_on_write_pty(GhosttyTerminal terminal, void* userdata, const uint8_t* data, size_t len) {

    (void)terminal;
    (void)userdata;

    if (context.reply.size + len > context.reply.capacity) {

        size_t capacity = context.reply.capacity ? context.reply.capacity : 64;

        while (capacity < context.reply.size + len) {
            capacity *= 2;
        }

        uint8_t* grown = realloc(context.reply.data, capacity);

        if (!grown) {
            ulog_warnf("dropping a %zu byte reply to the shell", len);
            return;
        }

        context.reply.data     = grown;
        context.reply.capacity = capacity;
    }

    memcpy(context.reply.data + context.reply.size, data, len);

    context.reply.size += len;
}


static void aterm_on_title_changed(GhosttyTerminal terminal, void* userdata) {

    (void)userdata;

    GhosttyString title = {0};

    if (ghostty_terminal_get(terminal, GHOSTTY_TERMINAL_DATA_TITLE, &title) != GHOSTTY_SUCCESS) {
        return;
    }

    aterm_set_title((const char*)title.ptr, title.len);
}


int main(int argc, char** argv) {

    static struct option long_options[] = {
        {"command",     required_argument, NULL, 'c'},
        {"working-dir", required_argument, NULL, 'w'},
        {"framebuffer", no_argument,       NULL, 'f'},
        {"help",        no_argument,       NULL, 'h'},
        {"version",     no_argument,       NULL, 'v'},
        {NULL,          0,                 NULL, 0  }
    };

    char* cmd = NULL;
    char* pwd = NULL;
    bool windowed = true;

    int c, idx;
    while ((c = getopt_long(argc, argv, "c:w:fvh", long_options, &idx)) != -1) {
        switch (c) {
            case 'c':
                cmd = strdup(optarg);
                break;
            case 'w':
                pwd = strdup(optarg);
                break;
            case 'f':
                windowed = false;
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
        ulog_errorf("pthread_mutex_init() failed: %s", strerror(errno));
        exit(1);
    }

    context.windowed = windowed;
    ulog_infof("running in %s mode", context.windowed ? "windowed" : "framebuffer");

    if (context.windowed) {
        if ((context.conn = ui_connect(NULL, 5000)) == NULL) {
            ulog_errorf("cannot reach the display server on %s: %s", UI_DEFAULT_SOCKET, strerror(errno));
            exit(1);
        }
    } else {
        if (aterm_fb_connect() < 0) {
            ulog_errorf("cannot open the framebuffer: %s", strerror(errno));
            exit(1);
        }
    }


    //* Font initialization
    if (aterm_font_init() < 0) {
        ulog_errorf("cannot load %s: %s", CONFIG_ATERM_FONT_PATH, strerror(errno));
        exit(1);
    }

    //* Window initialization
    if (context.windowed) {
        if ((context.win = ui_window_create(context.conn, cols * context.font.width + 2 * ATERM_PADDING, rows * context.font.height + 2 * ATERM_PADDING, ATERM_TITLE)) == NULL) {
            ulog_errorf("ui_window_create() failed: %s", strerror(errno));
            exit(1);
        }
    }

    if (aterm_bind_surface() < 0) {
        ulog_errorf("cannot draw on the window surface: %s", strerror(errno));
        exit(1);
    }

    context.cols = (unsigned int)((aterm_window_width() - 2 * ATERM_PADDING) / context.font.width);
    context.rows = (unsigned int)((aterm_window_height() - 2 * ATERM_PADDING) / context.font.height);

    if (context.cols < 1) {
        context.cols = 1;
    }

    if (context.rows < 1) {
        context.rows = 1;
    }


    //* 3. Terminal initialization
    if (ghostty_terminal_new(NULL, &context.term, (uint16_t)context.cols, (uint16_t)context.rows) != GHOSTTY_SUCCESS) {
        ulog_errorf("ghostty_terminal_new() failed");
        exit(1);
    }

    if (ghostty_terminal_resize(context.term, (uint16_t)context.cols, (uint16_t)context.rows, (uint32_t)context.font.width, (uint32_t)context.font.height) != GHOSTTY_SUCCESS) {
        ulog_errorf("cannot resize the screen to %ux%u", context.cols, context.rows);
        exit(1);
    }

    if (ghostty_render_state_new(NULL, &context.render) != GHOSTTY_SUCCESS || ghostty_render_state_row_iterator_new(NULL, &context.rows_iter) != GHOSTTY_SUCCESS ||
        ghostty_render_state_row_cells_new(NULL, &context.cells) != GHOSTTY_SUCCESS) {
        ulog_errorf("cannot create the render state");
        exit(1);
    }

    if (ghostty_key_encoder_new(NULL, &context.key_encoder) != GHOSTTY_SUCCESS || ghostty_key_event_new(NULL, &context.key_event) != GHOSTTY_SUCCESS) {
        ulog_errorf("cannot create the key encoder");
        exit(1);
    }

    ghostty_terminal_set(context.term, GHOSTTY_TERMINAL_OPT_WRITE_PTY, (const void*)aterm_on_write_pty);
    ghostty_terminal_set(context.term, GHOSTTY_TERMINAL_OPT_TITLE_CHANGED, (const void*)aterm_on_title_changed);

    aterm_apply_palette();
    aterm_apply_colors();


    // Set Line Mode LNM
    const GhosttyTerminalModeConfig lnm = {.mode = GHOSTTY_MODE_LINEFEED, .value = true};

    if (ghostty_terminal_set(context.term, GHOSTTY_TERMINAL_OPT_MODE_DEFAULT, &lnm) != GHOSTTY_SUCCESS) {
        ulog_warnf("cannot set the line feed mode");
    }

    context.cursor = (aterm_cursor_t){.x = -1, .y = -1};

    // Draw first frame
    aterm_update_screen();



    //* 2. I/O initialization
    struct winsize ws;

    ws.ws_col    = (unsigned short)context.cols;
    ws.ws_row    = (unsigned short)context.rows;
    ws.ws_xpixel = (unsigned short)aterm_window_width();
    ws.ws_ypixel = (unsigned short)aterm_window_height();

    if (openpty(&context.masterfd, &context.slavefd, NULL, NULL, &ws) < 0) {
        ulog_errorf("cannot open pseudo-terminal: %s", strerror(errno));
        exit(1);
    }

    if (!context.windowed) {

        sigset_t set;
        sigemptyset(&set);
        sigprocmask(SIG_SETMASK, &set, NULL);
        for (int i = 1; i < NSIG; i++)
            signal(i, SIG_DFL);
    }


    //* 4. Input initialization
    if (!context.windowed) {

        context.kbd.fd = open("/dev/kbd", O_RDONLY);
        if (context.kbd.fd < 0) {
            ulog_errorf("cannot open the keyboard: %s", strerror(errno));
            exit(1);
        }

        int thr_e = pthread_create(&context.kbd.thr, NULL, thr_kbd_handler, NULL);
        if (thr_e != 0) {
            ulog_errorf("pthread_create() failed: %s", strerror(errno));
            exit(1);
        }
    }

    //* Load Keymap
    context.keymap = ui_keymap_open(NULL);

    if (!context.keymap) {
        ulog_errorf("cannot load the keymap: %s", strerror(errno));
        exit(1);
    }

    if (aterm_watch_child() < 0) {
        ulog_errorf("cannot watch for the shell to exit: %s", strerror(errno));
        exit(1);
    }

    //* 4. UI initialization
    if (context.windowed) {

        int thr_e = pthread_create(&context.thr_ui, NULL, thr_ui_handler, NULL);
        if (thr_e != 0) {
            ulog_errorf("pthread_create() failed: %s", strerror(thr_e));
            exit(1);
        }
    }


    //* 5. Session initialization
    if (setsid() < 0 && errno != EPERM) {
        ulog_errorf("setsid() failed: %s", strerror(errno));
        exit(1);
    }

    if (ioctl(context.masterfd, TIOCSCTTY, 0) < 0) {
        ulog_warnf("cannot claim the pty as a controlling terminal: %s", strerror(errno));
    }


    //* 5. Child initialization
    pid_t pid = fork();

    if (pid < 0) {

        ulog_errorf("fork() failed");
        exit(1);

    } else if (pid == 0) {

        if (context.windowed) {
            close(ui_connection_fd(context.conn));
        } else {
            close(context.fb.fd);
            close(context.kbd.fd);
        }
        close(context.masterfd);

        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        setenv("TERMINFO", "/usr/share/terminfo", 1);
        setenv("COLORFGBG", "7;0", 1);

        struct passwd* pw = getpwuid(getuid());

        if (pw) {
            setenv("HOME", pw->pw_dir, 0);
            setenv("USER", pw->pw_name, 0);
            setenv("LOGNAME", pw->pw_name, 0);

            if (pw->pw_shell && pw->pw_shell[0] != '\0')
                setenv("SHELL", pw->pw_shell, 0);
        }

        setenv("SHELL", CONFIG_DEFAULT_SHELL, 0);


        pid_t pgrp = getpid();

        if (setpgrp() < 0) {
            ulog_errorf("setpgrp() failed: %s", strerror(errno));
            exit(1);
        }

        if (ioctl(context.slavefd, TIOCSPGRP, &pgrp) < 0) {
            ulog_errorf("ioctl() failed: %s", strerror(errno));
            exit(1);
        }

        if (dup2(context.slavefd, STDIN_FILENO) < 0) {
            ulog_errorf("dup2() failed");
            exit(1);
        }

        if (dup2(context.slavefd, STDOUT_FILENO) < 0) {
            ulog_errorf("dup2() failed");
            exit(1);
        }

        if (dup2(context.slavefd, STDERR_FILENO) < 0) {
            ulog_errorf("dup2() failed");
            exit(1);
        }


        if (pwd) {
            if (chdir(pwd) < 0) {
                ulog_errorf("chdir() failed: %s", strerror(errno));
                exit(1);
            }
        }


        const char* shell = getenv("SHELL");

        if (cmd) {
            execl(shell, shell, "-c", cmd, NULL);
        } else {
            execl(shell, shell, NULL);
        }

        ulog_errorf("cannot run %s: %s", shell, strerror(errno));
        exit(1);
    }


    context.child = pid;

    if (close(context.slavefd) < 0) {
        ulog_errorf("close() failed: %s", strerror(errno));
        exit(1);
    }


    char buf[BUFSIZ];
    bool pending = false;

    for (;;) {

        struct pollfd fds[2] = {
            {.fd = context.masterfd,   .events = POLLIN},
            {.fd = context.sigchld[0], .events = POLLIN},
        };

        const int ready = poll(fds, 2, pending ? 0 : -1);

        if (ready < 0) {

            if (errno == EINTR) {
                continue;
            }

            ulog_errorf("poll() failed: %s", strerror(errno));
            exit(1);
        }

        if (ready == 0) {

            aterm_update_screen();
            pending = false;

            continue;
        }

        if (fds[0].revents & POLLIN) {

            const ssize_t size = read(context.masterfd, buf, sizeof(buf));

            if (size > 0) {

                pthread_mutex_lock(&context.lock);
                ghostty_terminal_vt_write(context.term, (const uint8_t*)buf, (size_t)size);
                pthread_mutex_unlock(&context.lock);

                aterm_reply_flush();

                pending = size == (ssize_t)sizeof(buf);

                if (!pending) {
                    aterm_update_screen();
                }
            }
        }

        if (fds[1].revents & POLLIN) {

            while (read(context.sigchld[0], buf, sizeof(buf)) > 0)
                ;

            if (waitpid(context.child, NULL, WNOHANG) != 0) {
                aterm_quit();
            }
        }
    }
}
