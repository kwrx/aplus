/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2026 Antonino Natale
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

#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <aplus/events.h>
#include <aplus/input.h>
#include <aplus/ulog.h>

#include <wm.h>


#define WM_BENCH_FRAMES      120
#define WM_BENCH_REPEAT      8
#define WM_BENCH_ANIM_MAX    256
#define WM_BENCH_SAMPLES_MAX 16384
#define WM_BENCH_SLOTS       2


typedef struct {

    const char* name;
    const char* about;
    void (*run)(void);

} wm_bench_case_t;


typedef struct {

    const char* title;
    const char* icon;
    int x;
    int y;
    int width;
    int height;
    uint32_t flags;

} wm_bench_spec_t;


static struct {

    int frames;
    int repeat;

    int input[2];
    wm_client_t* client;

    uint64_t clock;
    bool hwcursor;

    int slot;
    uint64_t* samples[WM_BENCH_SLOTS];
    size_t count[WM_BENCH_SLOTS];

} bench;


static const wm_bench_spec_t wm_bench_apps[] = {
    {"Terminal",   "aplus-terminal",   40,  60,  520, 550, 0},
    {"Files",      "aplus-explorer",   340, 160, 560, 620, 0},
    {"Calculator", "aplus-calculator", 740, 80,  260, 600, 0},
    {"Editor",     "aplus-editor",     120, 360, 500, 560, 0},
    {"Settings",   "aplus-settings",   420, 340, 500, 560, 0},
};

static const wm_bench_spec_t wm_bench_launcher = {"Launcher", "aplus-launcher", -1, -1, 500, 550, UI_WINDOW_BORDERLESS | UI_WINDOW_CENTERED | UI_WINDOW_TRANSLUCENT | UI_WINDOW_BLUR};

static const wm_bench_spec_t wm_bench_popup = {"Bench", "aplus-viewer", -1, -1, 500, 520, 0};


static uint64_t wm_bench_ns(void) {

    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}


static void wm_bench_into(int slot) {

    bench.slot = slot;
}


static void wm_bench_record(uint64_t ns) {

    if (bench.slot < 0 || bench.count[bench.slot] >= WM_BENCH_SAMPLES_MAX) {
        return;
    }

    bench.samples[bench.slot][bench.count[bench.slot]++] = ns;
}


static int wm_bench_compare(const void* a, const void* b) {

    const uint64_t x = *(const uint64_t*)a;
    const uint64_t y = *(const uint64_t*)b;

    return x < y ? -1 : x > y;
}


static void wm_bench_report(int slot, const char* name) {

    const size_t count = bench.count[slot];
    uint64_t* samples  = bench.samples[slot];

    bench.count[slot] = 0;

    if (!count) {
        printf("%-16s %7s\n", name, "-");
        return;
    }

    qsort(samples, count, sizeof(*samples), wm_bench_compare);

    uint64_t total = 0;

    for (size_t i = 0; i < count; i++) {
        total += samples[i];
    }

    const size_t p95 = WM_MIN((count * 95 + 99) / 100, count) - 1;

    printf("%-16s %7zu %9.3f %9.3f %9.3f %9.3f %9.3f\n", name, count, (double)total / count / 1e6, samples[count / 2] / 1e6, samples[p95] / 1e6, samples[count - 1] / 1e6, total / 1e6);
}


static void wm_bench_reset_client(void) {

    bench.client->tx.head = 0;
    bench.client->tx.size = 0;
}


static int wm_bench_frame(void) {

    bench.clock = WM_MAX(bench.clock + UI_ANIM_FRAME_MS, ui_anim_now());

    while (ui_anim_now() < bench.clock) {
    }

    wm.frame_time = bench.clock;


    const uint64_t start = wm_bench_ns();

    const int next = wm_animate(wm.frame_time);

    const bool drawn = wm.damage.count || wm.glass_damage.count;

    wm_composite();

    const uint64_t spent = wm_bench_ns() - start;


    if (drawn) {
        wm_bench_record(spent);
    }

    wm_bench_reset_client();

    return next;
}


static void wm_bench_animate(void) {

    for (int i = 0; i < WM_BENCH_ANIM_MAX; i++) {

        if (wm_bench_frame() < 0) {
            break;
        }
    }
}


static void wm_bench_settle(void) {

    const int slot = bench.slot;

    wm_bench_into(-1);
    wm_bench_animate();
    wm_bench_into(slot);
}


static void wm_bench_send(uint8_t type, int a, int b) {

    event_t ev;

    memset(&ev, 0, sizeof(ev));

    ev.ev_type = type;

    if (type == EV_ABS) {
        ev.ev_abs.x = (vaxis_t)a;
        ev.ev_abs.y = (vaxis_t)b;
    } else {
        ev.ev_key.vkey = (vkey_t)a;
        ev.ev_key.down = (uint8_t)b;
    }

    if (write(bench.input[1], &ev, sizeof(ev)) != (ssize_t)sizeof(ev)) {
        fprintf(stderr, "aplus-wm: bench: cannot queue an input event: %s\n", strerror(errno));
        exit(1);
    }

    wm_input_dispatch(bench.input[0]);

    wm_bench_reset_client();
}


static int wm_bench_axis(int value, int size) {

    value = WM_CLAMP(value, 0, size - 1);

    return (int)(((int64_t)value * EV_ABS_MAX + size - 2) / (size - 1));
}


static void wm_bench_pointer(int x, int y) {

    wm_bench_send(EV_ABS, wm_bench_axis(x, wm.display.width), wm_bench_axis(y, wm.display.height));
}


static void wm_bench_button(uint16_t button, bool down) {

    wm_bench_send(EV_KEY, button, down);
}


static void wm_bench_click(uint16_t button, int x, int y) {

    wm.title_click.window_id = 0;

    wm_bench_pointer(x, y);
    wm_bench_button(button, true);
    wm_bench_button(button, false);
}


static void wm_bench_click_rect(uint16_t button, wm_rect_t r) {

    wm_bench_click(button, r.x + r.width / 2, r.y + r.height / 2);
}


static void wm_bench_park(void) {

    wm_bench_pointer(wm.display.width - 1, wm.display.height - 1);
}


static bool wm_bench_spot(wm_window_t* win, wm_region_t region, int* x, int* y) {

    const wm_rect_t f = wm_window_frame(win);

    for (int v = 0; v < 32; v++) {

        for (int u = 0; u < 32; u++) {

            const int px = f.x + (f.width * (2 * u + 1)) / 64;
            const int py = region == WM_REGION_TITLEBAR ? f.y + WM_TITLEBAR_HEIGHT / 2 : f.y + (f.height * (2 * v + 1)) / 64;

            wm_window_t* over = NULL;

            if (wm_window_hit_test(px, py, &over) == region && over == win) {
                *x = px;
                *y = py;
                return true;
            }
        }
    }

    return false;
}


static bool wm_bench_corner(wm_window_t* win, int* x, int* y) {

    const wm_rect_t f = wm_window_frame(win);

    for (int k = 0; k < WM_RESIZE_GRIP; k++) {

        wm_window_t* over = NULL;

        const int px = f.x + f.width - 1 - k;
        const int py = f.y + f.height - 1 - k;

        if (wm_window_hit_test(px, py, &over) == WM_REGION_RESIZE_SE && over == win) {
            *x = px;
            *y = py;
            return true;
        }
    }

    return false;
}


static void wm_bench_fill(wm_window_t* win) {

    const double alpha = wm_window_translucent(win) ? 0.55 : 1.0;

    cairo_t* cr = cairo_create(win->backstore);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0.11, 0.11, 0.12, alpha);
    cairo_paint(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    for (int y = 14, i = 0; y + 10 < win->height; y += 22, i++) {

        cairo_set_source_rgba(cr, 0.72 + 0.04 * (i % 3), 0.76, 0.82, 0.85);
        cairo_rectangle(cr, 14, y, (win->width - 28) * (0.3 + 0.07 * ((i * 7) % 10)), 10);
        cairo_fill(cr);
    }

    cairo_destroy(cr);

    cairo_surface_flush(win->backstore);
}


static wm_window_t* wm_bench_window(const wm_bench_spec_t* spec) {

    const wm_rect_t work = wm_workarea();

    wm_window_t* win = wm_window_create(bench.client, work.width * spec->width / 1000, work.height * spec->height / 1000, spec->title, spec->flags);

    if (!win) {
        fprintf(stderr, "aplus-wm: bench: cannot create the %s window: %s\n", spec->title, strerror(errno));
        exit(1);
    }

    wm_window_notify_configure(win);
    wm_window_focus(win);

    wm_bench_fill(win);

    wm_window_set_icon(win, spec->icon);
    wm_window_damage_content(win, 0, 0, win->width, win->height);

    if (spec->x >= 0 && spec->y >= 0) {
        wm_window_move(win, work.x + work.width * spec->x / 1000, work.y + work.height * spec->y / 1000);
    }

    wm_bench_reset_client();

    return win;
}


static void wm_bench_scene_apps(void) {

    for (size_t i = 0; i < WM_ARRAY_COUNT(wm_bench_apps); i++) {
        wm_bench_window(&wm_bench_apps[i]);
    }
}


static void wm_bench_scene_glass(void) {

    wm_bench_scene_apps();
    wm_bench_window(&wm_bench_launcher);
}


static void wm_bench_damage_all(void) {

    const wm_rect_t all = {0, 0, wm.display.width, wm.display.height};

    wm_damage(&all);
}


static void wm_bench_ready(void) {

    wm_bench_park();
    wm_bench_damage_all();
    wm_bench_settle();
}


static void wm_bench_clear(void) {

    wm_bench_into(-1);

    wm_menu_hide();
    wm_taskbar_dismiss();

    while (wm.windows) {
        wm_window_destroy(wm.windows);
    }

    wm.display.hwcursor = bench.hwcursor;

    wm_bench_park();
    wm_bench_damage_all();
    wm_bench_settle();
}


static void wm_bench_warm(void) {

    wm_bench_scene_glass();
    wm_bench_ready();

    for (int i = 0; i < 30; i++) {
        wm_bench_damage_all();
        wm_bench_frame();
    }

    wm_bench_clear();
}


static wm_window_t* wm_bench_find(const char* title) {

    for (wm_window_t* win = wm.windows; win; win = win->next) {

        if (!strcmp(win->title, title)) {
            return win;
        }
    }

    return NULL;
}


static void wm_bench_repaint(const char* name) {

    wm_bench_ready();
    wm_bench_into(0);

    for (int i = 0; i < bench.frames; i++) {
        wm_bench_damage_all();
        wm_bench_frame();
    }

    wm_bench_report(0, name);
}


static void wm_bench_case_full_desktop(void) {

    wm_bench_repaint("full-desktop");
}


static void wm_bench_case_full_apps(void) {

    wm_bench_scene_apps();
    wm_bench_repaint("full-apps");
}


static void wm_bench_case_full_glass(void) {

    wm_bench_scene_glass();
    wm_bench_repaint("full-glass");
}


static void wm_bench_drag(const char* name, wm_window_t* win, int ax, int ay, bool triangle) {

    int x, y;

    if (!wm_bench_spot(win, WM_REGION_TITLEBAR, &x, &y)) {
        printf("%-16s %7s\n", name, "no grab");
        return;
    }


    wm.title_click.window_id = 0;

    wm_bench_pointer(x, y);
    wm_bench_button(BTN_LEFT, true);
    wm_bench_settle();

    wm_bench_into(0);

    for (int i = 1; i <= bench.frames; i++) {

        const double t = (double)i / bench.frames;

        int dx, dy;

        if (triangle) {
            dx = (int)(ax * (1.0 - fabs(1.0 - 2.0 * t)));
            dy = (int)(ay * (1.0 - fabs(1.0 - 2.0 * t)));
        } else {
            dx = (int)(ax * (cos(2.0 * M_PI * t) - 1.0));
            dy = (int)(ay * sin(2.0 * M_PI * t));
        }

        wm_bench_pointer(x + dx, y + dy);
        wm_bench_frame();
    }

    wm_bench_into(-1);

    wm_bench_button(BTN_LEFT, false);
    wm_bench_settle();

    wm_bench_report(0, name);
}


static void wm_bench_case_drag(void) {

    wm_bench_scene_apps();
    wm_bench_ready();

    wm_bench_drag("drag", wm.windows, wm.display.width / 5, wm.display.height / 6, false);
}


static void wm_bench_case_drag_edge(void) {

    wm_bench_scene_apps();

    wm_window_t* win     = wm.windows;
    const wm_rect_t work = wm_workarea();
    const wm_insets_t in = wm_window_insets(win);

    wm_window_move(win, work.x + in.left, work.y + in.top);

    wm_bench_ready();

    wm_bench_drag("drag-edge", win, 0, work.height - win->height - in.top - in.bottom, true);
}


static void wm_bench_case_resize(void) {

    wm_bench_scene_apps();
    wm_bench_ready();

    int x, y;

    if (!wm_bench_corner(wm.windows, &x, &y)) {
        printf("%-16s %7s\n", "resize", "no grip");
        return;
    }

    wm_bench_pointer(x, y);
    wm_bench_button(BTN_LEFT, true);
    wm_bench_settle();

    wm_bench_into(0);

    for (int i = 1; i <= bench.frames; i++) {

        const double t = 2.0 * M_PI * i / bench.frames;

        wm_bench_pointer(x - (int)(wm.display.width / 8 * (1.0 - cos(t))), y - (int)(wm.display.height / 8 * sin(t)));
        wm_bench_frame();
    }

    wm_bench_into(-1);

    wm_bench_button(BTN_LEFT, false);
    wm_window_notify_configure(wm.windows);
    wm_bench_settle();

    wm_bench_report(0, "resize");
}


static void wm_bench_commit(wm_window_t* win, int x, int y, int width, int height) {

    wm_window_damage_content(win, x, y, width, height);

    const wm_rect_t r = {win->x + x, win->y + y, width, height};

    wm_damage(&r);
}


static void wm_bench_commits(const char* name, wm_window_t* win, int y, int height) {

    wm_bench_ready();
    wm_bench_into(0);

    for (int i = 0; i < bench.frames; i++) {
        wm_bench_commit(win, 0, y, win->width, height);
        wm_bench_frame();
    }

    wm_bench_report(0, name);
}


static void wm_bench_case_commit_full(void) {

    wm_bench_scene_apps();
    wm_bench_commits("commit-full", wm.windows, 0, wm.windows->height);
}


static void wm_bench_case_commit_line(void) {

    wm_bench_scene_apps();
    wm_bench_commits("commit-line", wm.windows, wm.windows->height - 40, 20);
}


static void wm_bench_case_commit_glass(void) {

    wm_bench_scene_glass();

    wm_window_t* win = wm_bench_find("Settings");

    wm_bench_commits("commit-glass", win, 0, win->height);
}


static void wm_bench_case_pointer(void) {

    wm_bench_scene_apps();

    wm.display.hwcursor = false;

    wm_cursor_set(WM_CURSOR_ARROW);
    wm_bench_ready();

    wm_bench_into(0);

    const int cx = wm.display.width / 2;
    const int cy = wm.display.height / 2;

    for (int i = 1; i <= bench.frames; i++) {

        const double t = 2.0 * M_PI * i / bench.frames;

        wm_bench_pointer(cx + (int)(cx * 0.8 * sin(t)), cy + (int)(cy * 0.8 * sin(2.0 * t)));
        wm_bench_frame();
    }

    wm_bench_report(0, "pointer-sw");
}


static void wm_bench_case_rail(void) {

    wm_bench_scene_apps();
    wm_bench_ready();

    wm_bench_into(0);

    const int span = wm.display.height - 2 * WM_TASKBAR_PAD;

    for (int i = 1; i <= bench.frames; i++) {

        const double t = (double)i / bench.frames;

        wm_bench_pointer(WM_TASKBAR_WIDTH / 2, WM_TASKBAR_PAD + (int)(span * (1.0 - fabs(1.0 - 2.0 * t))));
        wm_bench_frame();
    }

    wm_bench_report(0, "rail-hover");
}


static void wm_bench_case_open(void) {

    wm_bench_scene_apps();
    wm_bench_ready();

    wm_window_t* opened[64];

    const int count = WM_MIN(bench.repeat, (int)WM_ARRAY_COUNT(opened));

    for (int i = 0; i < count; i++) {

        wm_bench_into(0);

        opened[i] = wm_bench_window(&wm_bench_popup);

        wm_bench_animate();
    }

    for (int i = count - 1; i >= 0; i--) {

        wm_bench_into(1);

        wm_window_destroy(opened[i]);

        wm_bench_animate();
    }

    wm_bench_into(-1);

    wm_bench_report(0, "open");
    wm_bench_report(1, "close");
}


static void wm_bench_case_maximize(void) {

    wm_bench_scene_apps();
    wm_bench_ready();

    wm_window_t* win = wm.windows;

    for (int i = 0; i < bench.repeat; i++) {

        for (int slot = 0; slot < 2; slot++) {

            wm_bench_into(slot);
            wm_bench_click_rect(BTN_LEFT, wm_window_button_rect(win, WM_REGION_MAXIMIZE));
            wm_bench_animate();
        }
    }

    wm_bench_into(-1);

    wm_bench_report(0, "maximize");
    wm_bench_report(1, "unmaximize");
}


static void wm_bench_case_minimize(void) {

    wm_bench_scene_apps();
    wm_bench_ready();

    wm_window_t* win = wm.windows;

    for (int i = 0; i < bench.repeat; i++) {

        wm_bench_into(0);
        wm_bench_click_rect(BTN_LEFT, wm_window_button_rect(win, WM_REGION_MINIMIZE));
        wm_bench_animate();

        wm_bench_into(1);
        wm_bench_click_rect(BTN_LEFT, wm_taskbar_entry_rect(win));
        wm_bench_animate();
    }

    wm_bench_into(-1);

    wm_bench_report(0, "minimize");
    wm_bench_report(1, "unminimize");
}


static void wm_bench_case_focus(void) {

    wm_bench_scene_apps();
    wm_bench_ready();

    for (int i = 0; i < 2 * bench.repeat; i++) {

        wm_window_t* below = wm.windows->next;

        int x, y;

        if (!below || !wm_bench_spot(below, WM_REGION_CONTENT, &x, &y)) {
            break;
        }

        wm_bench_into(0);
        wm_bench_click(BTN_LEFT, x, y);
        wm_bench_animate();
    }

    wm_bench_into(-1);

    wm_bench_report(0, "focus");
}


static void wm_bench_case_menu(void) {

    wm_bench_scene_apps();
    wm_bench_ready();

    for (int i = 0; i < bench.repeat; i++) {

        wm_bench_into(0);
        wm_bench_click_rect(BTN_RIGHT, wm_taskbar_entry_rect(wm.windows));
        wm_bench_animate();

        wm_bench_into(1);
        wm_menu_hide();
        wm_bench_animate();
    }

    wm_bench_into(-1);

    wm_bench_report(0, "menu-open");
    wm_bench_report(1, "menu-close");
}


static void wm_bench_case_session(void) {

    wm_bench_scene_apps();
    wm_bench_ready();

    for (int i = 0; i < bench.repeat; i++) {

        for (int slot = 0; slot < 2; slot++) {

            wm_bench_into(slot);
            wm_taskbar_session_toggle();
            wm_bench_animate();
        }
    }

    wm_bench_into(-1);

    wm_bench_report(0, "session-open");
    wm_bench_report(1, "session-close");
}


static void wm_bench_micro(const char* name, void (*fn)(void*), void* arg) {

    wm_bench_into(0);

    for (int i = 0; i < bench.frames; i++) {

        const uint64_t start = wm_bench_ns();

        fn(arg);

        wm_bench_record(wm_bench_ns() - start);
    }

    wm_bench_into(-1);

    wm_bench_report(0, name);
}


static void wm_bench_do_background(void* arg) {

    (void)arg;

    cairo_t* cr = wm.display.cr;

    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source(cr, wm.display.background);
    cairo_paint(cr);
    cairo_restore(cr);
}


static void wm_bench_do_window(void* arg) {

    wm_window_paint(wm.display.cr, (wm_window_t*)arg);
}


static void wm_bench_do_shadow(void* arg) {

    const wm_rect_t f = wm_window_frame((wm_window_t*)arg);
    const int solid   = wm_window_radius((wm_window_t*)arg);

    wm_shadow_paint(wm.display.cr, &wm.shadow_key, &f, WM_SHADOW_KEY_OFFSET, wm.theme.shadow_key, solid);
    wm_shadow_paint(wm.display.cr, &wm.shadow_ambient, &f, 0, wm.theme.shadow_ambient, solid);
}


static void wm_bench_do_taskbar(void* arg) {

    (void)arg;

    const wm_rect_t all = {0, 0, wm.display.width, wm.display.height};

    wm_taskbar_paint(wm.display.cr, &all, 1);
}


static void wm_bench_do_blur(void* arg) {

    wm_blur_t* blur = (wm_blur_t*)arg;

    const wm_rect_t shape = wm_window_frame(wm.windows);

    blur->stale = true;

    wm_blur_capture(blur, &shape);
}


static void wm_bench_do_flush(void* arg) {

    (void)arg;

    const wm_rect_t all = {0, 0, wm.display.width, wm.display.height};

    wm_display_flush(&wm.display, &all, 1);
}


static void wm_bench_case_parts(void) {

    wm_bench_scene_apps();
    wm_bench_ready();

    wm_blur_t blur;

    memset(&blur, 0, sizeof(blur));

    wm_bench_micro("p-background", wm_bench_do_background, NULL);
    wm_bench_micro("p-window", wm_bench_do_window, wm.windows);
    wm_bench_micro("p-shadow", wm_bench_do_shadow, wm.windows);
    wm_bench_micro("p-taskbar", wm_bench_do_taskbar, NULL);
    wm_bench_micro("p-blur", wm_bench_do_blur, &blur);
    wm_bench_micro("p-flush", wm_bench_do_flush, NULL);

    wm_blur_fini(&blur);
}


static const wm_bench_case_t wm_bench_cases[] = {
    {"full-desktop", "repaint the whole screen with no window open",            wm_bench_case_full_desktop},
    {"full-apps",    "repaint the whole screen over five overlapping windows",  wm_bench_case_full_apps   },
    {"full-glass",   "repaint the whole screen with a frosted launcher on top", wm_bench_case_full_glass  },
    {"drag",         "drag the top window around an ellipse by its titlebar",   wm_bench_case_drag        },
    {"drag-edge",    "drag the top window up and down along the rail",          wm_bench_case_drag_edge   },
    {"resize",       "resize the top window by its bottom-right grip",          wm_bench_case_resize      },
    {"commit-full",  "the top window redraws all of itself every frame",        wm_bench_case_commit_full },
    {"commit-line",  "the top window redraws one text line every frame",        wm_bench_case_commit_line },
    {"commit-glass", "a window under the frosted launcher redraws every frame", wm_bench_case_commit_glass},
    {"pointer-sw",   "sweep the software-drawn pointer across the screen",      wm_bench_case_pointer     },
    {"rail-hover",   "sweep the pointer up and down the taskbar rail",          wm_bench_case_rail        },
    {"open",         "open and then close a window, animations included",       wm_bench_case_open        },
    {"maximize",     "maximize and restore the top window from its button",     wm_bench_case_maximize    },
    {"minimize",     "minimize the top window and restore it from the rail",    wm_bench_case_minimize    },
    {"focus",        "click the window below the top one, raising it",          wm_bench_case_focus       },
    {"menu",         "open and close a rail context menu",                      wm_bench_case_menu        },
    {"session",      "open and close the power popout",                         wm_bench_case_session     },
    {"parts",        "time single drawing steps in isolation (p-*)",            wm_bench_case_parts       },
};


static bool wm_bench_selected(const wm_bench_case_t* c, int argc, char** argv) {

    if (optind >= argc) {
        return true;
    }

    for (int i = optind; i < argc; i++) {

        if (!strncmp(c->name, argv[i], strlen(argv[i]))) {
            return true;
        }
    }

    return false;
}


static void wm_bench_usage(void) {

    printf("Use: aplus-wm --bench [options]... [case]...\n"
           "Measure how long aplus-wm takes to composite frames, with synthetic windows and input.\n"
           "A case runs when its name starts with one of the arguments; with none, every case runs.\n\n"
           "   -d, --device                composite onto this framebuffer instead of memory\n"
           "   -s, --size                  memory display size (default: 1280x800)\n"
           "   -n, --frames                frames per case, and calls per part (default: %d)\n"
           "   -r, --repeat                repetitions of each animation (default: %d)\n"
           "   -l, --list                  list the cases and exit\n"
           "       --help                  show this help\n\n"
           "Frames are paced at %d ms like the server's, spinning between them so the CPU stays\n"
           "busy and clocked up; times are milliseconds spent in "
           "wm_animate() and wm_composite(),\n"
           "frames with nothing to draw excluded.\n",
           WM_BENCH_FRAMES, WM_BENCH_REPEAT, UI_ANIM_FRAME_MS);
}


static int wm_bench_display(int width, int height) {

    wm_display_t* display = &wm.display;

    memset(display, 0, sizeof(*display));

    display->fd       = -1;
    display->width    = width;
    display->height   = height;
    display->hwcursor = true;

    display->screen = wm_surface_create(CAIRO_FORMAT_RGB24, width, height);
    display->back   = wm_surface_create(CAIRO_FORMAT_RGB24, width, height);

    if (!display->screen || !display->back) {
        return -1;
    }

    display->cr        = cairo_create(display->back);
    display->cr_screen = cairo_create(display->screen);

    return wm_display_background(display);
}


int wm_bench_main(int argc, char** argv) {

    static struct option long_options[] = {
        {"device", required_argument, NULL, 'd'},
        {"size",   required_argument, NULL, 's'},
        {"frames", required_argument, NULL, 'n'},
        {"repeat", required_argument, NULL, 'r'},
        {"list",   no_argument,       NULL, 'l'},
        {"help",   no_argument,       NULL, 'h'},
        {NULL,     0,                 NULL, 0  }
    };

    const char* device = NULL;

    int width  = 1280;
    int height = 800;

    bench.frames = WM_BENCH_FRAMES;
    bench.repeat = WM_BENCH_REPEAT;


    int c, idx;

    while ((c = getopt_long(argc, argv, "d:s:n:r:lh", long_options, &idx)) != -1) {

        switch (c) {

            case 'd':
                device = optarg;
                break;

            case 's':

                if (sscanf(optarg, "%dx%d", &width, &height) != 2 || width < 320 || height < 240 || width > 8192 || height > 8192) {
                    fprintf(stderr, "aplus-wm: bench: bad size: %s\n", optarg);
                    return 1;
                }

                break;

            case 'n':
                bench.frames = WM_CLAMP(atoi(optarg), 1, WM_BENCH_SAMPLES_MAX);
                break;

            case 'r':
                bench.repeat = WM_CLAMP(atoi(optarg), 1, 64);
                break;

            case 'l':

                for (size_t i = 0; i < WM_ARRAY_COUNT(wm_bench_cases); i++) {
                    printf("%-16s %s\n", wm_bench_cases[i].name, wm_bench_cases[i].about);
                }

                return 0;

            default:
                wm_bench_usage();
                return c == 'h' ? 0 : 1;
        }
    }


    ulog_init("/dev/null", ULOG_ERROR);


    for (int i = 0; i < WM_BENCH_SLOTS; i++) {

        if (!(bench.samples[i] = calloc(WM_BENCH_SAMPLES_MAX, sizeof(uint64_t)))) {
            fprintf(stderr, "aplus-wm: bench: out of memory\n");
            return 1;
        }
    }

    bench.slot = -1;


    wm.next_window_id = 1;
    wm.running        = true;

    wm_theme_init();

    if (device ? wm_display_open(&wm.display, device) < 0 : wm_bench_display(width, height) < 0) {
        fprintf(stderr, "aplus-wm: bench: cannot set up the %s display\n", device ? device : "memory");
        return 1;
    }

    ui_font_face(wm.theme.font);

    if (wm_shadow_init(&wm.shadow_key, WM_SHADOW_KEY_BLUR, UI_WINDOW_RADIUS) < 0 || wm_shadow_init(&wm.shadow_ambient, WM_SHADOW_AMBIENT_BLUR, UI_WINDOW_RADIUS) < 0) {
        fprintf(stderr, "aplus-wm: bench: cannot render the window shadows\n");
        return 1;
    }

    wm_pins_load();


    if (socketpair(AF_UNIX, SOCK_STREAM, 0, bench.input) < 0) {
        fprintf(stderr, "aplus-wm: bench: socketpair() failed: %s\n", strerror(errno));
        return 1;
    }

    if (!(bench.client = calloc(1, sizeof(wm_client_t)))) {
        fprintf(stderr, "aplus-wm: bench: out of memory\n");
        return 1;
    }

    bench.client->fd      = -1;
    bench.client->pid     = getpid();
    bench.client->hello   = true;
    bench.client->version = UI_PROTOCOL_VERSION;

    wm.clients = bench.client;


    wm.pointer.x = wm.display.width / 2;
    wm.pointer.y = wm.display.height / 2;

    wm_display_cursor_move(&wm.display, wm.pointer.x, wm.pointer.y);

    bench.hwcursor = wm.display.hwcursor;

    bench.clock   = ui_anim_now();
    wm.frame_time = bench.clock;


    printf("aplus-wm bench: %dx%d %s display, %s cursor, %d frames, %d repeats\n", wm.display.width, wm.display.height, device ? device : "memory", wm.display.hwcursor ? "hardware" : "software", bench.frames, bench.repeat);
    printf("%-16s %7s %9s %9s %9s %9s %9s\n", "case", "frames", "mean ms", "p50 ms", "p95 ms", "max ms", "total ms");

    wm_bench_ready();
    wm_bench_warm();

    const uint64_t start = wm_bench_ns();

    for (size_t i = 0; i < WM_ARRAY_COUNT(wm_bench_cases); i++) {

        if (!wm_bench_selected(&wm_bench_cases[i], argc, argv)) {
            continue;
        }

        wm_bench_cases[i].run();
        wm_bench_clear();
    }

    printf("done in %.1f s\n", (wm_bench_ns() - start) / 1e9);


    wm.clients = NULL;

    free(bench.client);

    close(bench.input[0]);
    close(bench.input[1]);

    wm_ghost_fini();
    wm_menu_fini();
    wm_shadow_fini(&wm.shadow_key);
    wm_shadow_fini(&wm.shadow_ambient);
    wm_stroke_fini();

    for (int i = 0; i < WM_BENCH_SLOTS; i++) {
        free(bench.samples[i]);
    }

    return 0;
}
