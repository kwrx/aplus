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
 * @brief Traces the drawer's outline from the top border round to the rail, an edge's width out from the shape.
 *
 * @param cr The cairo context.
 * @param x0 The rail's edge.
 * @param y0 The top border's edge.
 * @param x1 The drawer's right edge.
 * @param y1 The drawer's bottom.
 * @param top The radius of the fillet into the top border.
 * @param corner The radius of the outer corner.
 * @param bottom The radius of the fillet into the rail.
 * @param edge How far out to trace it: 0 for the shape itself, 0.5 for its hairline.
 */
static void wm_drawer_outline(cairo_t* cr, double x0, double y0, double x1, double y1, double top, double corner, double bottom, double edge) {

    cairo_arc_negative(cr, x1 + top, y0 + top, WM_MAX(top - edge, 0.0), 3.0 * M_PI / 2.0, M_PI);
    cairo_arc(cr, x1 - corner, y1 - corner, corner + edge, 0.0, M_PI / 2.0);
    cairo_arc_negative(cr, x0 + bottom, y1 + bottom, WM_MAX(bottom - edge, 0.0), 3.0 * M_PI / 2.0, M_PI);
}


static void wm_drawer_outline_bottom(cairo_t* cr, double x0, double x1, double y1, double yb, double corner, double fillet, double edge) {

    cairo_arc_negative(cr, x0 - fillet, yb - fillet, WM_MAX(fillet - edge, 0.0), M_PI / 2.0, 0.0);
    cairo_arc(cr, x0 + corner, y1 + corner, corner + edge, M_PI, 3.0 * M_PI / 2.0);
    cairo_arc(cr, x1 - corner, y1 + corner, corner + edge, 3.0 * M_PI / 2.0, 2.0 * M_PI);
    cairo_arc_negative(cr, x1 + fillet, yb - fillet, WM_MAX(fillet - edge, 0.0), M_PI, M_PI / 2.0);
}


static wm_drawer_t* wm_drawer_of(const wm_window_t* win) {

    return &wm.drawers[(win->flags & UI_WINDOW_BOTTOM) ? WM_DRAWER_BOTTOM : WM_DRAWER_RAIL];
}


static wm_drawer_edge_t wm_drawer_edge(const wm_drawer_t* drawer) {

    return (wm_drawer_edge_t)(drawer - wm.drawers);
}


static double wm_drawer_center(const wm_drawer_t* drawer) {

    return drawer->rect.x + drawer->rect.width / 2.0;
}


/**
 * @brief Reports where the drawer is at a moment, with the fillets that join it to the rail and the border.
 *
 * @param now The clock reading.
 * @return Its bounding box, empty while it is all the way in.
 */
static wm_rect_t wm_drawer_extent(const wm_drawer_t* drawer, uint64_t now) {

    const double open = ui_anim_value(&drawer->open, now);

    if (open <= 0.0) {

        const wm_rect_t none = {0, 0, 0, 0};

        return none;
    }


    const double span = ui_anim_value(&drawer->span, now);

    if (wm_drawer_edge(drawer) == WM_DRAWER_BOTTOM) {

        const double reveal = drawer->rect.height * open;
        const double bottom = drawer->rect.y + drawer->rect.height;
        const double left   = floor(wm_drawer_center(drawer) - span / 2.0) - WM_TASKBAR_RADIUS - 2;
        const double top    = floor(bottom - reveal) - 2;

        const wm_rect_t r = {(int)left, (int)top, (int)ceil(span) + 2 * WM_TASKBAR_RADIUS + 5, (int)(bottom - top)};

        return wm_taskbar_shadow_extent(r);
    }


    const double reveal = drawer->rect.width * open;

    const wm_rect_t r = {drawer->rect.x, drawer->rect.y, (int)ceil(reveal) + WM_TASKBAR_RADIUS + 2, (int)ceil(span) + WM_TASKBAR_RADIUS + 2};

    return wm_taskbar_shadow_extent(r);
}


/**
 * @brief Repaints the drawer where the last frame drew it and where the next one will.
 */
void wm_drawer_damage(void) {

    for (int i = 0; i < WM_DRAWER_EDGES; i++) {

        wm_drawer_t* drawer = &wm.drawers[i];

        const wm_rect_t extent = wm_drawer_extent(drawer, wm.frame_time);

        wm_damage_glass(&drawer->painted);
        wm_damage_glass(&extent);
    }
}


/**
 * @brief Drops the picture of a drawer that has gone.
 */
static void wm_drawer_drop_still(wm_drawer_t* drawer) {

    if (drawer->still) {
        cairo_surface_destroy(drawer->still);
        drawer->still = NULL;
    }
}


static void wm_drawer_place(wm_drawer_t* drawer, wm_window_t* win) {

    const wm_rect_t work = wm_workarea();

    if (wm_drawer_edge(drawer) == WM_DRAWER_BOTTOM) {
        win->x = work.x + (work.width - win->width) / 2;
        win->y = work.y + work.height - win->height;
    } else {
        win->x = work.x;
        win->y = work.y;
    }

    drawer->rect.x      = win->x;
    drawer->rect.y      = win->y;
    drawer->rect.width  = win->width;
    drawer->rect.height = win->height;
}


static double wm_drawer_span_of(const wm_drawer_t* drawer, const wm_window_t* win) {

    return wm_drawer_edge(drawer) == WM_DRAWER_BOTTOM ? win->width : win->height;
}


/**
 * @brief Makes a new window the drawer, in the top-left corner of the work area, and asks the one before it to close.
 *
 * @param win The window, not yet drawn.
 */
void wm_drawer_attach(wm_window_t* win) {

    wm_drawer_t* drawer = wm_drawer_of(win);

    if (drawer->window) {
        wm_window_request_close(drawer->window);
    }

    wm_drawer_drop_still(drawer);


    drawer->window = win;

    wm_drawer_place(drawer, win);

    ui_anim_set(&drawer->span, wm_drawer_span_of(drawer, win));

    wm_drawer_damage();
}


/**
 * @brief Slides the drawer out once its window has drawn.
 *
 * @param win The window that drew for the first time.
 */
void wm_drawer_reveal(wm_window_t* win) {

    wm_drawer_t* drawer = wm_drawer_of(win);

    if (win != drawer->window) {
        return;
    }

    ui_anim_to(&drawer->open, 1.0, UI_MOTION_SLOW, ui_anim_now());

    wm_drawer_damage();
}


/**
 * @brief Eases the drawer's background to its window's new height.
 *
 * @param win The window that was resized.
 */
void wm_drawer_resized(wm_window_t* win) {

    wm_drawer_t* drawer = wm_drawer_of(win);

    if (win != drawer->window) {
        return;
    }

    wm_drawer_damage();

    wm_drawer_place(drawer, win);

    const double span = wm_drawer_span_of(drawer, win);

    if (ui_anim_value(&drawer->open, ui_anim_now()) > 0.0) {
        ui_anim_to(&drawer->span, span, UI_MOTION_NORMAL, ui_anim_now());
    } else {
        ui_anim_set(&drawer->span, span);
    }

    wm_drawer_damage();
}


/**
 * @brief Keeps a picture of the drawer's window as it goes, and slides that back into the rail.
 *
 * @param win The window about to be destroyed.
 */
void wm_drawer_detach(wm_window_t* win) {

    wm_drawer_t* drawer = wm_drawer_of(win);

    if (win != drawer->window) {
        return;
    }

    drawer->window = NULL;


    if (win->committed && win->backstore) {

        const int width  = cairo_image_surface_get_width(win->backstore);
        const int height = cairo_image_surface_get_height(win->backstore);

        if ((drawer->still = wm_surface_create(CAIRO_FORMAT_ARGB32, width, height))) {

            cairo_t* cr = cairo_create(drawer->still);

            cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
            cairo_set_source_surface(cr, win->backstore, 0, 0);
            cairo_paint(cr);

            cairo_destroy(cr);

            cairo_surface_flush(drawer->still);
        }
    }

    ui_anim_to(&drawer->open, 0.0, UI_MOTION_NORMAL, ui_anim_now());

    wm_drawer_damage();
    wm_taskbar_damage();
}


/**
 * @brief Drops the picture of a drawer that has gone once it is all the way back in.
 */
void wm_drawer_settle(void) {

    for (int i = 0; i < WM_DRAWER_EDGES; i++) {

        wm_drawer_t* drawer = &wm.drawers[i];

        if (drawer->window || ui_anim_running(&drawer->open, wm.frame_time) || ui_anim_value(&drawer->open, wm.frame_time) > 0.0) {
            continue;
        }

        wm_drawer_drop_still(drawer);
    }
}


/**
 * @brief Closes the drawer when a press lands anywhere but on it or on the taskbar.
 *
 * @param win The window the press landed on, or NULL.
 * @param region The part of the display it landed on.
 */
void wm_drawer_press(const wm_window_t* win, wm_region_t region) {

    wm_window_t* rail = wm.drawers[WM_DRAWER_RAIL].window;

    if (rail && win != rail && region != WM_REGION_TASKBAR) {
        wm_window_request_close(rail);
    }
}


static wm_window_t* wm_drawer_hit(const wm_drawer_t* drawer, int x, int y) {

    wm_window_t* win = drawer->window;

    if (!win || !win->committed || ui_anim_target(&drawer->open) < 1.0) {
        return NULL;
    }


    const int span = (int)ui_anim_value(&drawer->span, wm.frame_time);

    wm_rect_t r = {win->x, win->y, win->width, WM_MIN(win->height, span)};

    if (wm_drawer_edge(drawer) == WM_DRAWER_BOTTOM) {

        r.width  = WM_MIN(win->width, span);
        r.x      = win->x + (win->width - r.width) / 2;
        r.height = win->height;
    }

    return ui_rect_contains_point(&r, x, y) ? win : NULL;
}


/**
 * @brief Reports whether a point is on the drawer, which takes it ahead of every window and the taskbar.
 *
 * @param x The point.
 * @param y The point.
 * @return The drawer's window, or NULL when the point is elsewhere or the drawer is not all the way out.
 */
wm_window_t* wm_drawer_at(int x, int y) {

    wm_window_t* win = wm_drawer_hit(&wm.drawers[WM_DRAWER_RAIL], x, y);

    return win ? win : wm_drawer_hit(&wm.drawers[WM_DRAWER_BOTTOM], x, y);
}


wm_rect_t wm_drawer_glass_shape(wm_drawer_edge_t edge) {

    const wm_drawer_t* drawer = &wm.drawers[edge];

    if (ui_anim_value(&drawer->open, wm.frame_time) <= 0.0) {

        const wm_rect_t none = {0, 0, 0, 0};

        return none;
    }


    const wm_rect_t panel = drawer->rect;

    if (edge == WM_DRAWER_BOTTOM) {

        const int span = WM_MAX(panel.width, (int)ceil(ui_anim_value(&drawer->span, wm.frame_time)));
        const int left = (int)floor(wm_drawer_center(drawer) - span / 2.0);

        const wm_rect_t r = {left - WM_TASKBAR_RADIUS, panel.y, span + 2 * WM_TASKBAR_RADIUS + 1, panel.height};

        return r;
    }


    const int height  = WM_MAX(panel.height, (int)ceil(ui_anim_value(&drawer->span, wm.frame_time)));
    const wm_rect_t r = {panel.x, panel.y, panel.width + WM_TASKBAR_RADIUS, height + WM_TASKBAR_RADIUS};

    return r;
}


static cairo_surface_t* wm_drawer_surface(const wm_drawer_t* drawer) {

    const wm_window_t* win = drawer->window;

    return win ? (win->committed ? win->backstore : NULL) : drawer->still;
}


static void wm_drawer_paint_rail(cairo_t* cr, wm_drawer_t* drawer) {

    const double open   = ui_anim_value(&drawer->open, wm.frame_time);
    const double reveal = drawer->rect.width * open;

    const double x0 = drawer->rect.x;
    const double y0 = drawer->rect.y;
    const double x1 = x0 + reveal;
    const double y1 = y0 + round(ui_anim_value(&drawer->span, wm.frame_time));

    if (reveal < 1.0 || y1 <= y0) {
        return;
    }


    const double corner = WM_MIN(WM_TASKBAR_RADIUS, WM_MIN(reveal, y1 - y0) / 2.0);
    const double top    = WM_MIN(WM_TASKBAR_RADIUS, reveal);
    const double bottom = WM_MIN(WM_TASKBAR_RADIUS, reveal - corner);

    const wm_rect_t shape = {(int)x0, (int)y0, (int)ceil(reveal), (int)(y1 - y0)};

    wm_taskbar_shadow(cr, &shape, open);

    cairo_save(cr);

    cairo_new_path(cr);
    cairo_move_to(cr, x0, y0);
    cairo_line_to(cr, x1 + top, y0);
    wm_drawer_outline(cr, x0, y0, x1, y1, top, corner, bottom, 0.0);
    cairo_close_path(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    wm_taskbar_glass(cr, &drawer->backdrop);


    cairo_surface_t* surface = wm_drawer_surface(drawer);

    if (surface) {

        cairo_rectangle(cr, x0, y0, reveal, y1 - y0);
        cairo_clip(cr);

        cairo_set_source_surface(cr, surface, x1 - drawer->rect.width, y0);
        cairo_paint(cr);
    }

    cairo_restore(cr);


    cairo_new_path(cr);
    wm_drawer_outline(cr, x0, y0, x1, y1, top, corner, bottom, 0.5);
    wm_taskbar_edge(cr);
}


static void wm_drawer_paint_bottom(cairo_t* cr, wm_drawer_t* drawer) {

    const double open   = ui_anim_value(&drawer->open, wm.frame_time);
    const double reveal = round(drawer->rect.height * open);
    const double span   = round(ui_anim_value(&drawer->span, wm.frame_time));

    const double x0 = round(wm_drawer_center(drawer) - span / 2.0);
    const double x1 = x0 + span;
    const double yb = drawer->rect.y + drawer->rect.height;
    const double y1 = yb - reveal;

    if (reveal < 1.0 || span < 1.0) {
        return;
    }


    const double corner = WM_MIN(WM_TASKBAR_RADIUS, WM_MIN(reveal, span) / 2.0);
    const double fillet = WM_MIN(WM_TASKBAR_RADIUS, reveal - corner);

    const wm_rect_t shape = {(int)x0, (int)y1, (int)span, (int)reveal};

    wm_taskbar_shadow(cr, &shape, open);

    cairo_save(cr);

    cairo_new_path(cr);
    wm_drawer_outline_bottom(cr, x0, x1, y1, yb, corner, fillet, 0.0);
    cairo_close_path(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    wm_taskbar_glass(cr, &drawer->backdrop);


    cairo_surface_t* surface = wm_drawer_surface(drawer);

    if (surface) {

        cairo_rectangle(cr, x0, y1, span, reveal);
        cairo_clip(cr);

        cairo_set_source_surface(cr, surface, drawer->rect.x, y1);
        cairo_paint(cr);
    }

    cairo_restore(cr);


    cairo_new_path(cr);
    wm_drawer_outline_bottom(cr, x0, x1, y1, yb, corner, fillet, 0.5);
    wm_taskbar_edge(cr);
}


/**
 * @brief Draws the drawer over everything but the pointer: its background flaring into the rail and the border, and its window on top.
 *
 * @param cr The cairo context to draw with, already clipped to the damage.
 * @param damage The damaged rectangles.
 * @param count How many there are.
 */
void wm_drawer_paint(cairo_t* cr, const wm_rect_t* damage, size_t count) {

    for (int i = WM_DRAWER_EDGES - 1; i >= 0; i--) {

        wm_drawer_t* drawer = &wm.drawers[i];

        const wm_rect_t extent = wm_drawer_extent(drawer, wm.frame_time);

        drawer->painted = extent;

        bool visible = false;

        for (size_t j = 0; j < count && !visible && extent.width > 0; j++) {
            visible = ui_rect_intersects(&extent, &damage[j]);
        }

        if (!visible) {
            continue;
        }

        if (i == WM_DRAWER_BOTTOM) {
            wm_drawer_paint_bottom(cr, drawer);
        } else {
            wm_drawer_paint_rail(cr, drawer);
        }
    }
}


void wm_drawer_fini(void) {

    for (int i = 0; i < WM_DRAWER_EDGES; i++) {

        wm_drawer_drop_still(&wm.drawers[i]);

        wm_blur_fini(&wm.drawers[i].backdrop);

        wm.drawers[i].window = NULL;
    }
}
