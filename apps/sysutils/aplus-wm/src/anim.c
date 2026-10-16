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
 * @brief The server's clock: transitions stepped once a frame, and the windows still fading out after they have gone.
 */

#include <stdlib.h>

#include <wm.h>


/**
 * @brief Steps one transition, settling it on its target once it has run out.
 *
 * @param anim The transition.
 * @param now The clock reading of the frame about to be composited.
 * @param dirty Set when the transition has something left to paint.
 * @return true while it is still running.
 */
static bool wm_anim_step(ui_anim_t* anim, uint64_t now, bool* dirty) {

    if (!anim->duration) {
        return false;
    }

    *dirty = true;

    if (ui_anim_running(anim, now)) {
        return true;
    }

    ui_anim_set(anim, anim->to);

    return false;
}


/**
 * @brief Repaints the part of the screen a frame's shadow could reach, wherever the frame happens to be.
 *
 * @param frame The frame.
 */
static void wm_damage_frame(const wm_rect_t* frame) {

    wm_rect_t area = {

        .x      = frame->x - WM_SHADOW_KEY_BLUR,
        .y      = frame->y - WM_SHADOW_KEY_BLUR,
        .width  = frame->width + 2 * WM_SHADOW_KEY_BLUR,
        .height = frame->height + 2 * WM_SHADOW_KEY_BLUR + WM_SHADOW_KEY_OFFSET,
    };

    wm_damage(&area);
}


/**
 * @brief Advances every transition to the frame about to be composited, damaging what it moved.
 *
 * @param now The clock reading of that frame.
 * @return How long the loop may sleep before the next frame, or -1 when nothing is moving.
 */
int wm_animate(uint64_t now) {

    bool running = false;


    for (wm_window_t* win = wm.windows; win; win = win->next) {

        bool whole   = false;
        bool buttons = false;
        bool bar     = false;
        bool flight  = false;

        running |= wm_anim_step(&win->appear, now, &whole);
        running |= wm_anim_step(&win->focus, now, &bar);
        running |= wm_anim_step(&win->minimize, now, &flight);

        whole |= bar;

        running |= wm_anim_step(&win->hover_task, now, &bar);

        if (win->morph) {

            running |= wm_anim_step(&win->morph_anim, now, &whole);

            wm_damage_frame(&win->morph_from);

            if (!win->morph_anim.duration) {

                cairo_surface_destroy(win->morph);

                win->morph = NULL;
                whole      = true;
            }
        }

        running |= wm_anim_step(&win->hover_close, now, &buttons);
        running |= wm_anim_step(&win->hover_maximize, now, &buttons);
        running |= wm_anim_step(&win->hover_minimize, now, &buttons);


        if (flight) {

            const wm_rect_t shadow = wm_window_shadow_rect(win);
            const wm_rect_t entry  = wm_taskbar_entry_rect(win);
            const wm_rect_t path   = wm_rect_union(&shadow, &entry);

            wm_damage(&path);
        }

        if (whole) {

            wm_damage_window(win);

        } else if (buttons) {

            const wm_rect_t close    = wm_window_close_rect(win);
            const wm_rect_t maximize = wm_window_maximize_rect(win);
            const wm_rect_t minimize = wm_window_button_rect(win, WM_REGION_MINIMIZE);

            wm_damage(&close);
            wm_damage(&maximize);
            wm_damage(&minimize);
        }

        if (bar || flight) {
            wm_taskbar_damage();
        }
    }


    bool launcher = false;

    running |= wm_anim_step(&wm.taskbar.hover_launcher, now, &launcher);

    if (launcher) {
        wm_taskbar_damage();
    }


    wm_ghost_t** it = &wm.ghosts;

    while (*it) {

        wm_ghost_t* ghost = *it;

        bool dirty = false;

        const bool alive = wm_anim_step(&ghost->fade, now, &dirty);

        wm_damage(&ghost->rect);

        if (alive) {

            running = true;
            it      = &ghost->next;

            continue;
        }


        *it = ghost->next;

        cairo_surface_destroy(ghost->surface);
        free(ghost);
    }


    return running ? UI_ANIM_FRAME_MS : -1;
}


/**
 * @brief Paints a window, shadow and all, into a surface of its own.
 *
 * @param win The window to picture.
 * @param rect Receives where on screen the picture belongs.
 * @return The picture, or NULL when it cannot be allocated.
 */
cairo_surface_t* wm_window_snapshot(wm_window_t* win, wm_rect_t* rect) {

    *rect = wm_window_shadow_rect(win);

    if (rect->width <= 0 || rect->height <= 0) {
        return NULL;
    }


    cairo_surface_t* surface = wm_surface_create(CAIRO_FORMAT_ARGB32, rect->width, rect->height);

    if (!surface) {
        return NULL;
    }


    cairo_t* cr = cairo_create(surface);

    cairo_translate(cr, -rect->x, -rect->y);

    wm_window_paint_still(cr, win);

    cairo_destroy(cr);

    cairo_surface_flush(surface);

    return surface;
}


/**
 * @brief Leaves a picture of a window behind as it goes, to fade out from wherever it had faded in to.
 *
 * @param win The window about to be destroyed.
 */
void wm_ghost_create(wm_window_t* win) {

    if (!win->committed || win->minimized) {
        return;
    }


    const double shown = ui_anim_value(&win->appear, wm.frame_time);

    if (shown <= 0.0) {
        return;
    }


    wm_ghost_t* ghost = (wm_ghost_t*)calloc(1, sizeof(wm_ghost_t));

    if (!ghost) {
        return;
    }

    if (!(ghost->surface = wm_window_snapshot(win, &ghost->rect))) {
        free(ghost);
        return;
    }


    ui_anim_set(&ghost->fade, shown);

    ghost->fade.ease = UI_EASE_OUT;

    ui_anim_to(&ghost->fade, 0.0, UI_MOTION_NORMAL, ui_anim_now());


    ghost->next = wm.ghosts;
    wm.ghosts   = ghost;

    wm_damage(&ghost->rect);
}


/**
 * @brief Paints the windows that have gone, shrinking and fading, over everything else.
 *
 * @param cr The cairo context to draw with, already clipped to the damage.
 * @param damage The damaged rectangles.
 * @param count How many there are.
 */
void wm_ghost_paint(cairo_t* cr, const wm_rect_t* damage, size_t count) {

    for (wm_ghost_t* ghost = wm.ghosts; ghost; ghost = ghost->next) {

        bool visible = false;

        for (size_t i = 0; i < count && !visible; i++) {
            visible = wm_rect_intersects(&ghost->rect, &damage[i]);
        }

        if (!visible) {
            continue;
        }


        const double t = ui_anim_value(&ghost->fade, wm.frame_time);

        if (t <= 0.0) {
            continue;
        }


        const double scale = ui_lerp(WM_CLOSE_SCALE, 1.0, t);
        const double cx    = ghost->rect.x + ghost->rect.width / 2.0;
        const double cy    = ghost->rect.y + ghost->rect.height / 2.0;

        cairo_save(cr);

        cairo_translate(cr, cx, cy);
        cairo_scale(cr, scale, scale);
        cairo_translate(cr, -cx, -cy);

        cairo_set_source_surface(cr, ghost->surface, ghost->rect.x, ghost->rect.y);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
        cairo_paint_with_alpha(cr, t);

        cairo_restore(cr);
    }
}


/**
 * @brief Drops every window still fading out, at shutdown.
 */
void wm_ghost_fini(void) {

    while (wm.ghosts) {

        wm_ghost_t* next = wm.ghosts->next;

        cairo_surface_destroy(wm.ghosts->surface);
        free(wm.ghosts);

        wm.ghosts = next;
    }
}
