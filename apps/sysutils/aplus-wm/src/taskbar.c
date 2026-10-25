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
 * @brief The taskbar: a rail down the left edge with the launcher's button, an entry per window in the order they opened,
 * a clock and a power button, joined to a border round the rest of the screen.
 */

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <aplus/ulog.h>

#include <wm.h>


/**
 * @brief The most entries the taskbar lays out, the same bound compositing puts on the stack.
 */
#define WM_TASKBAR_ENTRIES_MAX 64


static const char* const wm_taskbar_command[]  = {WM_LAUNCHER_PATH, "--all", "--drawer", NULL};
static const char* const wm_taskbar_halt[]     = {WM_SHUTDOWN_PATH, "--halt", "now", NULL};
static const char* const wm_taskbar_restart[]  = {WM_SHUTDOWN_PATH, "--reboot", "now", NULL};
static const char* const wm_taskbar_shutdown[] = {WM_SHUTDOWN_PATH, "--poweroff", "now", NULL};


/**
 * @brief The session menu's rows, top to bottom, the last one level with the power button.
 */
static const struct {
    uint32_t item;
    const char* label;
    const char* const* argv;
} wm_taskbar_session_rows[WM_TASKBAR_SESSION_ROWS] = {
    {WM_TASKBAR_HALT,     "Halt",      wm_taskbar_halt    },
    {WM_TASKBAR_RESTART,  "Restart",   wm_taskbar_restart },
    {WM_TASKBAR_SHUTDOWN, "Shut down", wm_taskbar_shutdown},
};


/**
 * @brief Finds an item's row in the session menu.
 *
 * @param item Any taskbar item.
 * @return Its index in wm_taskbar_session_rows, or -1 when it is not a session item.
 */
static int wm_taskbar_session_index(uint32_t item) {

    for (int i = 0; i < WM_TASKBAR_SESSION_ROWS; i++) {

        if (wm_taskbar_session_rows[i].item == item) {
            return i;
        }
    }

    return -1;
}


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
        ulog_errorf("socketpair() failed: %s", strerror(errno));
        return -1;
    }


    pthread_t thread;

    if (pthread_create(&thread, NULL, wm_clock_thread, NULL) != 0) {
        ulog_errorf("pthread_create() failed for the clock: %s", strerror(errno));
        return -1;
    }

    pthread_detach(thread);

    return wm_clock_pipe[0];
}


/**
 * @brief Reports the rail down the left edge, which holds everything on the taskbar that can change.
 *
 * @return The rail, the full height of the display.
 */
wm_rect_t wm_taskbar_rect(void) {

    const wm_rect_t r = {0, 0, WM_TASKBAR_WIDTH, wm.display.height};

    return r;
}


/**
 * @brief Reports the part of the display that windows are placed, clamped and maximised within.
 *
 * @return The inset left by the rail and the border, whose corners the border rounds off.
 */
wm_rect_t wm_workarea(void) {

    const wm_rect_t r = {WM_TASKBAR_WIDTH, WM_TASKBAR_BORDER, wm.display.width - WM_TASKBAR_WIDTH - WM_TASKBAR_BORDER, wm.display.height - 2 * WM_TASKBAR_BORDER};

    return r;
}


/**
 * @brief Reports where the session menu is once all the way out, leaving out the fillets that join it to the rail and the border.
 *
 * @return The menu, sitting on the bottom border against the rail.
 */
static wm_rect_t wm_taskbar_session_rect(void) {

    const wm_rect_t work = wm_workarea();
    const int height     = WM_TASKBAR_SESSION_ROWS * (WM_TASKBAR_ITEM + WM_TASKBAR_GAP) - WM_TASKBAR_GAP + 2 * WM_TASKBAR_PAD;

    const wm_rect_t r = {WM_TASKBAR_WIDTH, work.y + work.height - height, WM_TASKBAR_SESSION_WIDTH, height};

    return r;
}


/**
 * @brief Reports whether the session menu is out, or on its way out.
 *
 * @return true from the press that opens it to the one that closes it.
 */
static bool wm_taskbar_session_open(void) {
    return ui_anim_target(&wm.taskbar.session.open) > 0.0;
}


/**
 * @brief Reports whether a point is on the taskbar, which is everything outside the rounded inset.
 *
 * @param x The point.
 * @param y The point.
 * @return true on the rail, the border, a corner it rounds off, or the session menu while it is out.
 */
bool wm_taskbar_contains(int x, int y) {

    const wm_rect_t work = wm_workarea();

    if (!wm_rect_contains_point(&work, x, y)) {
        return true;
    }

    const wm_rect_t session = wm_taskbar_session_rect();

    if (wm_taskbar_session_open() && wm_rect_contains_point(&session, x, y)) {
        return true;
    }


    const double r  = WM_TASKBAR_RADIUS;
    const double px = x + 0.5;
    const double py = y + 0.5;

    const double dx = WM_MAX(WM_MAX(work.x + r - px, px - (work.x + work.width - r)), 0.0);
    const double dy = WM_MAX(WM_MAX(work.y + r - py, py - (work.y + work.height - r)), 0.0);

    return dx * dx + dy * dy > r * r;
}


/**
 * @brief Reports where the popout is at a moment, with the fillets that join it to the rail.
 *
 * @param now The clock reading.
 * @return Its bounding box, empty while it is all the way in.
 */
static wm_rect_t wm_taskbar_popout_extent(uint64_t now) {

    const double open = ui_anim_value(&wm.taskbar.popout.open, now);

    if (open <= 0.0) {

        const wm_rect_t none = {0, 0, 0, 0};

        return none;
    }


    const double width  = ui_anim_value(&wm.taskbar.popout.width, now) * open;
    const double center = ui_anim_value(&wm.taskbar.popout.center, now);
    const double reach  = WM_TASKBAR_ITEM / 2.0 + WM_TASKBAR_POPOUT_RADIUS;

    const wm_rect_t r = {WM_TASKBAR_WIDTH, (int)floor(center - reach) - 2, (int)ceil(width) + 2, (int)ceil(2.0 * reach) + 4};

    return r;
}


/**
 * @brief Reports where the session menu is at a moment, with the fillets that join it to the rail and the border.
 *
 * @param now The clock reading.
 * @return Its bounding box, empty while it is all the way in.
 */
static wm_rect_t wm_taskbar_session_extent(uint64_t now) {

    const double open = ui_anim_value(&wm.taskbar.session.open, now);

    if (open <= 0.0) {

        const wm_rect_t none = {0, 0, 0, 0};

        return none;
    }


    const wm_rect_t panel = wm_taskbar_session_rect();

    const wm_rect_t r = {panel.x, panel.y - WM_TASKBAR_POPOUT_RADIUS - 2, (int)ceil(panel.width * open) + WM_TASKBAR_POPOUT_RADIUS + 2, panel.height + WM_TASKBAR_POPOUT_RADIUS + 4};

    return r;
}


/**
 * @brief Repaints the rail, and the popout and the session menu where the last frame drew them and where the next one will.
 */
void wm_taskbar_damage(void) {

    const wm_rect_t bar     = wm_taskbar_rect();
    const wm_rect_t popout  = wm_taskbar_popout_extent(wm.frame_time);
    const wm_rect_t session = wm_taskbar_session_extent(wm.frame_time);

    wm_damage(&bar);
    wm_damage(&wm.taskbar.popout.painted);
    wm_damage(&popout);
    wm_damage(&wm.taskbar.session.painted);
    wm_damage(&session);
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

    const wm_rect_t r = {WM_TASKBAR_PAD, WM_TASKBAR_PAD + 4, WM_TASKBAR_ITEM, WM_TASKBAR_ITEM};

    return r;
}


/**
 * @brief Reports where the power button sits, at the foot of the rail.
 *
 * @return The button's square, mirroring the launcher's at the head.
 */
static wm_rect_t wm_taskbar_power_rect(void) {

    const wm_rect_t r = {WM_TASKBAR_PAD, wm.display.height - WM_TASKBAR_PAD - 4 - WM_TASKBAR_ITEM, WM_TASKBAR_ITEM, WM_TASKBAR_ITEM};

    return r;
}


/**
 * @brief Reports where the clock sits, just above the power button.
 *
 * @return The clock's rectangle, as wide as the rail.
 */
static wm_rect_t wm_taskbar_clock_rect(void) {

    const wm_rect_t power = wm_taskbar_power_rect();

    const wm_rect_t r = {0, power.y - WM_TASKBAR_GAP - WM_TASKBAR_CLOCK, WM_TASKBAR_WIDTH, WM_TASKBAR_CLOCK};

    return r;
}


/**
 * @brief Reports where a row of the session menu is, once the menu is all the way out.
 *
 * @param index The row's place in wm_taskbar_session_rows.
 * @return The row, inset from the menu's edges.
 */
static wm_rect_t wm_taskbar_session_row(int index) {

    const wm_rect_t panel = wm_taskbar_session_rect();

    const wm_rect_t r = {panel.x + WM_TASKBAR_PAD, panel.y + WM_TASKBAR_PAD + index * (WM_TASKBAR_ITEM + WM_TASKBAR_GAP), panel.width - 2 * WM_TASKBAR_PAD, WM_TASKBAR_ITEM};

    return r;
}


/**
 * @brief Reports where an entry sits, one under another below the launcher's button.
 *
 * @param index The entry's place in the order.
 * @return The entry's square, empty when it would run into the clock.
 */
static wm_rect_t wm_taskbar_slot(size_t index) {

    const wm_rect_t launcher = wm_taskbar_launcher_rect();
    const wm_rect_t clock    = wm_taskbar_clock_rect();

    wm_rect_t r = {launcher.x, launcher.y + launcher.height + 3 * WM_TASKBAR_GAP + (int)index * (WM_TASKBAR_ITEM + WM_TASKBAR_GAP), WM_TASKBAR_ITEM, WM_TASKBAR_ITEM};

    if (r.y + r.height > clock.y - 2 * WM_TASKBAR_GAP) {
        r.width = 0;
    }

    return r;
}


/**
 * @brief Reports where a window's entry is, which is where minimising sends it.
 *
 * @param win The window.
 * @return The entry, or an empty rectangle in the middle of the rail when it has none on show.
 */
wm_rect_t wm_taskbar_entry_rect(const wm_window_t* win) {

    wm_window_t* windows[WM_TASKBAR_ENTRIES_MAX];

    const size_t count = wm_taskbar_windows(windows);

    for (size_t i = 0; i < count; i++) {

        if (windows[i] != win) {
            continue;
        }

        const wm_rect_t r = wm_taskbar_slot(i);

        if (r.width > 0) {
            return r;
        }
    }


    const wm_rect_t none = {WM_TASKBAR_WIDTH / 2, wm.display.height / 2, 0, 0};

    return none;
}


/**
 * @brief Reports where an item sits on the rail.
 *
 * @param item A window id, WM_TASKBAR_LAUNCHER, WM_TASKBAR_DATE, WM_TASKBAR_POWER, or 0.
 * @return Its rectangle, empty when it is not on show.
 */
static wm_rect_t wm_taskbar_item_rect(uint32_t item) {

    if (item == WM_TASKBAR_LAUNCHER) {
        return wm_taskbar_launcher_rect();
    }

    if (item == WM_TASKBAR_DATE) {
        return wm_taskbar_clock_rect();
    }

    if (item == WM_TASKBAR_POWER) {
        return wm_taskbar_power_rect();
    }


    const wm_window_t* win = item ? wm_window_from_id(item) : NULL;

    if (!win) {

        const wm_rect_t none = {0, 0, 0, 0};

        return none;
    }

    return wm_taskbar_entry_rect(win);
}


/**
 * @brief Reports what on the taskbar is under a point.
 *
 * @param x The point.
 * @param y The point.
 * @return A window id, WM_TASKBAR_LAUNCHER, WM_TASKBAR_DATE, a session item, or 0 for nothing.
 */
static uint32_t wm_taskbar_item_at(int x, int y) {

    if (!wm_taskbar_contains(x, y)) {
        return 0;
    }


    if (ui_anim_target(&wm.taskbar.session.open) >= 1.0) {

        for (int i = 0; i < WM_TASKBAR_SESSION_ROWS; i++) {

            const wm_rect_t r = wm_taskbar_session_row(i);

            if (wm_rect_contains_point(&r, x, y)) {
                return wm_taskbar_session_rows[i].item;
            }
        }
    }

    const wm_rect_t power = wm_taskbar_power_rect();

    if (wm_rect_contains_point(&power, x, y)) {
        return WM_TASKBAR_POWER;
    }


    const wm_rect_t launcher = wm_taskbar_launcher_rect();

    if (wm_rect_contains_point(&launcher, x, y)) {
        return WM_TASKBAR_LAUNCHER;
    }

    const wm_rect_t clock = wm_taskbar_clock_rect();

    if (wm_rect_contains_point(&clock, x, y)) {
        return WM_TASKBAR_DATE;
    }


    wm_window_t* windows[WM_TASKBAR_ENTRIES_MAX];

    const size_t count = wm_taskbar_windows(windows);

    for (size_t i = 0; i < count; i++) {

        const wm_rect_t r = wm_taskbar_slot(i);

        if (wm_rect_contains_point(&r, x, y)) {
            return windows[i]->id;
        }
    }

    return 0;
}


/**
 * @brief Writes what the popout says for an item: the window's title, the date, or what the launcher's button opens.
 *
 * @param item A window id, WM_TASKBAR_LAUNCHER, WM_TASKBAR_DATE or a session item.
 * @param out Receives the text, cut to fit the popout.
 * @param size The size of out.
 * @return false when there is nothing to say.
 */
static bool wm_taskbar_label(uint32_t item, char* out, size_t size) {

    char date[64];

    const char* text = NULL;

    if (wm_taskbar_session_index(item) >= 0) {

        return false;

    } else if (item == WM_TASKBAR_POWER) {

        text = wm_taskbar_session_open() ? NULL : "Power";

    } else if (item == WM_TASKBAR_LAUNCHER) {

        text = "Applications";

    } else if (item == WM_TASKBAR_DATE) {

        const time_t now = time(NULL);

        struct tm tm;

        if (localtime_r(&now, &tm) && strftime(date, sizeof(date), "%A %d %B", &tm)) {
            text = date;
        }

    } else {

        const wm_window_t* win = wm_window_from_id(item);

        text = win ? win->title : NULL;
    }

    if (!text || !text[0]) {
        return false;
    }

    ui_draw_ellipsize(out, size, text, WM_FONT_PATH, WM_TASKBAR_FONT_SIZE, WM_TASKBAR_POPOUT_MAX - 2 * WM_TASKBAR_POPOUT_PAD);

    return out[0] != '\0';
}


/**
 * @brief Slides the popout out beside an item, along to it from the one it was beside, or back in.
 *
 * @param item The item the pointer is over, or 0 for none.
 */
static void wm_taskbar_popout(uint32_t item) {

    const uint64_t now = ui_anim_now();

    const wm_rect_t at = wm_taskbar_item_rect(item);

    char text[sizeof(wm.taskbar.popout.text)];

    if (at.width <= 0 || !wm_taskbar_label(item, text, sizeof(text))) {

        ui_anim_to(&wm.taskbar.popout.open, 0.0, UI_MOTION_FAST, now);
        wm_taskbar_damage();

        return;
    }


    const double center = at.y + at.height / 2.0;
    const double width  = ceil(ui_draw_text_width(text, WM_FONT_PATH, WM_TASKBAR_FONT_SIZE)) + 2.0 * WM_TASKBAR_POPOUT_PAD;

    if (ui_anim_value(&wm.taskbar.popout.open, now) <= 0.0) {

        ui_anim_set(&wm.taskbar.popout.center, center);
        ui_anim_set(&wm.taskbar.popout.width, width);

    } else {

        ui_anim_to(&wm.taskbar.popout.center, center, UI_MOTION_NORMAL, now);
        ui_anim_to(&wm.taskbar.popout.width, width, UI_MOTION_NORMAL, now);
    }

    ui_anim_to(&wm.taskbar.popout.open, 1.0, UI_MOTION_NORMAL, now);

    memcpy(wm.taskbar.popout.text, text, sizeof(text));

    wm_taskbar_damage();
}


/**
 * @brief Finds the wash behind a taskbar item.
 *
 * @param item A window id, WM_TASKBAR_LAUNCHER, WM_TASKBAR_DATE or a session item.
 * @return Its animation, or NULL for an item that has none.
 */
static ui_anim_t* wm_taskbar_hover_anim(uint32_t item) {

    switch (item) {

        case WM_TASKBAR_LAUNCHER:
            return &wm.taskbar.hover_launcher;

        case WM_TASKBAR_POWER:
            return &wm.taskbar.hover_power;

    }

    const int row = wm_taskbar_session_index(item);

    if (row >= 0) {
        return &wm.taskbar.session.hover[row];
    }

    wm_window_t* win = wm_window_from_id(item);

    return win ? &win->hover_task : NULL;
}


/**
 * @brief Eases the wash behind one taskbar item in or out.
 *
 * @param item A window id, WM_TASKBAR_LAUNCHER, WM_TASKBAR_DATE or a session item.
 * @param lit Whether the pointer is on it.
 */
static void wm_taskbar_light(uint32_t item, bool lit) {

    ui_anim_t* anim = wm_taskbar_hover_anim(item);

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

    wm_taskbar_popout(item);
}


void wm_taskbar_press(int x, int y) {

    wm.taskbar.pressed = wm_taskbar_item_at(x, y);

    wm_taskbar_damage();
}


/**
 * @brief Slides the session menu out of the rail or back in, and brings the pointer's item and label up to date.
 *
 * @param open Whether it comes out.
 */
static void wm_taskbar_session(bool open) {

    ui_anim_to(&wm.taskbar.session.open, open ? 1.0 : 0.0, open ? UI_MOTION_NORMAL : UI_MOTION_FAST, ui_anim_now());

    wm_taskbar_hover(wm.pointer.x, wm.pointer.y);
    wm_taskbar_popout(wm.taskbar.hover);
}


/**
 * @brief Closes the session menu when a press lands anywhere but on it or on the power button.
 *
 * @param x The press.
 * @param y The press.
 */
void wm_taskbar_session_press(int x, int y) {

    const wm_rect_t panel = wm_taskbar_session_rect();
    const wm_rect_t power = wm_taskbar_power_rect();

    if (wm_taskbar_session_open() && !wm_rect_contains_point(&panel, x, y) && !wm_rect_contains_point(&power, x, y)) {
        wm_taskbar_session(false);
    }
}


/**
 * @brief Closes the session menu, which is what Escape does while it is out.
 *
 * @return true when it was out.
 */
bool wm_taskbar_dismiss(void) {

    if (!wm_taskbar_session_open()) {
        return false;
    }

    wm_taskbar_session(false);

    return true;
}


/**
 * @brief Acts on a release over the item the press went down on: open or close the launcher or the session menu,
 * restart or power off, or show, raise or hide a window.
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

    if (pressed == WM_TASKBAR_POWER) {

        wm_taskbar_session(!wm_taskbar_session_open());

        return true;
    }

    const int row = wm_taskbar_session_index(pressed);

    if (row >= 0) {

        wm_taskbar_session(false);
        wm_spawn(wm_taskbar_session_rows[row].argv);

        return true;
    }

    if (pressed == WM_TASKBAR_LAUNCHER) {

        if (wm.drawer.window) {
            wm_window_request_close(wm.drawer.window);
        } else {
            wm_spawn(wm_taskbar_command);
        }

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
 * @brief Lets go of a window that is going away, so that no item is left lit, pressed or labelled for it.
 *
 * @param win The window.
 */
void wm_taskbar_forget(const wm_window_t* win) {

    if (wm.taskbar.hover == win->id) {
        wm.taskbar.hover = 0;
        wm_taskbar_popout(0);
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

    ui_draw_rounded_rect_d(cr, r->x, r->y, r->width, r->height, WM_TASKBAR_ITEM_RADIUS);
    cairo_fill(cr);
}


/**
 * @brief Draws an icon already scaled to the size it is shown at, which makes it a plain copy.
 *
 * @param cr The cairo context.
 * @param icon The icon, or NULL for nothing.
 * @param x The left edge.
 * @param y The top edge.
 * @param alpha How opaque to draw it.
 */
static void wm_taskbar_paint_icon(cairo_t* cr, cairo_surface_t* icon, int x, int y, double alpha) {

    if (!icon) {
        return;
    }

    cairo_save(cr);

    cairo_rectangle(cr, x, y, cairo_image_surface_get_width(icon), cairo_image_surface_get_height(icon));
    cairo_clip(cr);

    cairo_set_source_surface(cr, icon, x, y);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
    cairo_paint_with_alpha(cr, alpha);

    cairo_restore(cr);
}


/**
 * @brief Draws the launcher's mark: a grid of dots whose middle cross is the accent, turning into a close cross as the launcher opens.
 *
 * @param cr The cairo context.
 * @param r The launcher's button.
 * @param open How far open the launcher is, 0 to 1.
 */
static void wm_taskbar_paint_launcher(cairo_t* cr, const wm_rect_t* r, double open) {

    const double grey[4] = {WM_COLOR_TASKBAR_MARK};

    cairo_save(cr);
    cairo_translate(cr, r->x + r->width / 2.0, r->y + r->height / 2.0);

    if (open < 1.0) {

        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {

                if (i && j) {
                    cairo_new_sub_path(cr);
                    cairo_arc(cr, i * WM_TASKBAR_LAUNCHER_PITCH, j * WM_TASKBAR_LAUNCHER_PITCH, WM_TASKBAR_LAUNCHER_DOT, 0.0, 2.0 * M_PI);
                }
            }
        }

        cairo_set_source_rgba(cr, grey[0], grey[1], grey[2], grey[3] * (1.0 - open));
        cairo_fill(cr);
    }


    cairo_rotate(cr, open * M_PI / 4.0);

    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {

            if (!i || !j) {
                cairo_new_sub_path(cr);
                cairo_arc(cr, i * WM_TASKBAR_LAUNCHER_PITCH, j * WM_TASKBAR_LAUNCHER_PITCH, WM_TASKBAR_LAUNCHER_DOT, 0.0, 2.0 * M_PI);
            }
        }
    }

    cairo_set_source_rgb(cr, WM_COLOR_ACCENT);
    cairo_fill(cr);

    cairo_restore(cr);
}


/**
 * @brief Draws the power mark: a ring broken at the top, with a stroke down through the gap.
 *
 * @param cr The cairo context, its source already set.
 * @param x The centre.
 * @param y The centre.
 */
static void wm_taskbar_paint_power_glyph(cairo_t* cr, double x, double y) {

    const double g = WM_TASKBAR_SESSION_GLYPH;

    cairo_save(cr);

    cairo_set_line_width(cr, 2.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    cairo_new_path(cr);
    cairo_arc(cr, x, y, g, -M_PI / 2.0 + 0.7, 3.0 * M_PI / 2.0 - 0.7);
    cairo_stroke(cr);

    cairo_move_to(cr, x, y - g - 1.5);
    cairo_line_to(cr, x, y - 1.0);
    cairo_stroke(cr);

    cairo_restore(cr);
}


/**
 * @brief Draws the restart mark: a ring turning clockwise, open at the top where its arrowhead points along.
 *
 * @param cr The cairo context, its source already set.
 * @param x The centre.
 * @param y The centre.
 */
static void wm_taskbar_paint_restart_glyph(cairo_t* cr, double x, double y) {

    const double g = WM_TASKBAR_SESSION_GLYPH;

    cairo_save(cr);

    cairo_set_line_width(cr, 2.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    cairo_new_path(cr);
    cairo_arc(cr, x, y, g, -M_PI / 2.0 + 0.6, 3.0 * M_PI / 2.0);
    cairo_stroke(cr);

    cairo_move_to(cr, x - 1.0, y - g - 3.5);
    cairo_line_to(cr, x + 2.5, y - g);
    cairo_line_to(cr, x - 1.0, y - g + 3.5);
    cairo_stroke(cr);

    cairo_restore(cr);
}


/**
 * @brief Draws the halt mark: a rounded square, the sign for stop.
 *
 * @param cr The cairo context, its source already set.
 * @param x The centre.
 * @param y The centre.
 */
static void wm_taskbar_paint_halt_glyph(cairo_t* cr, double x, double y) {

    const double g = WM_TASKBAR_SESSION_GLYPH - 1.0;

    cairo_save(cr);

    cairo_set_line_width(cr, 2.0);

    cairo_new_path(cr);
    ui_draw_rounded_rect_d(cr, x - g, y - g, 2.0 * g, 2.0 * g, 2.5);
    cairo_stroke(cr);

    cairo_restore(cr);
}


/**
 * @brief Draws the power button at the foot of the rail, its mark in red and its wash held while the session menu is out.
 *
 * @param cr The cairo context.
 */
static void wm_taskbar_paint_power(cairo_t* cr) {

    const wm_rect_t r = wm_taskbar_power_rect();

    const bool held   = wm.taskbar.pressed == WM_TASKBAR_POWER && wm.taskbar.hover == WM_TASKBAR_POWER;
    const double open = ui_anim_value(&wm.taskbar.session.open, wm.frame_time);

    wm_taskbar_paint_wash(cr, &r, ui_anim_value(&wm.taskbar.hover_power, wm.frame_time), held, open);

    cairo_set_source_rgb(cr, WM_COLOR_TASKBAR_POWER);
    wm_taskbar_paint_power_glyph(cr, r.x + r.width / 2.0, r.y + r.height / 2.0);
}


/**
 * @brief Draws one window's entry: its icon, and a mark against the rail's edge.
 *
 * The mark is a short grey one for a window in the background and grows into the accent for the active one.
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

    wm_taskbar_paint_icon(cr, win->icon, r->x + (WM_TASKBAR_ITEM - WM_ICON_SIZE) / 2, r->y + (WM_TASKBAR_ITEM - WM_ICON_SIZE) / 2, ui_lerp(1.0, WM_ICON_IDLE, gone));


    const double grey[4]   = {WM_COLOR_TASKBAR_MARK};
    const double accent[3] = {WM_COLOR_ACCENT};

    const double height = ui_lerp(8.0, 20.0, active);

    cairo_set_source_rgba(cr, ui_lerp(grey[0], accent[0], active), ui_lerp(grey[1], accent[1], active), ui_lerp(grey[2], accent[2], active), ui_lerp(grey[3], 1.0, active));

    ui_draw_rounded_rect_d(cr, (WM_TASKBAR_PAD - WM_TASKBAR_MARK) / 2.0, r->y + (r->height - height) / 2.0, WM_TASKBAR_MARK, height, WM_TASKBAR_MARK / 2.0);
    cairo_fill(cr);
}


/**
 * @brief Draws the active window's title along a free stretch of the rail, turned to read upwards.
 *
 * @param cr The cairo context.
 * @param top Where the stretch starts.
 * @param bottom Where it ends.
 */
static void wm_taskbar_paint_title(cairo_t* cr, int top, int bottom) {

    const wm_window_t* win = wm.focused;

    cairo_font_face_t* face = ui_font_face(WM_FONT_PATH);

    const double length = bottom - top - 4 * WM_TASKBAR_GAP;

    if (!win || !wm_window_listed(win) || !win->title[0] || !face || length < 48.0) {
        return;
    }


    char title[UI_TITLE_MAX * 2];

    ui_draw_ellipsize(title, sizeof(title), win->title, WM_FONT_PATH, WM_TASKBAR_FONT_SIZE, length);


    cairo_text_extents_t extents;
    cairo_font_extents_t font;

    cairo_save(cr);

    cairo_set_font_face(cr, face);
    cairo_set_font_size(cr, WM_TASKBAR_FONT_SIZE);
    cairo_text_extents(cr, title, &extents);
    cairo_font_extents(cr, &font);

    cairo_translate(cr, round(WM_TASKBAR_WIDTH / 2.0 + (font.ascent - font.descent) / 2.0), round((top + bottom + extents.x_advance) / 2.0));
    cairo_rotate(cr, -M_PI / 2.0);

    cairo_set_source_rgb(cr, WM_COLOR_TITLE_IDLE);
    cairo_move_to(cr, 0.0, 0.0);
    cairo_show_text(cr, title);

    cairo_restore(cr);
}


/**
 * @brief Draws the time at the foot of the rail, the hours over the minutes, in the local time zone.
 *
 * @param cr The cairo context.
 */
static void wm_taskbar_paint_clock(cairo_t* cr) {

    cairo_font_face_t* face = ui_font_face(WM_FONT_PATH);

    if (!face) {
        return;
    }


    const time_t now = time(NULL);

    struct tm tm;

    if (!localtime_r(&now, &tm)) {
        return;
    }

    char lines[2][4];

    strftime(lines[0], sizeof(lines[0]), "%H", &tm);
    strftime(lines[1], sizeof(lines[1]), "%M", &tm);


    const wm_rect_t r = wm_taskbar_clock_rect();

    cairo_font_extents_t font;

    cairo_set_font_face(cr, face);
    cairo_set_font_size(cr, WM_TASKBAR_CLOCK_SIZE);
    cairo_font_extents(cr, &font);

    cairo_set_source_rgb(cr, WM_COLOR_TITLE_ACTIVE);

    for (size_t i = 0; i < WM_ARRAY_COUNT(lines); i++) {

        cairo_text_extents_t extents;

        cairo_text_extents(cr, lines[i], &extents);

        const double x = r.x + (r.width - extents.x_advance) / 2.0;
        const double y = r.y + r.height * (2.0 * i + 1.0) / 4.0 + (font.ascent - font.descent) / 2.0;

        cairo_move_to(cr, round(x), round(y));
        cairo_show_text(cr, lines[i]);
    }
}


/**
 * @brief Draws the popout out of the rail beside the hovered item, its corners flaring into the rail's edge.
 *
 * @param cr The cairo context.
 */
static void wm_taskbar_paint_popout(cairo_t* cr) {

    wm.taskbar.popout.painted = wm_taskbar_popout_extent(wm.frame_time);

    const double open   = ui_anim_value(&wm.taskbar.popout.open, wm.frame_time);
    const double full   = ui_anim_value(&wm.taskbar.popout.width, wm.frame_time);
    const double center = ui_anim_value(&wm.taskbar.popout.center, wm.frame_time);

    const double x0 = WM_TASKBAR_WIDTH;
    const double x1 = x0 + full * open;

    if (x1 - x0 < 1.0) {
        return;
    }


    const double top    = round(center - WM_TASKBAR_ITEM / 2.0);
    const double bottom = top + WM_TASKBAR_ITEM;

    const double fillet = WM_MIN(WM_TASKBAR_POPOUT_RADIUS, x1 - x0);
    const double corner = WM_MIN(WM_TASKBAR_POPOUT_RADIUS, (x1 - x0) / 2.0);

    cairo_save(cr);

    cairo_new_path(cr);
    cairo_move_to(cr, x0, top - fillet);
    cairo_arc_negative(cr, x0 + fillet, top - fillet, fillet, M_PI, M_PI / 2.0);
    cairo_arc(cr, x1 - corner, top + corner, corner, -M_PI / 2.0, 0.0);
    cairo_arc(cr, x1 - corner, bottom - corner, corner, 0.0, M_PI / 2.0);
    cairo_arc_negative(cr, x0 + fillet, bottom + fillet, fillet, 3.0 * M_PI / 2.0, M_PI);
    cairo_close_path(cr);

    cairo_set_source_rgba(cr, WM_COLOR_TASKBAR);
    cairo_fill_preserve(cr);
    cairo_clip(cr);


    cairo_font_face_t* face = ui_font_face(WM_FONT_PATH);

    if (face) {

        cairo_font_extents_t font;

        cairo_set_font_face(cr, face);
        cairo_set_font_size(cr, WM_TASKBAR_FONT_SIZE);
        cairo_font_extents(cr, &font);

        const double x = x1 - full + WM_TASKBAR_POPOUT_PAD;
        const double y = top + (WM_TASKBAR_ITEM - (font.ascent + font.descent)) / 2.0 + font.ascent;

        cairo_set_source_rgba(cr, WM_COLOR_TITLE_ACTIVE, open);
        cairo_move_to(cr, round(x), round(y));
        cairo_show_text(cr, wm.taskbar.popout.text);
    }

    cairo_restore(cr);
}


/**
 * @brief Draws one row of the session menu: its wash, its mark and what it does.
 *
 * @param cr The cairo context, clipped to the menu.
 * @param index The row's place in wm_taskbar_session_rows.
 * @param shift How far the menu still is from all the way out, which the row is slid back by.
 * @param alpha How opaque to draw the mark and the text.
 */
static void wm_taskbar_paint_session_row(cairo_t* cr, int index, int shift, double alpha) {

    const uint32_t item = wm_taskbar_session_rows[index].item;

    wm_rect_t r = wm_taskbar_session_row(index);

    r.x -= shift;

    const bool held = wm.taskbar.pressed == item && wm.taskbar.hover == item;

    wm_taskbar_paint_wash(cr, &r, ui_anim_value(&wm.taskbar.session.hover[index], wm.frame_time), held, 0.0);


    const double cx = r.x + WM_TASKBAR_ITEM / 2.0;
    const double cy = r.y + r.height / 2.0;

    if (item == WM_TASKBAR_SHUTDOWN) {
        cairo_set_source_rgba(cr, WM_COLOR_TASKBAR_POWER, alpha);
        wm_taskbar_paint_power_glyph(cr, cx, cy);
    } else if (item == WM_TASKBAR_RESTART) {
        cairo_set_source_rgba(cr, WM_COLOR_TITLE_ACTIVE, alpha);
        wm_taskbar_paint_restart_glyph(cr, cx, cy);
    } else {
        cairo_set_source_rgba(cr, WM_COLOR_TITLE_ACTIVE, alpha);
        wm_taskbar_paint_halt_glyph(cr, cx, cy);
    }


    cairo_font_face_t* face = ui_font_face(WM_FONT_PATH);

    if (!face) {
        return;
    }

    cairo_font_extents_t font;

    cairo_set_font_face(cr, face);
    cairo_set_font_size(cr, WM_TASKBAR_FONT_SIZE);
    cairo_font_extents(cr, &font);

    const double y = r.y + (r.height - (font.ascent + font.descent)) / 2.0 + font.ascent;

    cairo_set_source_rgba(cr, WM_COLOR_TITLE_ACTIVE, alpha);
    cairo_move_to(cr, r.x + WM_TASKBAR_ITEM, round(y));
    cairo_show_text(cr, wm_taskbar_session_rows[index].label);
}


/**
 * @brief Draws the session menu sliding out of the rail, flaring into it at the top and into the bottom border at its right.
 *
 * @param cr The cairo context.
 */
static void wm_taskbar_paint_session(cairo_t* cr) {

    wm.taskbar.session.painted = wm_taskbar_session_extent(wm.frame_time);

    const double open     = ui_anim_value(&wm.taskbar.session.open, wm.frame_time);
    const wm_rect_t panel = wm_taskbar_session_rect();

    const double x0 = panel.x;
    const double x1 = x0 + panel.width * open;

    if (x1 - x0 < 1.0) {
        return;
    }


    const double top    = panel.y;
    const double bottom = panel.y + panel.height;

    const double fillet = WM_MIN(WM_TASKBAR_POPOUT_RADIUS, x1 - x0);
    const double corner = WM_MIN(WM_TASKBAR_POPOUT_RADIUS, (x1 - x0) / 2.0);

    cairo_save(cr);

    cairo_new_path(cr);
    cairo_move_to(cr, x0, top - fillet);
    cairo_arc_negative(cr, x0 + fillet, top - fillet, fillet, M_PI, M_PI / 2.0);
    cairo_arc(cr, x1 - corner, top + corner, corner, -M_PI / 2.0, 0.0);
    cairo_arc_negative(cr, x1 + fillet, bottom - fillet, fillet, M_PI, M_PI / 2.0);
    cairo_line_to(cr, x0, bottom);
    cairo_close_path(cr);

    cairo_set_source_rgba(cr, WM_COLOR_TASKBAR);
    cairo_fill_preserve(cr);
    cairo_clip(cr);

    const int shift = (int)round(panel.width - (x1 - x0));

    for (int i = 0; i < WM_TASKBAR_SESSION_ROWS; i++) {
        wm_taskbar_paint_session_row(cr, i, shift, open);
    }

    cairo_restore(cr);
}


/**
 * @brief Reports whether any damage reaches the taskbar: the rail, the border, a corner it rounds off, the popout or the session menu.
 *
 * @param damage The damaged rectangles.
 * @param count How many there are.
 * @return true when the taskbar has to be drawn.
 */
static bool wm_taskbar_visible(const wm_rect_t* damage, size_t count) {

    const int width  = wm.display.width;
    const int height = wm.display.height;
    const int edge   = WM_TASKBAR_BORDER + WM_TASKBAR_RADIUS;

    const wm_rect_t parts[] = {

        {0,            0,             WM_TASKBAR_WIDTH + WM_TASKBAR_RADIUS, height},
        {width - edge, 0,             edge,                                 height},
        {0,            0,             width,                                edge  },
        {0,            height - edge, width,                                edge  },

        wm.taskbar.popout.painted,
        wm_taskbar_popout_extent(wm.frame_time),
        wm.taskbar.session.painted,
        wm_taskbar_session_extent(wm.frame_time),
    };

    for (size_t i = 0; i < WM_ARRAY_COUNT(parts); i++) {

        if (parts[i].width <= 0 || parts[i].height <= 0) {
            continue;
        }

        for (size_t j = 0; j < count; j++) {

            if (wm_rect_intersects(&parts[i], &damage[j])) {
                return true;
            }
        }
    }

    return false;
}


/**
 * @brief Draws the taskbar over everything else on the display but the pointer.
 *
 * @param cr The cairo context to draw with, already clipped to the damage.
 * @param damage The damaged rectangles.
 * @param count How many there are.
 */
void wm_taskbar_paint(cairo_t* cr, const wm_rect_t* damage, size_t count) {

    if (!wm_taskbar_visible(damage, count)) {
        return;
    }


    const wm_rect_t work = wm_workarea();

    cairo_save(cr);

    cairo_new_path(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);

    cairo_set_source_rgba(cr, WM_COLOR_TASKBAR);
    cairo_rectangle(cr, 0, 0, wm.display.width, wm.display.height);
    ui_draw_rounded_rect_d(cr, work.x, work.y, work.width, work.height, WM_TASKBAR_RADIUS);
    cairo_fill(cr);

    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);

    wm_taskbar_paint_session(cr);
    wm_taskbar_paint_popout(cr);


    const wm_rect_t launcher = wm_taskbar_launcher_rect();

    const bool held   = wm.taskbar.pressed == WM_TASKBAR_LAUNCHER && wm.taskbar.hover == WM_TASKBAR_LAUNCHER;
    const double open = ui_anim_value(&wm.drawer.open, wm.frame_time);

    wm_taskbar_paint_wash(cr, &launcher, ui_anim_value(&wm.taskbar.hover_launcher, wm.frame_time), held, open);
    wm_taskbar_paint_launcher(cr, &launcher, open);


    wm_window_t* windows[WM_TASKBAR_ENTRIES_MAX];

    const size_t entries = wm_taskbar_windows(windows);

    int below = launcher.y + launcher.height;

    for (size_t i = 0; i < entries; i++) {

        const wm_rect_t r = wm_taskbar_slot(i);

        if (r.width <= 0) {
            break;
        }

        wm_taskbar_paint_entry(cr, windows[i], &r);

        below = r.y + r.height;
    }


    const wm_rect_t clock = wm_taskbar_clock_rect();

    wm_taskbar_paint_title(cr, below, clock.y);
    wm_taskbar_paint_clock(cr);
    wm_taskbar_paint_power(cr);

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

    for (int i = 0; i < 2; i++) {

        if (wm_clock_pipe[i] >= 0) {
            close(wm_clock_pipe[i]);
            wm_clock_pipe[i] = -1;
        }
    }
}
