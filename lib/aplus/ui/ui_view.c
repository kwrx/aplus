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

#include <errno.h>
#include <stdlib.h>

#include "ui_internal.h"
#include "ui_widget_internal.h"


static void ui_view_damage(ui_view_t* view, ui_rect_t rect) {

    /* Widgets are painted over each other with anti-aliased edges, so the pixels a widget
       affects reach a hair past the rectangle it claims. Growing the damage by a pixel on
       every side is what stops the outermost row of a rounded corner from being left
       behind when the widget under it is repainted. */
    int x0 = rect.x - 1;
    int y0 = rect.y - 1;
    int x1 = rect.x + rect.width + 1;
    int y1 = rect.y + rect.height + 1;

    if (x0 >= x1 || y0 >= y1) {
        return;
    }


    if (!view->damage.valid) {

        view->damage.valid = true;
        view->damage.x0    = x0;
        view->damage.y0    = y0;
        view->damage.x1    = x1;
        view->damage.y1    = y1;

        return;
    }

    if (x0 < view->damage.x0) {
        view->damage.x0 = x0;
    }

    if (y0 < view->damage.y0) {
        view->damage.y0 = y0;
    }

    if (x1 > view->damage.x1) {
        view->damage.x1 = x1;
    }

    if (y1 > view->damage.y1) {
        view->damage.y1 = y1;
    }
}


void ui_widget_invalidate(ui_widget_t* widget) {

    if (!widget) {
        return;
    }

    ui_view_damage(widget->view, widget->rect);
}


void ui_view_invalidate(ui_view_t* view) {

    if (!view) {
        return;
    }


    ui_rect_t all = {

        .x      = 0,
        .y      = 0,
        .width  = ui_window_width(view->window),
        .height = ui_window_height(view->window),
    };

    ui_view_damage(view, all);
}


bool ui_view_needs_paint(ui_view_t* view) {
    return view ? view->damage.valid : false;
}


/* Binds a cairo context to whatever buffer the window is holding right now. Every resize
 * hands out a different one -- ui_window_apply_configure() may reallocate -- so the
 * context cannot outlive the configure that made it.
 *
 * RGB24 rather than ARGB32, to match what the server reads back: ARGB32 in cairo is
 * premultiplied, and the window protocol carries plain 0xFFRRGGBB. The memory layout is
 * the same either way, so the only thing the choice changes is whether cairo tries to
 * un-premultiply pixels that were never premultiplied.
 */
static int ui_view_bind_surface(ui_view_t* view) {

    if (view->cr) {
        cairo_destroy(view->cr);
        view->cr = NULL;
    }

    if (view->surface) {
        cairo_surface_destroy(view->surface);
        view->surface = NULL;
    }


    const int width  = ui_window_width(view->window);
    const int height = ui_window_height(view->window);

    unsigned char* pixels = (unsigned char*)ui_window_pixels(view->window);

    if (!pixels || width <= 0 || height <= 0) {
        errno = EINVAL;
        return -1;
    }


    view->surface = cairo_image_surface_create_for_data(pixels, CAIRO_FORMAT_RGB24, width, height, (int)ui_window_stride(view->window));

    if (cairo_surface_status(view->surface) != CAIRO_STATUS_SUCCESS) {

        cairo_surface_destroy(view->surface);

        view->surface = NULL;
        errno         = ENOMEM;

        return -1;
    }


    view->cr = cairo_create(view->surface);

    if (cairo_status(view->cr) != CAIRO_STATUS_SUCCESS) {

        cairo_destroy(view->cr);
        cairo_surface_destroy(view->surface);

        view->cr      = NULL;
        view->surface = NULL;
        errno         = ENOMEM;

        return -1;
    }

    return 0;
}


static void ui_view_relayout(ui_view_t* view) {

    if (view->layout) {
        view->layout(view, ui_window_width(view->window), ui_window_height(view->window), view->layout_user);
    }

    ui_view_invalidate(view);
}


ui_view_t* ui_view_create(ui_window_t* window) {

    if (!window) {
        errno = EINVAL;
        return NULL;
    }


    ui_view_t* view = (ui_view_t*)calloc(1, sizeof(ui_view_t));

    if (!view) {
        return NULL;
    }

    view->window = window;
    view->theme  = ui_theme();

    if (ui_view_bind_surface(view) < 0) {
        free(view);
        return NULL;
    }

    ui_view_invalidate(view);

    return view;
}


void ui_view_destroy(ui_view_t* view) {

    if (!view) {
        return;
    }


    ui_widget_t* widget = view->widgets;

    while (widget) {

        ui_widget_t* next = widget->next;

        free(widget);

        widget = next;
    }


    if (view->cr) {
        cairo_destroy(view->cr);
    }

    if (view->surface) {
        cairo_surface_destroy(view->surface);
    }

    free(view);
}


ui_window_t* ui_view_window(ui_view_t* view) {
    return view ? view->window : NULL;
}


void ui_view_on_layout(ui_view_t* view, ui_layout_fn fn, void* user) {

    if (!view) {
        return;
    }

    view->layout      = fn;
    view->layout_user = user;

    /* Run straight away: widgets created before the callback was installed have no
       geometry yet, and everything that follows assumes a laid-out tree. */
    ui_view_relayout(view);
}


void ui_view_on_key(ui_view_t* view, ui_key_fn fn, void* user) {

    if (!view) {
        return;
    }

    view->key      = fn;
    view->key_user = user;
}


bool ui_view_closed(ui_view_t* view) {
    return view ? view->closed : true;
}


static ui_widget_t* ui_view_widget_at(ui_view_t* view, int x, int y) {

    ui_widget_t* found = NULL;

    /* The list is in paint order, so the last widget that covers the point is the one on
       top of the stack and the one the pointer is actually touching. */
    for (ui_widget_t* w = view->widgets; w; w = w->next) {

        if (ui_widget_hit(w, x, y)) {
            found = w;
        }
    }

    return found;
}


static bool ui_view_set_hovered(ui_view_t* view, ui_widget_t* widget) {

    if (view->hovered == widget) {
        return false;
    }


    bool changed = false;

    if (view->hovered && view->hovered->kind == UI_WIDGET_BUTTON) {

        if (ui_button_set_hovered(view->hovered, false)) {

            ui_widget_invalidate(view->hovered);

            changed = true;
        }
    }

    view->hovered = widget;

    if (widget && widget->kind == UI_WIDGET_BUTTON) {

        if (ui_button_set_hovered(widget, true)) {

            ui_widget_invalidate(widget);

            changed = true;
        }
    }

    return changed;
}


static bool ui_view_set_pressed(ui_view_t* view, ui_widget_t* widget) {

    if (view->pressed == widget) {
        return false;
    }


    bool changed = false;

    if (view->pressed && view->pressed->kind == UI_WIDGET_BUTTON) {

        if (ui_button_set_pressed(view->pressed, false)) {

            ui_widget_invalidate(view->pressed);

            changed = true;
        }
    }

    view->pressed = widget;

    if (widget && widget->kind == UI_WIDGET_BUTTON) {

        if (ui_button_set_pressed(widget, true)) {

            ui_widget_invalidate(widget);

            changed = true;
        }
    }

    return changed;
}


static bool ui_view_pointer(ui_view_t* view, const ui_event_t* event) {

    ui_widget_t* over = ui_view_widget_at(view, event->pointer.x, event->pointer.y);

    bool changed = ui_view_set_hovered(view, over);

    const bool down = (event->pointer.buttons & UI_BUTTON_LEFT) != 0;


    if (down) {

        /* Only the widget the press started on can be the pressed one. Dragging onto a
           second widget with the button already held must not press that one too, and
           sliding back off the first has to un-press it, which is what makes a press
           cancellable by moving away before letting go. */
        if (!view->pressed) {

            changed |= ui_view_set_pressed(view, over);

        } else if (view->pressed->kind == UI_WIDGET_BUTTON) {

            if (ui_button_set_pressed(view->pressed, view->pressed == over)) {

                ui_widget_invalidate(view->pressed);

                changed = true;
            }
        }

        return changed;
    }


    ui_widget_t* released = view->pressed;

    changed |= ui_view_set_pressed(view, NULL);

    /* The action runs on release and only over the widget the press began on, so a press
       can be taken back by sliding off it. */
    if (released && released == over && released->kind == UI_WIDGET_BUTTON) {

        ui_button_activate(released);

        changed = true;
    }

    return changed;
}


bool ui_view_dispatch(ui_view_t* view, const ui_event_t* event) {

    if (!view || !event) {
        return false;
    }


    switch (event->type) {

        case UI_EVENT_CONFIGURE: {

            const int applied = ui_window_apply_configure(view->window);

            if (applied <= 0) {

                /* Nothing was pending, or the resize failed and the pending configure is
                   still there to retry. Either way the surface still describes the buffer
                   the window is holding, so it must not be rebound. */
                return false;
            }

            if (ui_view_bind_surface(view) < 0) {
                return false;
            }

            ui_view_relayout(view);

            return true;
        }

        case UI_EVENT_POINTER:
            return ui_view_pointer(view, event);

        case UI_EVENT_LEAVE:

            /* A press that ends outside the window is a press that never arrives here as a
               release, so it is dropped rather than left held. */
            return ui_view_set_hovered(view, NULL) | ui_view_set_pressed(view, NULL);

        case UI_EVENT_KEY:

            if (view->key) {
                return view->key(view, event->key.vkey, event->key.down != 0, view->key_user);
            }

            return false;

        case UI_EVENT_CLOSE:

            view->closed = true;

            return true;

        case UI_EVENT_FOCUS:

            /* Losing focus means the pointer is somewhere else, and whatever was lit under
               it should go out. Gaining it says nothing about where the pointer is: the
               UI_EV_POINTER that follows does. */
            if (!event->focus.focused) {
                return ui_view_set_hovered(view, NULL) | ui_view_set_pressed(view, NULL);
            }

            return false;

        default:
            return false;
    }
}


int ui_view_present(ui_view_t* view) {

    if (!view || !view->cr) {
        errno = EINVAL;
        return -1;
    }

    if (!view->damage.valid) {
        return 0;
    }


    const int width  = ui_window_width(view->window);
    const int height = ui_window_height(view->window);

    int x0 = view->damage.x0 < 0 ? 0 : view->damage.x0;
    int y0 = view->damage.y0 < 0 ? 0 : view->damage.y0;
    int x1 = view->damage.x1 > width ? width : view->damage.x1;
    int y1 = view->damage.y1 > height ? height : view->damage.y1;

    view->damage.valid = false;

    if (x0 >= x1 || y0 >= y1) {
        return 0;
    }


    ui_rect_t area = {x0, y0, x1 - x0, y1 - y0};

    cairo_t* cr = view->cr;

    cairo_save(cr);

    cairo_rectangle(cr, area.x, area.y, area.width, area.height);
    cairo_clip(cr);

    /* Widgets draw only themselves, so the backdrop has to be laid down first or a label
       that got shorter would leave the tail of the old string behind. Clipping means this
       costs the damaged rectangle rather than the window. */
    ui_draw_set_color(cr, view->theme->background);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);


    for (ui_widget_t* w = view->widgets; w; w = w->next) {

        if (!w->visible) {
            continue;
        }

        if (w->rect.x >= x1 || w->rect.y >= y1 || w->rect.x + w->rect.width <= x0 || w->rect.y + w->rect.height <= y0) {
            continue;
        }

        cairo_save(cr);

        ui_widget_draw(w, cr);

        cairo_restore(cr);

        /* Widgets leave paths behind them -- a fill consumes one, a stroke on a
           fill_preserve does not -- and a leftover path would be picked up by whatever
           draws next. */
        cairo_new_path(cr);
    }

    cairo_restore(cr);


    /* The surface writes straight into the window's pixels, but cairo is free to be
       holding some of them back until it is told the drawing is finished. */
    cairo_surface_flush(view->surface);

    ui_window_damage(view->window, area.x, area.y, area.width, area.height);

    if (ui_window_commit(view->window) < 0) {
        return -1;
    }

    return 1;
}


int ui_view_run(ui_view_t* view) {

    if (!view) {
        errno = EINVAL;
        return -1;
    }


    while (!view->closed) {

        if (ui_view_present(view) < 0) {
            return -1;
        }


        ui_event_t event;

        const int e = ui_next_event(view->window->conn, &event, -1);

        if (e < 0) {
            return -1;
        }

        if (e == 0) {
            continue;
        }

        ui_view_dispatch(view, &event);
    }

    return 0;
}
