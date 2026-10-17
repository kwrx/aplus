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
 * @brief The drawer: a window slid out of the rail into the top-left corner of the work area, over a background
 * in the taskbar's colour that joins the rail and the border.
 */

#include <math.h>

#include <wm.h>


/**
 * @brief Reports where the drawer is at a moment, with the fillets that join it to the rail and the border.
 *
 * @param now The clock reading.
 * @return Its bounding box, empty while it is all the way in.
 */
static wm_rect_t wm_drawer_extent(uint64_t now) {

    const double open = ui_anim_value(&wm.drawer.open, now);

    if (open <= 0.0) {

        const wm_rect_t none = {0, 0, 0, 0};

        return none;
    }


    const double reveal = wm.drawer.rect.width * open;
    const double height = ui_anim_value(&wm.drawer.height, now);

    const wm_rect_t r = {wm.drawer.rect.x, wm.drawer.rect.y, (int)ceil(reveal) + WM_TASKBAR_RADIUS + 2, (int)ceil(height) + WM_TASKBAR_RADIUS + 2};

    return r;
}


/**
 * @brief Repaints the drawer where the last frame drew it and where the next one will.
 */
void wm_drawer_damage(void) {

    const wm_rect_t extent = wm_drawer_extent(wm.frame_time);

    wm_damage(&wm.drawer.painted);
    wm_damage(&extent);
}


/**
 * @brief Drops the picture of a drawer that has gone.
 */
static void wm_drawer_drop_still(void) {

    if (wm.drawer.still) {
        cairo_surface_destroy(wm.drawer.still);
        wm.drawer.still = NULL;
    }
}


/**
 * @brief Makes a new window the drawer, in the top-left corner of the work area, and asks the one before it to close.
 *
 * @param win The window, not yet drawn.
 */
void wm_drawer_attach(wm_window_t* win) {

    const wm_rect_t work = wm_workarea();

    win->x = work.x;
    win->y = work.y;

    if (wm.drawer.window) {
        wm_window_request_close(wm.drawer.window);
    }

    wm_drawer_drop_still();


    wm.drawer.window = win;

    wm.drawer.rect.x      = win->x;
    wm.drawer.rect.y      = win->y;
    wm.drawer.rect.width  = win->width;
    wm.drawer.rect.height = win->height;

    ui_anim_set(&wm.drawer.height, win->height);

    wm_drawer_damage();
}


/**
 * @brief Slides the drawer out once its window has drawn.
 *
 * @param win The window that drew for the first time.
 */
void wm_drawer_reveal(wm_window_t* win) {

    if (win != wm.drawer.window) {
        return;
    }

    ui_anim_to(&wm.drawer.open, 1.0, UI_MOTION_SLOW, ui_anim_now());

    wm_drawer_damage();
}


/**
 * @brief Eases the drawer's background to its window's new height.
 *
 * @param win The window that was resized.
 */
void wm_drawer_resized(wm_window_t* win) {

    if (win != wm.drawer.window) {
        return;
    }

    wm.drawer.rect.width  = win->width;
    wm.drawer.rect.height = win->height;

    if (ui_anim_value(&wm.drawer.open, ui_anim_now()) > 0.0) {
        ui_anim_to(&wm.drawer.height, win->height, UI_MOTION_NORMAL, ui_anim_now());
    } else {
        ui_anim_set(&wm.drawer.height, win->height);
    }

    wm_drawer_damage();
}


/**
 * @brief Keeps a picture of the drawer's window as it goes, and slides that back into the rail.
 *
 * @param win The window about to be destroyed.
 */
void wm_drawer_detach(wm_window_t* win) {

    if (win != wm.drawer.window) {
        return;
    }

    wm.drawer.window = NULL;


    if (win->committed && win->backstore) {

        const int width  = cairo_image_surface_get_width(win->backstore);
        const int height = cairo_image_surface_get_height(win->backstore);

        if ((wm.drawer.still = wm_surface_create(CAIRO_FORMAT_ARGB32, width, height))) {

            cairo_t* cr = cairo_create(wm.drawer.still);

            cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
            cairo_set_source_surface(cr, win->backstore, 0, 0);
            cairo_paint(cr);

            cairo_destroy(cr);

            cairo_surface_flush(wm.drawer.still);
        }
    }

    ui_anim_to(&wm.drawer.open, 0.0, UI_MOTION_NORMAL, ui_anim_now());

    wm_drawer_damage();
    wm_taskbar_damage();
}


/**
 * @brief Drops the picture of a drawer that has gone once it is all the way back in.
 */
void wm_drawer_settle(void) {

    if (wm.drawer.window || ui_anim_running(&wm.drawer.open, wm.frame_time) || ui_anim_value(&wm.drawer.open, wm.frame_time) > 0.0) {
        return;
    }

    wm_drawer_drop_still();
}


/**
 * @brief Closes the drawer when a press lands anywhere but on it or on the taskbar.
 *
 * @param win The window the press landed on, or NULL.
 * @param region The part of the display it landed on.
 */
void wm_drawer_press(const wm_window_t* win, wm_region_t region) {

    if (wm.drawer.window && win != wm.drawer.window && region != WM_REGION_TASKBAR) {
        wm_window_request_close(wm.drawer.window);
    }
}


/**
 * @brief Reports whether a point is on the drawer, which takes it ahead of every window and the taskbar.
 *
 * @param x The point.
 * @param y The point.
 * @return The drawer's window, or NULL when the point is elsewhere or the drawer is not all the way out.
 */
wm_window_t* wm_drawer_at(int x, int y) {

    wm_window_t* win = wm.drawer.window;

    if (!win || !win->committed || ui_anim_target(&wm.drawer.open) < 1.0) {
        return NULL;
    }


    const wm_rect_t r = {win->x, win->y, win->width, WM_MIN(win->height, (int)ui_anim_value(&wm.drawer.height, wm.frame_time))};

    return wm_rect_contains_point(&r, x, y) ? win : NULL;
}


/**
 * @brief Draws the drawer over everything but the pointer: its background flaring into the rail and the border, and its window on top.
 *
 * @param cr The cairo context to draw with, already clipped to the damage.
 * @param damage The damaged rectangles.
 * @param count How many there are.
 */
void wm_drawer_paint(cairo_t* cr, const wm_rect_t* damage, size_t count) {

    const wm_rect_t extent = wm_drawer_extent(wm.frame_time);

    wm.drawer.painted = extent;

    bool visible = false;

    for (size_t i = 0; i < count && !visible && extent.width > 0; i++) {
        visible = wm_rect_intersects(&extent, &damage[i]);
    }

    if (!visible) {
        return;
    }


    const double open   = ui_anim_value(&wm.drawer.open, wm.frame_time);
    const double reveal = wm.drawer.rect.width * open;

    const double x0 = wm.drawer.rect.x;
    const double y0 = wm.drawer.rect.y;
    const double x1 = x0 + reveal;
    const double y1 = y0 + round(ui_anim_value(&wm.drawer.height, wm.frame_time));

    if (reveal < 1.0 || y1 <= y0) {
        return;
    }


    const double corner = WM_MIN(WM_TASKBAR_RADIUS, WM_MIN(reveal, y1 - y0) / 2.0);
    const double top    = WM_MIN(WM_TASKBAR_RADIUS, reveal);
    const double bottom = WM_MIN(WM_TASKBAR_RADIUS, reveal - corner);

    cairo_save(cr);

    cairo_new_path(cr);
    cairo_move_to(cr, x0, y0);
    cairo_line_to(cr, x1 + top, y0);
    cairo_arc_negative(cr, x1 + top, y0 + top, top, 3.0 * M_PI / 2.0, M_PI);
    cairo_arc(cr, x1 - corner, y1 - corner, corner, 0.0, M_PI / 2.0);
    cairo_arc_negative(cr, x0 + bottom, y1 + bottom, bottom, 3.0 * M_PI / 2.0, M_PI);
    cairo_close_path(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, WM_COLOR_TASKBAR);
    cairo_fill_preserve(cr);
    cairo_clip(cr);


    const wm_window_t* win = wm.drawer.window;

    cairo_surface_t* surface = win ? (win->committed ? win->backstore : NULL) : wm.drawer.still;

    if (surface) {

        cairo_rectangle(cr, x0, y0, reveal, y1 - y0);
        cairo_clip(cr);

        cairo_set_source_surface(cr, surface, x1 - wm.drawer.rect.width, y0);
        cairo_paint(cr);
    }

    cairo_restore(cr);
}


void wm_drawer_fini(void) {

    wm_drawer_drop_still();

    wm.drawer.window = NULL;
}
