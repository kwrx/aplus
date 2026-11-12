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
#include <stdlib.h>

#include <aplus/ulog.h>

#include <wm.h>


/**
 * @brief Adds a rectangle to what the next frame repaints, clipped to the screen.
 *
 * Which rectangles are worth merging into one is ui_damage_add()'s decision, the same one the
 * clients make about their own surfaces.
 *
 * @param rect The rectangle that has to be repainted.
 */
void wm_damage(const wm_rect_t* rect) {

    ui_damage_add(&wm.damage, ui_rect_clip(rect, wm.display.width, wm.display.height));
}


void wm_damage_glass(const wm_rect_t* rect) {

    ui_damage_add(&wm.glass_damage, ui_rect_clip(rect, wm.display.width, wm.display.height));
}


/**
 * @brief Repaints a window's shadow rectangle, which reaches past the frame on every side.
 *
 * @param win The window to repaint.
 */
void wm_damage_window(const wm_window_t* win) {

    const wm_rect_t shadow = wm_window_shadow_rect(win);

    wm_damage(&shadow);
}


void wm_damage_window_edges(const wm_window_t* win) {

    const wm_rect_t s     = wm_window_shadow_rect(win);
    const wm_rect_t f     = wm_window_frame(win);
    const wm_insets_t in  = wm_window_insets(win);
    const int corner      = WM_MAX(wm_window_radius(win), 3);
    const int top         = WM_MAX(in.top, corner);
    const int bottom      = WM_MAX(in.bottom, corner);
    const int left        = WM_MAX(in.left, 3);
    const int right       = WM_MAX(in.right, 3);

    const wm_rect_t strips[] = {
        {s.x,                    s.y,                       s.width,                                  f.y - s.y + top                          },
        {s.x,                    f.y + f.height - bottom,   s.width,                                  s.y + s.height - (f.y + f.height) + bottom},
        {s.x,                    f.y + top,                 f.x - s.x + left,                         f.height - top - bottom                  },
        {f.x + f.width - right,  f.y + top,                 s.x + s.width - (f.x + f.width) + right,  f.height - top - bottom                  },
    };

    for (size_t i = 0; i < WM_ARRAY_COUNT(strips); i++) {
        wm_damage(&strips[i]);
    }
}


/**
 * @brief Reports whether some window paints over the whole of a rectangle anyway.
 *
 * Only the part of a content area clear of the frame's rounded corners counts, and only a
 * window that is solid right now covers anything: see wm_window_opaque().
 *
 * @param rect The rectangle to test.
 * @return true when the desktop behind it never becomes visible.
 */
static bool wm_damage_is_covered(const wm_rect_t* rect, const wm_window_t* below) {

    bool covered = false;

    for (wm_window_t* win = wm.windows; win && win != below; win = win->next) {

        if (wm_window_drawer(win)) {
            continue;
        }

        if (wm_window_frosted(win)) {

            const wm_rect_t frame = wm_window_frame(win);
            const wm_rect_t reach = {frame.x - WM_BLUR_RADIUS, frame.y - WM_BLUR_RADIUS, frame.width + 2 * WM_BLUR_RADIUS, frame.height + 2 * WM_BLUR_RADIUS};

            if (ui_rect_intersects(&reach, rect)) {
                covered = false;
            }

            continue;
        }

        if (!wm_window_opaque(win)) {
            continue;
        }


        const int radius = wm_window_radius(win);

        const wm_rect_t opaque = {

            .x      = win->x + radius,
            .y      = win->y,
            .width  = win->width - 2 * radius,
            .height = win->height - radius,
        };

        if (opaque.width > 0 && opaque.height > 0 && ui_rect_contains(&opaque, rect)) {
            covered = true;
        }
    }

    return covered;
}


/**
 * @brief Follows the adapter into a new mode: rebuilds the display and repaints all of it.
 *
 * @return 0 on success, or -1 when the new mode cannot be drawn on.
 */
static int wm_display_changed(void) {

    if (wm_display_resize(&wm.display) < 0) {
        ulog_errorf("cannot follow the display mode change");
        return -1;
    }

    wm.pointer.x = WM_CLAMP(wm.pointer.x, 0, wm.display.width - 1);
    wm.pointer.y = WM_CLAMP(wm.pointer.y, 0, wm.display.height - 1);

    wm_display_cursor_move(&wm.display, wm.pointer.x, wm.pointer.y);


    wm_rect_t everything = {0, 0, wm.display.width, wm.display.height};

    wm_damage(&everything);

    return 0;
}


void wm_composite(void) {

    if (!wm.damage.count && !wm.glass_damage.count) {
        return;
    }

    wm_blur_damage();

    for (size_t i = 0; i < wm.glass_damage.count; i++) {
        ui_damage_add(&wm.damage, wm.glass_damage.rects[i]);
    }

    ui_damage_reset(&wm.glass_damage);


    wm_rect_t damage[UI_DAMAGE_MAX];

    const size_t count = wm.damage.count;

    for (size_t i = 0; i < count; i++) {
        damage[i] = wm.damage.rects[i];
    }

    ui_damage_reset(&wm.damage);


    cairo_t* cr = wm.display.cr;

    cairo_save(cr);

    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);

    for (size_t i = 0; i < count; i++) {
        cairo_rectangle(cr, damage[i].x, damage[i].y, damage[i].width, damage[i].height);
    }

    cairo_clip(cr);


    size_t exposed = 0;

    for (size_t i = 0; i < count; i++) {

        if (wm_damage_is_covered(&damage[i], NULL)) {
            continue;
        }

        cairo_rectangle(cr, damage[i].x, damage[i].y, damage[i].width, damage[i].height);

        exposed++;
    }

    if (exposed) {

        cairo_save(cr);

        cairo_clip(cr);

        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source(cr, wm.display.background);
        cairo_paint(cr);

        cairo_restore(cr);

    } else {

        cairo_new_path(cr);
    }


    static wm_window_t** stack;
    static size_t stack_size;

    size_t depth = 0;

    for (wm_window_t* win = wm.windows; win; win = win->next) {

        if (depth == stack_size) {

            const size_t grown = stack_size ? stack_size * 2 : 64;

            wm_window_t** bigger = realloc(stack, grown * sizeof(*stack));

            if (!bigger) {
                break;
            }

            stack      = bigger;
            stack_size = grown;
        }

        stack[depth++] = win;
    }

    while (depth--) {

        if (wm_window_drawer(stack[depth])) {
            continue;
        }

        const wm_rect_t extent = wm_window_shadow_rect(stack[depth]);

        bool visible = stack[depth]->morph != NULL;

        for (size_t i = 0; i < count && !visible; i++) {

            const wm_rect_t shown = ui_rect_intersect(&extent, &damage[i]);

            visible = !ui_rect_empty(&shown) && !wm_damage_is_covered(&shown, stack[depth]);
        }

        if (!visible) {
            continue;
        }

        wm_window_paint(cr, stack[depth]);
    }

    wm_ghost_paint(cr, damage, count);

    wm_taskbar_capture(damage, count);

    wm_taskbar_paint(cr, damage, count);

    wm_drawer_paint(cr, damage, count);


    if (!wm.display.hwcursor) {
        wm_cursor_paint(cr, wm.pointer.x, wm.pointer.y);
    }

    cairo_restore(cr);


    if (wm_display_flush(&wm.display, damage, count) < 0 && errno == ESTALE) {

        if (wm_display_changed() < 0) {
            wm.running = false;
            return;
        }

        wm_composite();
    }
}
