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
#include <aplus/ui-anim.h>
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
 * @brief How far a new window steps down and right from one already open where it would have gone.
 */
#define WM_CASCADE_STEP 32

/**
 * @brief The drop shadow: a wide soft key light cast downwards, and a tight ambient one hugging the frame.
 *
 * Each is a blurred rounded rectangle rendered once and stretched around any frame size.
 */
#define WM_SHADOW_KEY_BLUR      30
#define WM_SHADOW_KEY_OFFSET    10
#define WM_SHADOW_KEY_ALPHA     0.42
#define WM_SHADOW_AMBIENT_BLUR  6
#define WM_SHADOW_AMBIENT_ALPHA 0.30
#define WM_SHADOW_IDLE          0.55

/**
 * @brief How a window grows into place when it opens and shrinks away when it closes.
 */
#define WM_OPEN_SCALE  0.94
#define WM_CLOSE_SCALE 0.94

/**
 * @brief How close together two presses on a titlebar have to be to maximise the window.
 */
#define WM_DOUBLE_CLICK_MS 400

/**
 * @brief How far a maximised window's titlebar has to be dragged before the window comes loose.
 */
#define WM_UNMAXIMIZE_SLOP 6


/**
 * @brief The palette, as cairo component lists that drop straight into a set_source call.
 *
 * Every surface is a neutral dark grey a shade apart from the one behind it; the ring marks the active window.
 */

#define WM_COLOR_DESKTOP_TOP    0.086, 0.086, 0.086
#define WM_COLOR_DESKTOP_BOTTOM 0.043, 0.043, 0.043

#define WM_COLOR_FRAME_ACTIVE 0.169, 0.169, 0.169
#define WM_COLOR_FRAME_IDLE   0.141, 0.141, 0.141

#define WM_COLOR_TITLE_ACTIVE 0.933, 0.933, 0.933
#define WM_COLOR_TITLE_IDLE   0.540, 0.540, 0.540

#define WM_COLOR_OUTLINE        0.000, 0.000, 0.000, 0.55
#define WM_COLOR_HIGHLIGHT      1.000, 1.000, 1.000, 0.10
#define WM_COLOR_HIGHLIGHT_IDLE 1.000, 1.000, 1.000, 0.05

#define WM_COLOR_BUTTON      1.000, 1.000, 1.000, 0.07
#define WM_COLOR_BUTTON_OVER 1.000, 1.000, 1.000, 0.16
#define WM_COLOR_BUTTON_DOWN 1.000, 1.000, 1.000, 0.24

#define WM_COLOR_CLOSE_OVER 0.898, 0.282, 0.302
#define WM_COLOR_CLOSE_DOWN 0.741, 0.196, 0.220

#define WM_COLOR_ACCENT 0.208, 0.518, 0.894

#define WM_COLOR_TASKBAR        0.071, 0.071, 0.071, 1.00
#define WM_COLOR_TASKBAR_HOVER  1.000, 1.000, 1.000, 0.07
#define WM_COLOR_TASKBAR_ACTIVE 1.000, 1.000, 1.000, 0.11
#define WM_COLOR_TASKBAR_DOWN   1.000, 1.000, 1.000, 0.16
#define WM_COLOR_TASKBAR_MARK   0.600, 0.600, 0.600, 0.70

/**
 * @brief The typeface titles are drawn in, loaded once at startup.
 */

#define WM_FONT_PATH "/usr/share/fonts/ttf/Ubuntu-M.ttf"
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
 * @brief The titlebar buttons, round and right-aligned, hit-tested ahead of the resize grips.
 */

#define WM_BUTTON_SIZE   22
#define WM_BUTTON_MARGIN 8
#define WM_BUTTON_GAP    6
#define WM_BUTTON_GLYPH  8

/**
 * @brief The titlebar icon, in the slot that mirrors the close button, dimmed like the title when idle.
 */

#define WM_ICON_SIZE     20
#define WM_ICON_IDLE     0.55
#define WM_ICON_FALLBACK "application-x-executable"

/**
 * @brief The taskbar: a rail down the left edge with the launcher, an entry per window and a clock,
 * joined to a thin border round the other three edges whose inner corners are rounded like a window's.
 *
 * Windows are placed, clamped and maximised within the rounded inset it leaves.
 */

#define WM_TASKBAR_WIDTH       48
#define WM_TASKBAR_BORDER      8
#define WM_TASKBAR_RADIUS      UI_WINDOW_RADIUS
#define WM_TASKBAR_ITEM        34
#define WM_TASKBAR_PAD         ((WM_TASKBAR_WIDTH - WM_TASKBAR_ITEM) / 2)
#define WM_TASKBAR_GAP         4
#define WM_TASKBAR_ITEM_RADIUS 9
#define WM_TASKBAR_MARK        3
#define WM_TASKBAR_CLOCK       40
#define WM_TASKBAR_CLOCK_SIZE  15.0
#define WM_TASKBAR_FONT_SIZE   13.0

/**
 * @brief The label that grows out of the rail beside the item under the pointer.
 */

#define WM_TASKBAR_POPOUT_RADIUS 10
#define WM_TASKBAR_POPOUT_PAD    14
#define WM_TASKBAR_POPOUT_MAX    360

/**
 * @brief The launcher's mark: a 3x3 grid of dots, this far apart centre to centre and this big in radius.
 */
#define WM_TASKBAR_LAUNCHER_PITCH 8.0
#define WM_TASKBAR_LAUNCHER_DOT   2.5

/**
 * @brief What the taskbar's own items name in place of a window id, and what the launcher's button starts.
 */
#define WM_TASKBAR_LAUNCHER UINT32_MAX
#define WM_TASKBAR_DATE     (UINT32_MAX - 1)
#define WM_LAUNCHER_PATH    "/usr/bin/aplus-launcher"

/**
 * @brief The server's cache on the tmpfs, and the copy of the launcher kept there, which starts without reading the disk.
 */
#define WM_CACHE_PATH    "/tmp/.wm"
#define WM_LAUNCHER_COPY WM_CACHE_PATH "/aplus-launcher"


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
    WM_CURSOR_TEXT,

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


/**
 * @brief A blurred rounded rectangle, cut into nine and stretched around a frame of any size.
 */
typedef struct {

    //? Rows then columns: a corner, the one-pixel strip that is stretched along an edge, and
    //? the opposite corner. The middle of the middle is never drawn, since a frame covers it.
    cairo_surface_t* pieces[3][3];

    int blur;
    int corner;

} wm_shadow_t;


/**
 * @brief What is left on screen of a window that has gone: a picture of it, fading and shrinking away.
 */
typedef struct wm_ghost {

    cairo_surface_t* surface;

    wm_rect_t rect;
    ui_anim_t fade;

    struct wm_ghost* next;

} wm_ghost_t;


typedef struct wm_client wm_client_t;
typedef struct wm_window wm_window_t;


/**
 * @brief The drawer: the window slid out of the rail, or a picture of it sliding back in after it has gone.
 */
typedef struct {

    wm_window_t* window;
    cairo_surface_t* still;

    wm_rect_t rect;

    ui_anim_t open;
    ui_anim_t height;

    wm_rect_t painted;

} wm_drawer_t;


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

    //? Already scaled to WM_ICON_SIZE, and a copy rather than libui's cached surface,
    //? which the cache destroys once other icons push it out.
    cairo_surface_t* icon;

    //? The shape the client asked the pointer to take over the content area.
    wm_cursor_shape_t cursor;

    //? Position of the *content* area on screen. The frame is derived from it.
    int x;
    int y;
    int width;
    int height;

    //? Bumped on every resize. A commit stamped with an older serial describes the surface
    //? the window used to have and is dropped rather than read against the current one.
    uint32_t serial;

    //? The window's surface, and the shared memory segment it lives in. The server creates
    //? the segment and hands its id to the client, which maps the same frames and copies
    //? each finished frame into them from a back buffer of its own as it commits -- so
    //? `backstore` is what compositing reads, and nothing but a damage rectangle travels
    //? over the socket.
    cairo_surface_t* backstore;

    int shm_id;
    size_t shm_size;
    int stride;

    void* shm_addr;

    //? Transitions: growing in on the first commit, the focus state, and the light on each
    //? titlebar button.
    ui_anim_t appear;
    ui_anim_t focus;
    ui_anim_t hover_close;
    ui_anim_t hover_maximize;
    ui_anim_t hover_minimize;
    ui_anim_t hover_task;

    //? Hidden in its taskbar entry, and how far it has gone there: 1 once it is out of sight.
    bool minimized;
    ui_anim_t minimize;

    //? Filling the display, and the content area it had before, to go back to.
    bool maximized;
    wm_rect_t restore;

    //? Maximising or restoring: a picture of the frame as it was, stretched towards where the
    //? frame now is while the live window grows out of it.
    cairo_surface_t* morph;
    wm_rect_t morph_rect;
    wm_rect_t morph_from;
    ui_anim_t morph_anim;

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
    WM_REGION_MAXIMIZE,
    WM_REGION_MINIMIZE,
    WM_REGION_TASKBAR,
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

    //? The titlebar button the pointer is over, if any. It doubles as the "still on the
    //? button" test that decides whether releasing there actually does anything.
    struct {
        wm_window_t* window;
        wm_region_t region;
    } hover;

    //? The last press on a titlebar, against which the next one is counted as a double click.
    struct {
        uint32_t window_id;
        uint64_t time;
    } title_click;

    //? Windows already gone, still fading out, painted over every window still open.
    wm_ghost_t* ghosts;

    wm_shadow_t shadow_key;
    wm_shadow_t shadow_ambient;

    //? The clock reading the frame being composited is drawn at.
    uint64_t frame_time;

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

    //? The taskbar item the pointer is over and the one it went down on: a window id,
    //? WM_TASKBAR_LAUNCHER, WM_TASKBAR_DATE, or 0 for nothing. And the minute the clock last showed.
    struct {
        uint32_t hover;
        uint32_t pressed;
        ui_anim_t hover_launcher;
        long minute;

        //? The label beside the hovered item: what it says, how far out it is, where it is
        //? centred and how wide it is when fully out, and where the last frame drew it.
        struct {
            char text[UI_TITLE_MAX * 2];
            ui_anim_t open;
            ui_anim_t center;
            ui_anim_t width;
            wm_rect_t painted;
        } popout;
    } taskbar;

    wm_drawer_t drawer;

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
wm_rect_t wm_rect_union(const wm_rect_t* a, const wm_rect_t* b);

/**
 * @brief Implemented in image.c.
 */
void* wm_slurp(const char* path, size_t* size);
void* wm_image_probe(const char* path, int max_size, size_t* size, int* width, int* height);

/**
 * @brief Implemented in draw.c.
 */
cairo_surface_t* wm_surface_create(cairo_format_t format, int width, int height);
cairo_surface_t* wm_icon_create(const char* name, int size);

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
 * @brief Implemented in shadow.c.
 */
int wm_shadow_init(wm_shadow_t* shadow, int blur, int radius);
void wm_shadow_fini(wm_shadow_t* shadow);
void wm_shadow_paint(cairo_t* cr, const wm_shadow_t* shadow, const wm_rect_t* frame, int offset, double alpha);

/**
 * @brief Implemented in anim.c.
 */
int wm_animate(uint64_t now);
void wm_ghost_create(wm_window_t* win);
void wm_ghost_paint(cairo_t* cr, const wm_rect_t* damage, size_t count);
void wm_ghost_fini(void);
cairo_surface_t* wm_window_snapshot(wm_window_t* win, wm_rect_t* rect);

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
void wm_input_refresh_cursor(void);

/**
 * @brief Implemented in keys.c.
 */
bool wm_keys_handle(uint16_t vkey, uint8_t down);
void wm_keys_reap(void);
void wm_spawn(const char* const* argv);
void wm_launcher_copy(void);

/**
 * @brief Implemented in taskbar.c.
 */
wm_rect_t wm_taskbar_rect(void);
wm_rect_t wm_workarea(void);
wm_rect_t wm_taskbar_entry_rect(const wm_window_t* win);
bool wm_taskbar_contains(int x, int y);
void wm_taskbar_damage(void);
void wm_taskbar_paint(cairo_t* cr, const wm_rect_t* damage, size_t count);
void wm_taskbar_hover(int x, int y);
void wm_taskbar_press(int x, int y);
bool wm_taskbar_release(int x, int y);
void wm_taskbar_forget(const wm_window_t* win);
int wm_taskbar_open(void);
void wm_taskbar_tick(int fd);
void wm_taskbar_fini(void);

/**
 * @brief Implemented in drawer.c.
 */
void wm_drawer_attach(wm_window_t* win);
void wm_drawer_reveal(wm_window_t* win);
void wm_drawer_resized(wm_window_t* win);
void wm_drawer_detach(wm_window_t* win);
void wm_drawer_press(const wm_window_t* win, wm_region_t region);
wm_window_t* wm_drawer_at(int x, int y);
void wm_drawer_damage(void);
void wm_drawer_settle(void);
void wm_drawer_paint(cairo_t* cr, const wm_rect_t* damage, size_t count);
void wm_drawer_fini(void);

/**
 * @brief Implemented in window.c.
 */
wm_window_t* wm_window_create(wm_client_t* client, int width, int height, const char* title, uint32_t flags);
void wm_window_destroy(wm_window_t* win);
void wm_window_request_close(wm_window_t* win);
void wm_window_set_icon(wm_window_t* win, const char* name);
void wm_window_set_cursor(wm_window_t* win, ui_cursor_t cursor);
wm_window_t* wm_window_from_id(uint32_t id);
wm_window_t* wm_window_topmost(void);
bool wm_window_listed(const wm_window_t* win);
void wm_window_minimize(wm_window_t* win);
void wm_window_restore(wm_window_t* win);
bool wm_window_borderless(const wm_window_t* win);
bool wm_window_centered(const wm_window_t* win);
bool wm_window_drawer(const wm_window_t* win);
bool wm_window_translucent(const wm_window_t* win);
wm_insets_t wm_window_insets(const wm_window_t* win);
cairo_format_t wm_window_format(const wm_window_t* win);
wm_rect_t wm_window_frame(const wm_window_t* win);
wm_rect_t wm_window_shadow_rect(const wm_window_t* win);
wm_rect_t wm_window_close_rect(const wm_window_t* win);
wm_rect_t wm_window_maximize_rect(const wm_window_t* win);
wm_rect_t wm_window_button_rect(const wm_window_t* win, wm_region_t region);
ui_anim_t* wm_window_button_hover(wm_window_t* win, wm_region_t region);
int wm_window_radius(const wm_window_t* win);
bool wm_window_opaque(const wm_window_t* win);
void wm_window_toggle_maximize(wm_window_t* win);
void wm_window_unmaximize_at(wm_window_t* win, int pointer_x, int pointer_y);
wm_region_t wm_window_hit_test(int x, int y, wm_window_t** out);
void wm_window_raise(wm_window_t* win);
void wm_window_focus(wm_window_t* win);
void wm_window_move(wm_window_t* win, int x, int y);
void wm_window_clamp_size(const wm_window_t* win, int* width, int* height);
int wm_window_resize(wm_window_t* win, int width, int height);
int wm_window_notify_configure(wm_window_t* win);
void wm_window_paint(cairo_t* cr, wm_window_t* win);
void wm_window_paint_still(cairo_t* cr, wm_window_t* win);
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
