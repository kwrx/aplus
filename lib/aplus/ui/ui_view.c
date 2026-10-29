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

#include <aplus/input.h>

#include "ui_internal.h"
#include "ui_widget_internal.h"


/**
 * @brief How many queued events one pass may fold into a frame before it has to paint again.
 *
 * A burst of pointer motion is worth one repaint rather than one each; the bound is what stops a
 * server talking faster than this end can draw from starving the painting altogether.
 */
#define UI_VIEW_DRAIN_MAX 256

/**
 * @brief How close together two presses have to be, in milliseconds and in pixels, to count as a double click.
 */
#define UI_VIEW_DOUBLE_CLICK_MS   400
#define UI_VIEW_DOUBLE_CLICK_SLOP 4


static void ui_view_damage(ui_view_t* view, ui_rect_t rect) {

    ui_rect_t bleed = {rect.x - 1, rect.y - 1, rect.width + 2, rect.height + 2};

    ui_damage_add(&view->damage, bleed);
}


void ui_widget_invalidate(ui_widget_t* widget) {

    if (!widget) {
        return;
    }

    ui_view_damage(widget->view, widget->rect);
}


void ui_widget_invalidate_rect(ui_widget_t* widget, ui_rect_t rect) {

    const int x0 = rect.x > widget->rect.x ? rect.x : widget->rect.x;
    const int y0 = rect.y > widget->rect.y ? rect.y : widget->rect.y;
    const int x1 = rect.x + rect.width < widget->rect.x + widget->rect.width ? rect.x + rect.width : widget->rect.x + widget->rect.width;
    const int y1 = rect.y + rect.height < widget->rect.y + widget->rect.height ? rect.y + rect.height : widget->rect.y + widget->rect.height;

    if (x1 <= x0 || y1 <= y0) {
        return;
    }

    ui_rect_t clipped = {x0, y0, x1 - x0, y1 - y0};

    ui_view_damage(widget->view, clipped);
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
    return view ? view->damage.count != 0 : false;
}


/**
 * @brief Binds a cairo context to whatever buffer the window is holding right now.
 *
 * @param view The view to bind.
 * @return 0 on success, or -1 with errno set.
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


    const cairo_format_t format = ui_window_translucent(view->window) ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;

    view->surface = cairo_image_surface_create_for_data(pixels, format, width, height, (int)ui_window_stride(view->window));

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
    view->active = true;

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

    ui_view_relayout(view);
}


void ui_view_on_key(ui_view_t* view, ui_key_fn fn, void* user) {

    if (!view) {
        return;
    }

    view->key      = fn;
    view->key_user = user;
}


void ui_view_on_theme(ui_view_t* view, ui_theme_fn fn, void* user) {

    if (!view) {
        return;
    }

    view->restyle      = fn;
    view->restyle_user = user;
}


bool ui_view_closed(ui_view_t* view) {
    return view ? view->closed : true;
}


static ui_widget_t* ui_view_widget_at(ui_view_t* view, int x, int y) {

    ui_widget_t* found = NULL;

    for (ui_widget_t* w = view->widgets; w; w = w->next) {

        if (ui_widget_hit(w, x, y)) {
            found = w;
        }
    }

    return found;
}


/**
 * @brief Runs one of a widget's state hooks and repaints it when the hook says it changed.
 *
 * @param widget The widget to notify, which may be NULL.
 * @param changed What the hook reported.
 * @return The same, so that a caller can accumulate it.
 */

static bool ui_view_notified(ui_widget_t* widget, bool changed) {

    if (widget && changed) {
        ui_widget_invalidate(widget);
    }

    return changed;
}


static bool ui_view_set_hovered(ui_view_t* view, ui_widget_t* widget) {

    if (view->hovered == widget) {
        return false;
    }


    bool changed = false;

    if (view->hovered && view->hovered->ops->on_hover) {
        changed |= ui_view_notified(view->hovered, view->hovered->ops->on_hover(view->hovered, false));
    }

    view->hovered = widget;

    if (widget && widget->ops->on_hover) {
        changed |= ui_view_notified(widget, widget->ops->on_hover(widget, true));
    }

    return changed;
}


/**
 * @brief Drops the press a widget is holding without activating it, as a pointer leaving the window does.
 *
 * @param view The view holding the press.
 * @return Whether anything has to be repainted.
 */

static bool ui_view_cancel_pressed(ui_view_t* view) {

    ui_widget_t* pressed = view->pressed;

    if (!pressed) {
        return false;
    }

    view->pressed = NULL;

    if (!pressed->ops->on_release) {
        return false;
    }

    return ui_view_notified(pressed, pressed->ops->on_release(pressed, 0, 0, false, 0));
}


bool ui_view_focus(ui_view_t* view, ui_widget_t* widget) {

    if (!view || view->focused == widget) {
        return false;
    }

    if (widget && (!widget->visible || !widget->enabled)) {
        return false;
    }


    bool changed = false;

    if (view->focused && view->focused->ops->on_focus) {
        changed |= ui_view_notified(view->focused, view->focused->ops->on_focus(view->focused, false));
    }

    view->focused = widget;

    if (widget && widget->ops->on_focus) {
        changed |= ui_view_notified(widget, widget->ops->on_focus(widget, true));
    }

    return changed;
}


ui_widget_t* ui_view_focused(const ui_view_t* view) {
    return view ? view->focused : NULL;
}


/**
 * @brief Gives the pointer the shape of whatever answers for it: the pressed widget, else the hovered one, else the view.
 *
 * @param view The view.
 */
static void ui_view_update_cursor(ui_view_t* view) {

    const ui_widget_t* widget = view->pressed ? view->pressed : view->hovered;

    ui_window_set_cursor(view->window, widget ? widget->cursor : view->cursor);
}


void ui_view_set_cursor(ui_view_t* view, ui_cursor_t cursor) {

    if (!view || (unsigned)cursor >= UI_CURSOR_COUNT) {
        return;
    }

    view->cursor = cursor;

    ui_view_update_cursor(view);
}


/**
 * @brief Counts a press towards a double click, against the last one the view saw.
 *
 * @param view The view holding the click history.
 * @param widget The widget being pressed.
 * @param x The press position.
 * @param y The press position.
 * @return 2 when this press completes a double click, 1 otherwise.
 */

static int ui_view_count_click(ui_view_t* view, ui_widget_t* widget, int x, int y) {

    const uint64_t now = ui_anim_now();

    const bool again = view->click_widget == widget && now - view->click_time <= UI_VIEW_DOUBLE_CLICK_MS && abs(x - view->click_x) <= UI_VIEW_DOUBLE_CLICK_SLOP && abs(y - view->click_y) <= UI_VIEW_DOUBLE_CLICK_SLOP;

    view->click_count  = again ? view->click_count + 1 : 1;
    view->click_widget = widget;
    view->click_time   = now;
    view->click_x      = x;
    view->click_y      = y;

    return view->click_count > 2 ? 2 : view->click_count;
}


static bool ui_view_pointer(ui_view_t* view, const ui_event_t* event) {

    const int x = event->pointer.x;
    const int y = event->pointer.y;

    ui_widget_t* over = ui_view_widget_at(view, x, y);

    bool changed = ui_view_set_hovered(view, over);

    const bool down    = (event->pointer.buttons & UI_BUTTON_LEFT) != 0;
    const bool context = (event->pointer.buttons & UI_BUTTON_RIGHT) && !(view->buttons & UI_BUTTON_RIGHT);

    view->buttons = event->pointer.buttons;
    view->menu.x  = x;
    view->menu.y  = y;

    if (context && over && !view->pressed && over->ops->on_context) {

        if (over->ops->on_key) {
            changed |= ui_view_focus(view, over);
        }

        changed |= ui_view_notified(over, over->ops->on_context(over, x, y));
    }


    if (down) {

        if (view->pressed) {

            if (view->pressed->ops->on_drag) {
                changed |= ui_view_notified(view->pressed, view->pressed->ops->on_drag(view->pressed, x, y, view->pressed == over));
            }

            return changed;
        }


        if (!over) {
            return changed;
        }


        view->pressed = over;

        view->click_count = ui_view_count_click(view, over, x, y);

        if (over->ops->on_key) {
            changed |= ui_view_focus(view, over);
        }

        if (over->ops->on_press) {
            changed |= ui_view_notified(over, over->ops->on_press(over, x, y));
        }

        return changed;
    }


    ui_widget_t* released = view->pressed;

    if (!released) {

        if (over && over->ops->on_motion) {
            changed |= ui_view_notified(over, over->ops->on_motion(over, x, y));
        }

        return changed;
    }

    view->pressed = NULL;

    if (released->ops->on_release) {
        changed |= ui_view_notified(released, released->ops->on_release(released, x, y, released == over, released == over ? view->click_count : 0));
    }

    return changed;
}


/**
 * @brief Sends a wheel step to whatever the pointer is over.
 *
 * @param view The view to scroll.
 * @param event The scroll event.
 * @return Whether anything has to be repainted.
 */

static bool ui_view_scroll(ui_view_t* view, const ui_event_t* event) {

    ui_widget_t* over = view->hovered;

    if (!over || !over->ops->on_scroll) {
        return false;
    }

    return ui_view_notified(over, over->ops->on_scroll(over, event->scroll.dy));
}


bool ui_view_dispatch(ui_view_t* view, const ui_event_t* event) {

    if (!view || !event) {
        return false;
    }


    switch (event->type) {

        case UI_EVENT_CONFIGURE: {

            const int applied = ui_window_apply_configure(view->window);

            if (applied <= 0) {

                return false;
            }

            if (ui_view_bind_surface(view) < 0) {
                return false;
            }

            ui_view_relayout(view);

            return true;
        }

        case UI_EVENT_POINTER: {

            const bool changed = ui_view_pointer(view, event);

            ui_view_update_cursor(view);

            return changed;
        }

        case UI_EVENT_SCROLL:
            return ui_view_scroll(view, event);

        case UI_EVENT_LEAVE:

            return ui_view_set_hovered(view, NULL) | ui_view_cancel_pressed(view);

        case UI_EVENT_KEY:

            if (event->key.vkey == KEY_LEFTSHIFT || event->key.vkey == KEY_RIGHTSHIFT) {

                const uint8_t side = event->key.vkey == KEY_LEFTSHIFT ? 1 : 2;

                view->shift = event->key.down ? view->shift | side : view->shift & (uint8_t)~side;
            }

            if (view->focused && view->focused->ops->on_key) {

                if (view->focused->ops->on_key(view->focused, event->key.vkey, event->key.down != 0)) {
                    return true;
                }
            }

            if (view->key) {
                return view->key(view, event->key.vkey, event->key.down != 0, view->key_user);
            }

            return false;

        case UI_EVENT_CLOSE:

            view->closed = true;

            return true;

        case UI_EVENT_MENU: {

            const ui_menu_fn fn = view->menu.fn;

            if (!fn || event->menu.serial != view->menu.serial) {
                return false;
            }

            view->menu.fn = NULL;

            fn(view, event->menu.id, view->menu.user);

            return true;
        }

        case UI_EVENT_THEME:

            ui_theme_update(view->window->conn);

            if (view->restyle) {
                view->restyle(view, view->restyle_user);
            }

            for (ui_widget_t* w = view->widgets; w; w = w->next) {

                if (w->ops->restyle) {
                    w->ops->restyle(w);
                }
            }

            ui_view_relayout(view);

            return true;

        case UI_EVENT_FOCUS:

            view->active = event->focus.focused != 0;

            if (view->focused) {
                ui_widget_invalidate(view->focused);
            }

            if (!event->focus.focused) {

                view->shift = 0;

                return ui_view_set_hovered(view, NULL) | ui_view_cancel_pressed(view);
            }

            return view->focused != NULL;

        default:
            return false;
    }
}


void ui_view_menu_anchor(ui_view_t* view, int x, int y) {

    view->menu.x = x;
    view->menu.y = y;
}


bool ui_view_shift(const ui_view_t* view) {
    return view->shift != 0;
}


int ui_view_popup_menu(ui_view_t* view, const ui_menu_item_t* items, size_t count, ui_menu_fn fn, void* user) {

    uint32_t serial;

    if (!view) {
        errno = EINVAL;
        return -1;
    }

    if (ui_window_popup_menu(view->window, view->menu.x, view->menu.y, items, count, &serial) < 0) {
        return -1;
    }

    view->menu.serial = serial;
    view->menu.fn     = fn;
    view->menu.user   = user;

    return 0;
}


int ui_view_timeout(ui_view_t* view) {

    if (!view) {
        return -1;
    }


    const uint64_t now = ui_anim_now();

    int soonest = -1;

    for (ui_widget_t* w = view->widgets; w; w = w->next) {

        if (!w->visible || !w->ops->tick) {
            continue;
        }


        const int next = w->ops->tick(w, now);

        if (next < 0) {
            continue;
        }

        if (soonest < 0 || next < soonest) {
            soonest = next;
        }
    }

    return soonest;
}


int ui_view_present(ui_view_t* view) {

    if (!view || !view->cr) {
        errno = EINVAL;
        return -1;
    }

    if (!view->damage.count) {
        return 0;
    }


    ui_damage_clip(&view->damage, ui_window_width(view->window), ui_window_height(view->window));

    if (!view->damage.count) {
        return 0;
    }


    const ui_damage_t area = view->damage;

    ui_damage_reset(&view->damage);

    view->frame_time = ui_anim_now();


    cairo_t* cr = view->cr;

    cairo_save(cr);

    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);

    for (size_t i = 0; i < area.count; i++) {
        cairo_rectangle(cr, area.rects[i].x, area.rects[i].y, area.rects[i].width, area.rects[i].height);
    }

    cairo_clip(cr);

    ui_draw_set_color(cr, view->theme->background);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);


    for (ui_widget_t* w = view->widgets; w; w = w->next) {

        if (!w->visible) {
            continue;
        }


        bool touched = false;

        for (size_t i = 0; i < area.count && !touched; i++) {

            touched = w->rect.x < area.rects[i].x + area.rects[i].width && area.rects[i].x < w->rect.x + w->rect.width && w->rect.y < area.rects[i].y + area.rects[i].height &&
                      area.rects[i].y < w->rect.y + w->rect.height;
        }

        if (!touched) {
            continue;
        }


        cairo_save(cr);

        ui_widget_draw(w, cr);

        cairo_restore(cr);

        cairo_new_path(cr);
    }

    cairo_restore(cr);


    cairo_surface_flush(view->surface);

    for (size_t i = 0; i < area.count; i++) {
        ui_window_damage(view->window, area.rects[i].x, area.rects[i].y, area.rects[i].width, area.rects[i].height);
    }

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

        const int timeout = ui_view_timeout(view);

        if (ui_view_present(view) < 0) {
            return -1;
        }


        ui_event_t event;

        const int e = ui_next_event(view->window->conn, &event, timeout);

        if (e < 0) {
            return -1;
        }

        if (e == 0) {
            continue;
        }

        ui_view_dispatch(view, &event);


        for (size_t drained = 0; drained < UI_VIEW_DRAIN_MAX && !view->closed; drained++) {

            const int pending = ui_next_event(view->window->conn, &event, 0);

            if (pending < 0) {
                return -1;
            }

            if (pending == 0) {
                break;
            }

            ui_view_dispatch(view, &event);
        }
    }

    return 0;
}
