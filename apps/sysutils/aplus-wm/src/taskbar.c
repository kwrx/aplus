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

/**
 * @brief The taskbar: the launcher's button at the left, an entry per window in the order they opened, a clock at the right.
 */

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <wm.h>


/**
 * @brief The most entries the taskbar lays out, the same bound compositing puts on the stack.
 */
#define WM_TASKBAR_ENTRIES_MAX 64


static const char* const wm_taskbar_command[] = {WM_LAUNCHER_PATH, NULL};


/**
 * @brief A thread that sleeps until each minute turns over and says so down a socketpair the main loop polls.
 */
static int wm_clock_pipe[2] = {-1, -1};


static void* wm_clock_thread(void* arg) {

    (void)arg;

    for (;;) {

        struct timespec now;

        if (clock_gettime(CLOCK_REALTIME, &now) < 0) {
            break;
        }

        struct timespec nap = {59 - now.tv_sec % 60, 1000000000L - now.tv_nsec};

        while (nanosleep(&nap, &nap) < 0 && errno == EINTR) {
            ;
        }

        const char tick = 0;

        if (ui_send_all(wm_clock_pipe[1], &tick, sizeof(tick)) < 0) {
            break;
        }
    }

    return NULL;
}


/**
 * @brief Starts the clock's thread.
 *
 * @return The read end of its socketpair, for the main loop to poll, or -1 with errno set.
 */
int wm_taskbar_open(void) {

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, wm_clock_pipe) < 0) {
        fprintf(stderr, "aplus-wm: socketpair() failed: %s\n", strerror(errno));
        return -1;
    }


    pthread_t thread;

    if (pthread_create(&thread, NULL, wm_clock_thread, NULL) != 0) {
        fprintf(stderr, "aplus-wm: pthread_create() failed for the clock: %s\n", strerror(errno));
        return -1;
    }

    pthread_detach(thread);

    return wm_clock_pipe[0];
}


wm_rect_t wm_taskbar_rect(void) {

    const wm_rect_t r = {0, wm.display.height - WM_TASKBAR_HEIGHT, wm.display.width, WM_TASKBAR_HEIGHT};

    return r;
}


/**
 * @brief Reports the part of the display that windows are placed, clamped and maximised within.
 *
 * @return Everything above the taskbar.
 */
wm_rect_t wm_workarea(void) {

    const wm_rect_t r = {0, 0, wm.display.width, wm.display.height - WM_TASKBAR_HEIGHT};

    return r;
}


bool wm_taskbar_contains(int x, int y) {

    const wm_rect_t bar = wm_taskbar_rect();

    return wm_rect_contains_point(&bar, x, y);
}


void wm_taskbar_damage(void) {

    const wm_rect_t bar = wm_taskbar_rect();

    wm_damage(&bar);
}


/**
 * @brief Collects the windows that have entries, oldest first, which is the order the entries keep.
 *
 * @param out Receives the windows.
 * @return How many there are.
 */
static size_t wm_taskbar_windows(wm_window_t** out) {

    size_t count = 0;

    for (wm_window_t* win = wm.windows; win && count < WM_TASKBAR_ENTRIES_MAX; win = win->next) {

        if (!wm_window_listed(win)) {
            continue;
        }

        size_t i = count++;

        for (; i > 0 && out[i - 1]->id > win->id; i--) {
            out[i] = out[i - 1];
        }

        out[i] = win;
    }

    return count;
}


static wm_rect_t wm_taskbar_launcher_rect(void) {

    const wm_rect_t bar = wm_taskbar_rect();

    const wm_rect_t r = {bar.x + WM_TASKBAR_PAD, bar.y + (WM_TASKBAR_HEIGHT - WM_TASKBAR_ITEM) / 2, WM_TASKBAR_ITEM, WM_TASKBAR_ITEM};

    return r;
}


/**
 * @brief Reports where an entry sits, every entry narrowing alike once they stop fitting.
 *
 * @param index The entry's place in the order.
 * @param count How many entries there are.
 * @return The entry's rectangle, empty when it falls past the clock.
 */
static wm_rect_t wm_taskbar_slot(size_t index, size_t count) {

    const wm_rect_t bar      = wm_taskbar_rect();
    const wm_rect_t launcher = wm_taskbar_launcher_rect();

    const int left  = launcher.x + launcher.width + 2 * WM_TASKBAR_GAP;
    const int right = bar.x + bar.width - WM_TASKBAR_CLOCK - WM_TASKBAR_PAD;

    const int width = WM_CLAMP((right - left) / (int)WM_MAX(count, 1) - WM_TASKBAR_GAP, WM_TASKBAR_ITEM, WM_TASKBAR_ENTRY_MAX);

    wm_rect_t r = {left + (int)index * (width + WM_TASKBAR_GAP), launcher.y, width, WM_TASKBAR_ITEM};

    if (r.x + r.width > right) {
        r.width = 0;
    }

    return r;
}


/**
 * @brief Reports where a window's entry is, which is where minimising sends it.
 *
 * @param win The window.
 * @return The entry, or an empty rectangle in the middle of the taskbar when it has none on show.
 */
wm_rect_t wm_taskbar_entry_rect(const wm_window_t* win) {

    wm_window_t* windows[WM_TASKBAR_ENTRIES_MAX];

    const size_t count = wm_taskbar_windows(windows);

    for (size_t i = 0; i < count; i++) {

        if (windows[i] != win) {
            continue;
        }

        const wm_rect_t r = wm_taskbar_slot(i, count);

        if (r.width > 0) {
            return r;
        }
    }


    const wm_rect_t bar = wm_taskbar_rect();

    const wm_rect_t none = {bar.x + bar.width / 2, bar.y + bar.height / 2, 0, 0};

    return none;
}


/**
 * @brief Reports what on the taskbar is under a point.
 *
 * @param x The point.
 * @param y The point.
 * @return A window id, WM_TASKBAR_LAUNCHER, or 0 for nothing.
 */
static uint32_t wm_taskbar_item_at(int x, int y) {

    if (!wm_taskbar_contains(x, y)) {
        return 0;
    }


    const wm_rect_t launcher = wm_taskbar_launcher_rect();

    if (wm_rect_contains_point(&launcher, x, y)) {
        return WM_TASKBAR_LAUNCHER;
    }


    wm_window_t* windows[WM_TASKBAR_ENTRIES_MAX];

    const size_t count = wm_taskbar_windows(windows);

    for (size_t i = 0; i < count; i++) {

        const wm_rect_t r = wm_taskbar_slot(i, count);

        if (wm_rect_contains_point(&r, x, y)) {
            return windows[i]->id;
        }
    }

    return 0;
}


/**
 * @brief Eases the wash behind one taskbar item in or out.
 *
 * @param item A window id or WM_TASKBAR_LAUNCHER.
 * @param lit Whether the pointer is on it.
 */
static void wm_taskbar_light(uint32_t item, bool lit) {

    wm_window_t* win = item == WM_TASKBAR_LAUNCHER ? NULL : wm_window_from_id(item);

    ui_anim_t* anim = item == WM_TASKBAR_LAUNCHER ? &wm.taskbar.hover_launcher : win ? &win->hover_task : NULL;

    if (anim) {
        ui_anim_to(anim, lit ? 1.0 : 0.0, UI_MOTION_FAST, ui_anim_now());
    }

    wm_taskbar_damage();
}


/**
 * @brief Follows the pointer from item to item, lighting none while a window is being dragged.
 *
 * @param x The pointer.
 * @param y The pointer.
 */
void wm_taskbar_hover(int x, int y) {

    const uint32_t item = wm.drag.window ? 0 : wm_taskbar_item_at(x, y);

    if (item == wm.taskbar.hover) {
        return;
    }

    if (wm.taskbar.hover) {
        wm_taskbar_light(wm.taskbar.hover, false);
    }

    wm.taskbar.hover = item;

    if (item) {
        wm_taskbar_light(item, true);
    }
}


void wm_taskbar_press(int x, int y) {

    wm.taskbar.pressed = wm_taskbar_item_at(x, y);

    wm_taskbar_damage();
}


/**
 * @brief Acts on a release over the item the press went down on: start the launcher, or show, raise or hide a window.
 *
 * @param x The pointer.
 * @param y The pointer.
 * @return true when the press had been on the taskbar, which then owns the release.
 */
bool wm_taskbar_release(int x, int y) {

    const uint32_t pressed = wm.taskbar.pressed;

    if (!pressed) {
        return false;
    }

    wm.taskbar.pressed = 0;

    wm_taskbar_damage();


    if (wm_taskbar_item_at(x, y) != pressed) {
        return true;
    }

    if (pressed == WM_TASKBAR_LAUNCHER) {
        wm_spawn(wm_taskbar_command);
        return true;
    }


    wm_window_t* win = wm_window_from_id(pressed);

    if (!win) {
        return true;
    }

    if (win->minimized) {

        wm_window_restore(win);

    } else if (win == wm.focused && win == wm_window_topmost()) {

        wm_window_minimize(win);

    } else {

        wm_window_raise(win);
        wm_window_focus(win);
    }

    return true;
}


/**
 * @brief Lets go of a window that is going away, so that no item is left lit or pressed for it.
 *
 * @param win The window.
 */
void wm_taskbar_forget(const wm_window_t* win) {

    if (wm.taskbar.hover == win->id) {
        wm.taskbar.hover = 0;
    }

    if (wm.taskbar.pressed == win->id) {
        wm.taskbar.pressed = 0;
    }

    wm_taskbar_damage();
}


/**
 * @brief Fills an item's rounded wash, at an alpha made of hover, press and whether it is the active one.
 *
 * @param cr The cairo context.
 * @param r The item.
 * @param hover How lit it is, 0 to 1.
 * @param pressed Whether it is held down.
 * @param active How much it is the active window's, 0 to 1.
 */
static void wm_taskbar_paint_wash(cairo_t* cr, const wm_rect_t* r, double hover, bool pressed, double active) {

    const double over[4] = {WM_COLOR_TASKBAR_HOVER};
    const double on[4]   = {WM_COLOR_TASKBAR_ACTIVE};
    const double down[4] = {WM_COLOR_TASKBAR_DOWN};

    const double alpha = pressed ? down[3] : on[3] * active + over[3] * hover;

    if (alpha <= 0.0) {
        return;
    }

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, alpha);

    ui_draw_rounded_rect_d(cr, r->x, r->y, r->width, r->height, WM_TASKBAR_RADIUS);
    cairo_fill(cr);
}


/**
 * @brief Draws an icon already scaled to WM_ICON_SIZE, which makes it a plain copy.
 *
 * @param cr The cairo context.
 * @param icon The icon, WM_ICON_SIZE square, or NULL for nothing.
 * @param x The left edge.
 * @param y The top edge.
 * @param alpha How opaque to draw it.
 */
static void wm_taskbar_paint_icon(cairo_t* cr, cairo_surface_t* icon, int x, int y, double alpha) {

    if (!icon) {
        return;
    }

    cairo_save(cr);

    cairo_rectangle(cr, x, y, WM_ICON_SIZE, WM_ICON_SIZE);
    cairo_clip(cr);

    cairo_set_source_surface(cr, icon, x, y);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
    cairo_paint_with_alpha(cr, alpha);

    cairo_restore(cr);
}


/**
 * @brief Draws one window's entry: its icon, its title cut to fit, and a mark along the bottom.
 *
 * The mark is grey for a window in the background and widens into the accent for the active one.
 *
 * @param cr The cairo context.
 * @param win The window.
 * @param r The entry.
 */
static void wm_taskbar_paint_entry(cairo_t* cr, wm_window_t* win, const wm_rect_t* r) {

    const double hover  = ui_anim_value(&win->hover_task, wm.frame_time);
    const double active = ui_anim_value(&win->focus, wm.frame_time);
    const double gone   = ui_anim_value(&win->minimize, wm.frame_time);

    const bool pressed = wm.taskbar.pressed == win->id && wm.taskbar.hover == win->id;

    wm_taskbar_paint_wash(cr, r, hover, pressed, active);


    const int icon_x = r->x + (WM_TASKBAR_ITEM - WM_ICON_SIZE) / 2;
    const int icon_y = r->y + (WM_TASKBAR_ITEM - WM_ICON_SIZE) / 2;

    wm_taskbar_paint_icon(cr, win->icon, icon_x, icon_y, ui_lerp(1.0, WM_ICON_IDLE, gone));


    const double text_x     = r->x + WM_TASKBAR_ITEM;
    const double text_width = r->width - WM_TASKBAR_ITEM - 2 * WM_TASKBAR_GAP;

    cairo_font_face_t* face = ui_font_face(WM_FONT_PATH);

    if (face && win->title[0] && text_width >= 16.0) {

        char title[UI_TITLE_MAX * 2];

        ui_draw_ellipsize(title, sizeof(title), win->title, WM_FONT_PATH, WM_TASKBAR_FONT_SIZE, text_width);


        cairo_font_extents_t font;

        cairo_set_font_face(cr, face);
        cairo_set_font_size(cr, WM_TASKBAR_FONT_SIZE);
        cairo_font_extents(cr, &font);

        const double y = r->y + (r->height - (font.ascent + font.descent)) / 2.0 + font.ascent;

        const double shade = ui_lerp(ui_lerp(0.780, 0.933, active), 0.540, gone);

        cairo_set_source_rgb(cr, shade, shade, shade);
        cairo_move_to(cr, round(text_x), round(y));
        cairo_show_text(cr, title);
    }


    const double grey[4]   = {WM_COLOR_TASKBAR_MARK};
    const double accent[3] = {WM_COLOR_ACCENT};

    const double width = ui_lerp(8.0, 18.0, active);

    cairo_set_source_rgba(cr, ui_lerp(grey[0], accent[0], active), ui_lerp(grey[1], accent[1], active), ui_lerp(grey[2], accent[2], active), ui_lerp(grey[3], 1.0, active));

    ui_draw_rounded_rect_d(cr, r->x + (r->width - width) / 2.0, r->y + r->height - WM_TASKBAR_MARK, width, WM_TASKBAR_MARK, WM_TASKBAR_MARK / 2.0);
    cairo_fill(cr);
}


/**
 * @brief Draws the time at the right end, in the local time zone.
 *
 * @param cr The cairo context.
 * @param bar The taskbar.
 */
static void wm_taskbar_paint_clock(cairo_t* cr, const wm_rect_t* bar) {

    cairo_font_face_t* face = ui_font_face(WM_FONT_PATH);

    if (!face) {
        return;
    }


    const time_t now = time(NULL);

    struct tm tm;

    if (!localtime_r(&now, &tm)) {
        return;
    }

    char text[8];

    strftime(text, sizeof(text), "%H:%M", &tm);


    cairo_text_extents_t extents;
    cairo_font_extents_t font;

    cairo_set_font_face(cr, face);
    cairo_set_font_size(cr, WM_FONT_SIZE);
    cairo_text_extents(cr, text, &extents);
    cairo_font_extents(cr, &font);

    const double right = bar->x + bar->width - WM_TASKBAR_PAD - WM_TASKBAR_GAP * 2;

    const double x = right - extents.x_advance;
    const double y = bar->y + (bar->height - (font.ascent + font.descent)) / 2.0 + font.ascent;

    cairo_set_source_rgb(cr, WM_COLOR_TITLE_ACTIVE);
    cairo_move_to(cr, round(x), round(y));
    cairo_show_text(cr, text);
}


/**
 * @brief Draws the taskbar over everything but the windows still fading out.
 *
 * @param cr The cairo context to draw with, already clipped to the damage.
 * @param damage The damaged rectangles.
 * @param count How many there are.
 */
void wm_taskbar_paint(cairo_t* cr, const wm_rect_t* damage, size_t count) {

    const wm_rect_t bar = wm_taskbar_rect();

    bool visible = false;

    for (size_t i = 0; i < count && !visible; i++) {
        visible = wm_rect_intersects(&bar, &damage[i]);
    }

    if (!visible) {
        return;
    }


    cairo_save(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    cairo_set_source_rgba(cr, WM_COLOR_TASKBAR);
    cairo_rectangle(cr, bar.x, bar.y, bar.width, bar.height);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, WM_COLOR_TASKBAR_EDGE);
    cairo_rectangle(cr, bar.x, bar.y, bar.width, 1);
    cairo_fill(cr);


    if (!wm.taskbar.launcher_icon) {
        wm.taskbar.launcher_icon = wm_icon_create("aplus-launcher", WM_ICON_SIZE);
    }

    const wm_rect_t launcher = wm_taskbar_launcher_rect();

    const bool held = wm.taskbar.pressed == WM_TASKBAR_LAUNCHER && wm.taskbar.hover == WM_TASKBAR_LAUNCHER;

    wm_taskbar_paint_wash(cr, &launcher, ui_anim_value(&wm.taskbar.hover_launcher, wm.frame_time), held, 0.0);
    wm_taskbar_paint_icon(cr, wm.taskbar.launcher_icon, launcher.x + (WM_TASKBAR_ITEM - WM_ICON_SIZE) / 2, launcher.y + (WM_TASKBAR_ITEM - WM_ICON_SIZE) / 2, 1.0);


    wm_window_t* windows[WM_TASKBAR_ENTRIES_MAX];

    const size_t entries = wm_taskbar_windows(windows);

    for (size_t i = 0; i < entries; i++) {

        const wm_rect_t r = wm_taskbar_slot(i, entries);

        if (r.width <= 0) {
            break;
        }

        wm_taskbar_paint_entry(cr, windows[i], &r);
    }


    wm_taskbar_paint_clock(cr, &bar);

    cairo_restore(cr);
}


/**
 * @brief Drains the clock's socketpair and repaints the clock if the minute has turned over.
 *
 * @param fd The read end returned by wm_taskbar_open().
 */
void wm_taskbar_tick(int fd) {

    char ticks[16];

    if (read(fd, ticks, sizeof(ticks)) <= 0) {
        return;
    }


    const long minute = (long)(time(NULL) / 60);

    if (minute != wm.taskbar.minute) {

        wm.taskbar.minute = minute;

        wm_taskbar_damage();
    }
}


void wm_taskbar_fini(void) {

    if (wm.taskbar.launcher_icon) {
        cairo_surface_destroy(wm.taskbar.launcher_icon);
        wm.taskbar.launcher_icon = NULL;
    }

    for (int i = 0; i < 2; i++) {

        if (wm_clock_pipe[i] >= 0) {
            close(wm_clock_pipe[i]);
            wm_clock_pipe[i] = -1;
        }
    }
}
