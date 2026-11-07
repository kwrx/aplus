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

#include <math.h>
#include <string.h>

#include <aplus/input.h>

#include "ui_widget_internal.h"


#define UI_SLIDER_TRACK_HEIGHT 4.0
#define UI_SLIDER_KNOB_RADIUS  6.0
#define UI_SLIDER_KNOB_GROW    2.0
#define UI_SLIDER_FOCUS_GAP    3.0
#define UI_SLIDER_FOCUS_WIDTH  2.0
#define UI_SLIDER_INSET        (UI_SLIDER_KNOB_RADIUS + UI_SLIDER_KNOB_GROW + UI_SLIDER_FOCUS_GAP + UI_SLIDER_FOCUS_WIDTH)
#define UI_SLIDER_STEPS        20.0
#define UI_SLIDER_PAGE_STEPS   5.0


static void ui_slider_draw(ui_widget_t* widget, cairo_t* cr);
static bool ui_slider_on_hover(ui_widget_t* widget, bool hovered);
static bool ui_slider_on_press(ui_widget_t* widget, int x, int y);
static bool ui_slider_on_drag(ui_widget_t* widget, int x, int y, bool inside);
static bool ui_slider_on_release(ui_widget_t* widget, int x, int y, bool inside, int clicks);
static bool ui_slider_on_scroll(ui_widget_t* widget, int delta);
static bool ui_slider_on_key(ui_widget_t* widget, uint16_t vkey, bool down);
static bool ui_slider_on_focus(ui_widget_t* widget, bool focused);
static int ui_slider_tick(ui_widget_t* widget, uint64_t now);


static const ui_widget_ops_t ui_slider_ops = {

    .draw       = ui_slider_draw,
    .on_hover   = ui_slider_on_hover,
    .on_press   = ui_slider_on_press,
    .on_drag    = ui_slider_on_drag,
    .on_release = ui_slider_on_release,
    .on_scroll  = ui_slider_on_scroll,
    .on_key     = ui_slider_on_key,
    .on_focus   = ui_slider_on_focus,
    .tick       = ui_slider_tick,
};


static double ui_slider_clamp(const ui_widget_t* widget, double value) {

    if (!(value >= widget->slider.min)) {
        return widget->slider.min;
    }

    if (value > widget->slider.max) {
        return widget->slider.max;
    }

    return value;
}


static double ui_slider_step(const ui_widget_t* widget) {

    if (widget->slider.step > 0.0) {
        return widget->slider.step;
    }

    return (widget->slider.max - widget->slider.min) / UI_SLIDER_STEPS;
}


static double ui_slider_fraction(const ui_widget_t* widget) {

    const double span = widget->slider.max - widget->slider.min;

    if (span <= 0.0) {
        return 0.0;
    }

    return (widget->slider.value - widget->slider.min) / span;
}


ui_widget_t* ui_slider_create(ui_view_t* view, double min, double max) {

    ui_widget_t* widget = ui_widget_new(view, UI_WIDGET_SLIDER, &ui_slider_ops, true);

    if (!widget) {
        return NULL;
    }

    widget->slider.min   = min;
    widget->slider.max   = max > min ? max : min;
    widget->slider.value = widget->slider.min;

    return widget;
}


void ui_slider_set_range(ui_widget_t* widget, double min, double max) {

    if (!widget || widget->kind != UI_WIDGET_SLIDER) {
        return;
    }

    widget->slider.min   = min;
    widget->slider.max   = max > min ? max : min;
    widget->slider.value = ui_slider_clamp(widget, widget->slider.value);

    ui_widget_invalidate(widget);
}


void ui_slider_set_value(ui_widget_t* widget, double value) {

    if (!widget || widget->kind != UI_WIDGET_SLIDER || widget->slider.dragging) {
        return;
    }

    value = ui_slider_clamp(widget, value);

    if (value == widget->slider.value) {
        return;
    }

    widget->slider.value = value;

    ui_widget_invalidate(widget);
}


double ui_slider_value(const ui_widget_t* widget) {

    if (!widget || widget->kind != UI_WIDGET_SLIDER) {
        return 0.0;
    }

    return widget->slider.value;
}


void ui_slider_set_step(ui_widget_t* widget, double step) {

    if (!widget || widget->kind != UI_WIDGET_SLIDER) {
        return;
    }

    widget->slider.step = step > 0.0 ? step : 0.0;
}


bool ui_slider_dragging(const ui_widget_t* widget) {

    if (!widget || widget->kind != UI_WIDGET_SLIDER) {
        return false;
    }

    return widget->slider.dragging;
}


void ui_slider_on_change(ui_widget_t* widget, ui_slider_fn fn, void* user) {

    if (!widget || widget->kind != UI_WIDGET_SLIDER) {
        return;
    }

    widget->slider.on_change      = fn;
    widget->slider.on_change_user = user;
}


void ui_slider_on_commit(ui_widget_t* widget, ui_slider_fn fn, void* user) {

    if (!widget || widget->kind != UI_WIDGET_SLIDER) {
        return;
    }

    widget->slider.on_commit      = fn;
    widget->slider.on_commit_user = user;
}


static bool ui_slider_move(ui_widget_t* widget, double value) {

    value = ui_slider_clamp(widget, value);

    if (value == widget->slider.value) {
        return false;
    }

    widget->slider.value = value;

    if (widget->slider.on_change) {
        widget->slider.on_change(widget, value, widget->slider.on_change_user);
    }

    return true;
}


static void ui_slider_commit(ui_widget_t* widget) {

    if (widget->slider.on_commit) {
        widget->slider.on_commit(widget, widget->slider.value, widget->slider.on_commit_user);
    }
}


static bool ui_slider_nudge(ui_widget_t* widget, double delta) {

    if (!ui_slider_move(widget, widget->slider.value + delta)) {
        return false;
    }

    ui_slider_commit(widget);

    return true;
}


static bool ui_slider_track(const ui_widget_t* widget, double* left, double* right) {

    *left  = widget->rect.x + UI_SLIDER_INSET;
    *right = widget->rect.x + widget->rect.width - UI_SLIDER_INSET;

    return *right > *left;
}


static bool ui_slider_point(ui_widget_t* widget, int x) {

    double left;
    double right;

    if (!ui_slider_track(widget, &left, &right)) {
        return false;
    }


    double t = (x - left) / (right - left);

    if (t < 0.0) {
        t = 0.0;
    }

    if (t > 1.0) {
        t = 1.0;
    }

    return ui_slider_move(widget, widget->slider.min + t * (widget->slider.max - widget->slider.min));
}


static bool ui_slider_on_hover(ui_widget_t* widget, bool hovered) {

    if (widget->slider.hovered == hovered) {
        return false;
    }

    widget->slider.hovered = hovered;

    ui_anim_to(&widget->slider.hover_anim, hovered ? 1.0 : 0.0, UI_MOTION_FAST, ui_anim_now());

    return true;
}


static bool ui_slider_on_press(ui_widget_t* widget, int x, int y) {

    (void)y;

    widget->slider.dragging = true;

    ui_anim_to(&widget->slider.drag_anim, 1.0, UI_MOTION_FAST, ui_anim_now());

    ui_slider_point(widget, x);

    return true;
}


static bool ui_slider_on_drag(ui_widget_t* widget, int x, int y, bool inside) {

    (void)y;
    (void)inside;

    if (!widget->slider.dragging) {
        return false;
    }

    return ui_slider_point(widget, x);
}


static bool ui_slider_on_release(ui_widget_t* widget, int x, int y, bool inside, int clicks) {

    (void)x;
    (void)y;
    (void)inside;
    (void)clicks;

    if (!widget->slider.dragging) {
        return false;
    }

    widget->slider.dragging = false;

    ui_anim_to(&widget->slider.drag_anim, 0.0, UI_MOTION_NORMAL, ui_anim_now());

    ui_slider_commit(widget);

    return true;
}


static bool ui_slider_on_scroll(ui_widget_t* widget, int delta) {

    if (widget->slider.dragging || delta == 0) {
        return false;
    }

    return ui_slider_nudge(widget, delta * ui_slider_step(widget));
}


static bool ui_slider_on_key(ui_widget_t* widget, uint16_t vkey, bool down) {

    if (!down || widget->slider.dragging) {
        return false;
    }


    const double step = ui_slider_step(widget);

    switch (vkey) {

        case KEY_LEFT:
        case KEY_DOWN:
            ui_slider_nudge(widget, -step);
            return true;

        case KEY_RIGHT:
        case KEY_UP:
            ui_slider_nudge(widget, step);
            return true;

        case KEY_PAGEDOWN:
            ui_slider_nudge(widget, -step * UI_SLIDER_PAGE_STEPS);
            return true;

        case KEY_PAGEUP:
            ui_slider_nudge(widget, step * UI_SLIDER_PAGE_STEPS);
            return true;

        case KEY_HOME:
            ui_slider_nudge(widget, widget->slider.min - widget->slider.value);
            return true;

        case KEY_END:
            ui_slider_nudge(widget, widget->slider.max - widget->slider.value);
            return true;

        default:
            return false;
    }
}


static bool ui_slider_on_focus(ui_widget_t* widget, bool focused) {

    if (widget->slider.focused == focused) {
        return false;
    }

    widget->slider.focused = focused;

    ui_anim_to(&widget->slider.focus_anim, focused ? 1.0 : 0.0, UI_MOTION_FAST, ui_anim_now());

    return true;
}


static int ui_slider_tick(ui_widget_t* widget, uint64_t now) {

    (void)now;

    if (!ui_widget_animating(widget, &widget->slider.hover_anim) && !ui_widget_animating(widget, &widget->slider.drag_anim) && !ui_widget_animating(widget, &widget->slider.focus_anim)) {
        return -1;
    }

    ui_widget_invalidate(widget);

    return UI_ANIM_FRAME_MS;
}


static void ui_slider_draw(ui_widget_t* widget, cairo_t* cr) {

    const ui_theme_t* theme = widget->theme;

    double left;
    double right;

    if (!ui_slider_track(widget, &left, &right)) {
        return;
    }


    const double cy   = widget->rect.y + widget->rect.height / 2.0;
    const double knob = left + ui_slider_fraction(widget) * (right - left);

    ui_color_t rest = theme->surface_sunken;
    ui_color_t fill = theme->primary;

    double lift  = 0.0;
    double focus = 0.0;

    if (widget->enabled) {

        lift  = fmax(ui_widget_anim(widget, &widget->slider.hover_anim), ui_widget_anim(widget, &widget->slider.drag_anim));
        focus = ui_widget_anim(widget, &widget->slider.focus_anim);

        fill = ui_color_blend(fill, ui_color_alpha(theme->active, ui_widget_anim(widget, &widget->slider.drag_anim)));

    } else {

        rest = ui_draw_dim(rest, theme);
        fill = ui_draw_dim(fill, theme);
    }


    const double half = UI_SLIDER_TRACK_HEIGHT / 2.0;

    ui_draw_rounded_rect_d(cr, left - half, cy - half, right - left + UI_SLIDER_TRACK_HEIGHT, UI_SLIDER_TRACK_HEIGHT, half);
    ui_draw_set_color(cr, rest);
    cairo_fill_preserve(cr);

    ui_draw_set_color(cr, theme->border);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    if (knob > left) {

        ui_draw_rounded_rect_d(cr, left - half, cy - half, knob - left + UI_SLIDER_TRACK_HEIGHT, UI_SLIDER_TRACK_HEIGHT, half);
        ui_draw_set_color(cr, fill);
        cairo_fill(cr);
    }


    const double radius = UI_SLIDER_KNOB_RADIUS + UI_SLIDER_KNOB_GROW * lift;

    if (focus > 0.0) {

        cairo_new_sub_path(cr);
        cairo_arc(cr, knob, cy, radius + UI_SLIDER_FOCUS_GAP, 0.0, 2.0 * M_PI);
        ui_draw_set_color(cr, ui_color_alpha(theme->focus_ring, focus));
        cairo_set_line_width(cr, UI_SLIDER_FOCUS_WIDTH);
        cairo_stroke(cr);
    }

    cairo_new_sub_path(cr);
    cairo_arc(cr, knob, cy, radius, 0.0, 2.0 * M_PI);
    ui_draw_set_color(cr, fill);
    cairo_fill_preserve(cr);

    ui_draw_set_color(cr, widget->enabled ? theme->on_primary : ui_draw_dim(theme->on_primary, theme));
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
}
