/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
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

#ifndef _APLUS_WM_H
#define _APLUS_WM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cairo/cairo.h>

#include <aplus/fb.h>
#include <aplus/ui-draw.h>
#include <aplus/ui.h>


/**
 * @brief Picking one of two values, and holding one between two others.
 */

#define WM_MIN(a, b)        ((a) < (b) ? (a) : (b))
#define WM_MAX(a, b)        ((a) > (b) ? (a) : (b))
#define WM_CLAMP(v, lo, hi) WM_MIN(WM_MAX((v), (lo)), (hi))

#define WM_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))


/**
 * @brief Decoration geometry: the content area is the client's, everything outside it is the server's.
 */

#define WM_TITLEBAR_HEIGHT 36
#define WM_BORDER_WIDTH    3
#define WM_RESIZE_GRIP     16

#define WM_WINDOW_MIN_WIDTH  80
#define WM_WINDOW_MIN_HEIGHT 40

/**
 * @brief The drop shadow, faked by stacking translucent rounded rectangles.
 *
 * The corner radius they are stacked at is UI_WINDOW_RADIUS, which lives in the shared
 * header because a client painting its own edge has to trace the same curve.
 */
#define WM_SHADOW_EXTENT 18
#define WM_SHADOW_OFFSET 0
#define WM_SHADOW_LAYERS 14
#define WM_SHADOW_ALPHA  0.25


/**
 * @brief The palette, as cairo component lists that drop straight into a set_source call.
 *
 * Every surface is a neutral dark grey a shade apart from the one behind it; the ring marks the active window.
 */

#define WM_COLOR_DESKTOP_TOP    0.086, 0.086, 0.086
#define WM_COLOR_DESKTOP_BOTTOM 0.043, 0.043, 0.043

#define WM_COLOR_FRAME_ACTIVE 0.169, 0.169, 0.169
#define WM_COLOR_FRAME_IDLE   0.125, 0.125, 0.125

#define WM_COLOR_TITLE_ACTIVE 0.910, 0.910, 0.910
#define WM_COLOR_TITLE_IDLE   0.498, 0.498, 0.498

#define WM_COLOR_RING_ACTIVE 1.000, 1.000, 1.000, 0.18
#define WM_COLOR_RING_IDLE   1.000, 1.000, 1.000, 0.07

#define WM_COLOR_CLOSE_OVER 0.839, 0.271, 0.302
#define WM_COLOR_CLOSE_DOWN 0.651, 0.184, 0.212

/**
 * @brief The typeface titles are drawn in, loaded once at startup.
 */

#define WM_FONT_PATH "/usr/share/fonts/ttf/Ubuntu-R.ttf"
#define WM_FONT_SIZE 14.0


/**
 * @brief The cursor theme: webp images with straight alpha, and the largest one the plane will hold.
 */

#define WM_CURSOR_PATH     "/usr/share/cursors"
#define WM_CURSOR_MAX_SIZE 64

/**
 * @brief The desktop picture: one webp file, unlike the cursor theme, which is a directory.
 *
 * The limit is on the image as it comes out of the file, before it is scaled down to the screen.
 */

#define WM_WALLPAPER_PATH     "/usr/share/images/01.webp"
#define WM_WALLPAPER_MAX_SIZE 8192

/**
 * @brief The arrow drawn by hand, for when the theme is missing entirely.
 */

#define WM_CURSOR_WIDTH  10
#define WM_CURSOR_HEIGHT 16

/**
 * @brief The close button, at the right end of the titlebar, hit-tested ahead of the resize grips.
 */

#define WM_BUTTON_SIZE   16
#define WM_BUTTON_MARGIN 9


/**
 * @brief Keyboard modifiers, as a mask over whatever is held right now; left and right fold into one bit.
 */

#define WM_MOD_SHIFT (1 << 0)
#define WM_MOD_CTRL  (1 << 1)
#define WM_MOD_ALT   (1 << 2)
#define WM_MOD_SUPER (1 << 3)


/**
 * @brief The shapes the server puts under the pointer, each naming a file in the cursor theme.
 */

typedef enum {

    WM_CURSOR_ARROW = 0,
    WM_CURSOR_HAND,
    WM_CURSOR_SIZE_ALL,
    WM_CURSOR_SIZE_HOR,
    WM_CURSOR_SIZE_VER,
    WM_CURSOR_SIZE_FDIAG,
    WM_CURSOR_SIZE_BDIAG,

    WM_CURSOR_COUNT,

} wm_cursor_shape_t;


/**
 * @brief The server's rectangle is the library's, so that a damage set can be handed to libui as it stands.
 */
typedef ui_rect_t wm_rect_t;


/**
 * @brief How far the decorations reach out from the content area on each side.
 *
 * Everything that measures a window goes through these rather than the constants, which is
 * what lets a borderless window be the same window with every inset at zero.
 */

typedef struct {

    int left;
    int top;
    int right;
    int bottom;

} wm_insets_t;


typedef struct {

    int fd;

    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;

    int width;
    int height;

    cairo_surface_t* screen;
    cairo_surface_t* back;

    cairo_t* cr;
    cairo_t* cr_screen;

    //? What the desktop is painted with, built once: the wallpaper when one decoded, the
    //? gradient otherwise. It spans the whole screen and never changes, so rebuilding it per
    //? frame only costs allocations.
    cairo_pattern_t* background;

    //? Whether the adapter composites a cursor plane of its own. When it does, the pointer is
    //? not drawn into the frame at all and moving it costs one small ioctl instead of two
    //? damaged rectangles and the repaint and flush they pull in.
    bool hwcursor;

} wm_display_t;


typedef struct wm_client wm_client_t;


typedef struct wm_window {

    uint32_t id;
    wm_client_t* client;

    //? UI_WINDOW_*, as the client asked for at creation. Nothing changes them afterwards:
    //? a window that came up borderless stays borderless.
    uint32_t flags;

    //? Whether the client has ever said that it drew something. A window is created before
    //? its client has been told where to draw, so until the first commit its surface holds
    //? nothing anybody chose, and showing it means a black rectangle for the frame or two
    //? that takes.
    bool committed;

    char title[UI_TITLE_MAX];

    //? Position of the *content* area on screen. The frame is derived from it.
    int x;
    int y;
    int width;
    int height;

    //? Bumped on every resize. A commit stamped with an older serial describes the surface
    //? the window used to have and is dropped rather than read against the current one.
    uint32_t serial;

    //? The window's surface, and the shared memory segment it lives in. The server creates
    //? the segment and hands its id to the client, which maps the same frames and draws
    //? straight into them -- so `backstore` is at once what the client paints and what
    //? compositing reads, with no copy in between and nothing but a damage rectangle
    //? travelling over the socket.
    cairo_surface_t* backstore;

    int shm_id;
    size_t shm_size;
    int stride;

    void* shm_addr;

    //? The drop shadow, rendered once and kept as coverage alone: it is pure black, so an A8
    //? mask carries everything it needs at a quarter of the memory. Valid while the frame size
    //? and the focus state still match the ones it was rendered for, which is what the three
    //? fields below record; NULL when it could not be allocated, in which case the layers are
    //? drawn straight onto the frame instead.
    cairo_surface_t* shadow;

    int shadow_width;
    int shadow_height;
    bool shadow_focused;

    struct wm_window* next;

} wm_window_t;


struct wm_client {

    int fd;

    bool hello;
    bool dead;

    struct {
        uint8_t* data;
        size_t size;
        size_t capacity;
    } rx;

    //? Events are queued rather than written straight out. A blocking write here would
    //? deadlock against a client blocked writing a commit into a full socket buffer.
    struct {
        uint8_t* data;
        size_t head;
        size_t size;
        size_t capacity;
    } tx;

    struct wm_client* next;
};


typedef enum {

    WM_REGION_NONE = 0,
    WM_REGION_CONTENT,
    WM_REGION_TITLEBAR,
    WM_REGION_CLOSE,
    WM_REGION_RESIZE_N,
    WM_REGION_RESIZE_S,
    WM_REGION_RESIZE_E,
    WM_REGION_RESIZE_W,
    WM_REGION_RESIZE_NE,
    WM_REGION_RESIZE_NW,
    WM_REGION_RESIZE_SE,
    WM_REGION_RESIZE_SW,

} wm_region_t;


typedef struct {

    wm_display_t display;

    //? Head is the topmost window; compositing walks the list backwards.
    wm_window_t* windows;
    wm_client_t* clients;

    wm_window_t* focused;

    //? The window whose content area the pointer is over. Tracked only so that the client
    //? can be told when the pointer leaves again: a toolkit highlighting whatever is under
    //? the pointer has no other way to find out, since the stream of UI_EV_POINTER simply
    //? stops at the edge of the content.
    wm_window_t* pointer_focus;

    //? The window whose close button the pointer is over, if any. A button that lights up
    //? has to be repainted when the pointer arrives and again when it leaves, so the
    //? transition is what gets damaged; it doubles as the "still on the button" test that
    //? decides whether releasing there actually closes anything.
    wm_window_t* hovered_close;

    uint32_t next_window_id;

    struct {
        uint16_t modifiers;
    } keyboard;

    struct {
        int x;
        int y;
        uint8_t buttons;
    } pointer;

    struct {
        wm_window_t* window;
        wm_region_t region;
        int grab_x;
        int grab_y;
        wm_rect_t origin;
    } drag;

    //? What has to be repainted before the next frame reaches the screen, as a handful of
    //? rectangles rather than the one box around them all: dragging a window damages where it was
    //? and where it now is, and the box around those two covers the whole sweep between them.
    ui_damage_t damage;

    bool running;

} wm_server_t;


extern wm_server_t wm;


/**
 * @brief Implemented in rect.c.
 */
bool wm_rect_contains_point(const wm_rect_t* rect, int x, int y);
bool wm_rect_contains(const wm_rect_t* outer, const wm_rect_t* inner);
bool wm_rect_intersects(const wm_rect_t* a, const wm_rect_t* b);
wm_rect_t wm_rect_clip(const wm_rect_t* rect, int width, int height);

/**
 * @brief Implemented in image.c.
 */
void* wm_slurp(const char* path, size_t* size);
void* wm_image_probe(const char* path, int max_size, size_t* size, int* width, int* height);

/**
 * @brief Implemented in draw.c.
 */
cairo_surface_t* wm_surface_create(cairo_format_t format, int width, int height);

/**
 * @brief Implemented in display.c.
 */
int wm_display_open(wm_display_t* display, const char* device, const char* wallpaper);
void wm_display_close(wm_display_t* display);
void wm_display_flush(wm_display_t* display, const wm_rect_t* rects, size_t count);
void wm_display_cursor_move(wm_display_t* display, int x, int y);
int wm_display_cursor_image(wm_display_t* display, const uint32_t* image, int width, int height, int hot_x, int hot_y);

/**
 * @brief Implemented in cursor.c.
 */
void wm_cursor_set(wm_cursor_shape_t shape);
int wm_cursor_upload(void);
wm_rect_t wm_cursor_rect(void);
void wm_cursor_paint(cairo_t* cr, double x, double y);
void wm_cursor_fini(void);

/**
 * @brief Implemented in wallpaper.c.
 */
cairo_pattern_t* wm_wallpaper_load(const char* path, int width, int height);
cairo_pattern_t* wm_wallpaper_gradient(int height);

/**
 * @brief Implemented in input.c.
 */
int wm_input_open(void);
void wm_input_close(void);
int wm_input_dispatch(int fd);

/**
 * @brief Implemented in keys.c.
 */
bool wm_keys_handle(uint16_t vkey, uint8_t down);
void wm_keys_reap(void);

/**
 * @brief Implemented in window.c.
 */
wm_window_t* wm_window_create(wm_client_t* client, int width, int height, const char* title, uint32_t flags);
void wm_window_destroy(wm_window_t* win);
void wm_window_request_close(wm_window_t* win);
wm_window_t* wm_window_from_id(uint32_t id);
bool wm_window_borderless(const wm_window_t* win);
bool wm_window_centered(const wm_window_t* win);
bool wm_window_translucent(const wm_window_t* win);
wm_insets_t wm_window_insets(const wm_window_t* win);
cairo_format_t wm_window_format(const wm_window_t* win);
wm_rect_t wm_window_frame(const wm_window_t* win);
wm_rect_t wm_window_shadow_rect(const wm_window_t* win);
wm_rect_t wm_window_close_rect(const wm_window_t* win);
wm_region_t wm_window_hit_test(int x, int y, wm_window_t** out);
void wm_window_raise(wm_window_t* win);
void wm_window_focus(wm_window_t* win);
void wm_window_move(wm_window_t* win, int x, int y);
void wm_window_clamp_size(const wm_window_t* win, int* width, int* height);
int wm_window_resize(wm_window_t* win, int width, int height);
int wm_window_notify_configure(wm_window_t* win);
void wm_window_paint(cairo_t* cr, wm_window_t* win);
int wm_window_damage_content(wm_window_t* win, int x, int y, int width, int height);

/**
 * @brief Implemented in client.c.
 */
wm_client_t* wm_client_accept(int listener);
void wm_client_destroy(wm_client_t* client);
int wm_client_read(wm_client_t* client);
int wm_client_flush(wm_client_t* client);
bool wm_client_wants_write(const wm_client_t* client);
int wm_client_queue(wm_client_t* client, uint16_t type, const void* payload, size_t size);

/**
 * @brief Implemented in main.c.
 */
void wm_damage(const wm_rect_t* rect);
void wm_damage_window(const wm_window_t* win);

#endif
