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

#include <string.h>

#include "ui_widget_internal.h"


ui_widget_t* ui_button_create(ui_view_t* view, const char* text, ui_action_fn on_click, void* user) {

    ui_widget_t* widget = ui_widget_new(view, UI_WIDGET_BUTTON, true);

    if (!widget) {
        return NULL;
    }

    widget->button.style    = UI_BUTTON_STYLE_DEFAULT;
    widget->button.weight   = UI_FONT_REGULAR;
    widget->button.size     = widget->theme->font_size;
    widget->button.on_click = on_click;

    widget->user = user;

    if (text) {
        strncpy(widget->button.text, text, sizeof(widget->button.text) - 1);
    }

    return widget;
}


void ui_button_set_text(ui_widget_t* widget, const char* text) {

    if (!widget || widget->kind != UI_WIDGET_BUTTON) {
        return;
    }


    char next[UI_LABEL_TEXT_MAX] = {0};

    if (text) {
        strncpy(next, text, sizeof(next) - 1);
    }

    if (strcmp(next, widget->button.text) == 0) {
        return;
    }

    memcpy(widget->button.text, next, sizeof(next));

    ui_widget_invalidate(widget);
}


void ui_button_set_style(ui_widget_t* widget, ui_button_style_t style) {

    if (!widget || widget->kind != UI_WIDGET_BUTTON) {
        return;
    }

    widget->button.style = style;

    ui_widget_invalidate(widget);
}


void ui_button_set_font(ui_widget_t* widget, ui_font_weight_t weight, double size) {

    if (!widget || widget->kind != UI_WIDGET_BUTTON) {
        return;
    }

    widget->button.weight = weight;
    widget->button.size   = size > 0.0 ? size : widget->theme->font_size;

    ui_widget_invalidate(widget);
}


void ui_button_activate(ui_widget_t* widget) {

    if (!widget || widget->kind != UI_WIDGET_BUTTON || !widget->enabled || !widget->visible) {
        return;
    }

    if (widget->button.on_click) {
        widget->button.on_click(widget, widget->user);
    }
}


void ui_button_set_held(ui_widget_t* widget, bool held) {

    if (!widget || widget->kind != UI_WIDGET_BUTTON || widget->button.held == held) {
        return;
    }

    widget->button.held = held;

    ui_widget_invalidate(widget);
}


bool ui_button_held(const ui_widget_t* widget) {

    if (!widget || widget->kind != UI_WIDGET_BUTTON) {
        return false;
    }

    return widget->button.held;
}


bool ui_button_set_hovered(ui_widget_t* widget, bool hovered) {

    if (widget->button.hovered == hovered) {
        return false;
    }

    widget->button.hovered = hovered;

    return true;
}


bool ui_button_set_pressed(ui_widget_t* widget, bool pressed) {

    if (widget->button.pressed == pressed) {
        return false;
    }

    widget->button.pressed = pressed;

    return true;
}


/* The scheme role a style resolves to, as the pair of a fill and the colour of the text
 * that goes on it. Returning both together is what keeps a caller from picking a
 * legible-looking fill and an illegible label to sit on it.
 */
static void ui_button_colors(const ui_widget_t* widget, ui_color_t* fill, ui_color_t* text) {

    const ui_theme_t* theme = widget->theme;

    switch (widget->button.style) {

        case UI_BUTTON_STYLE_PRIMARY:
            *fill = theme->primary;
            *text = theme->on_primary;
            break;

        case UI_BUTTON_STYLE_DANGER:
            *fill = theme->danger;
            *text = theme->on_danger;
            break;

        case UI_BUTTON_STYLE_DEFAULT:
        default:
            *fill = theme->secondary;
            *text = theme->on_secondary;
            break;
    }
}


void ui_button_draw(ui_widget_t* widget, cairo_t* cr) {

    const ui_theme_t* theme = widget->theme;

    ui_color_t fill;
    ui_color_t text;

    ui_button_colors(widget, &fill, &text);


    /* One wash at a time, strongest first: a button held from the keyboard is as pressed as
       one held under the pointer, and a pointer sitting on a button it has already pressed
       should not read as twice as pressed. */
    if (widget->enabled) {

        const ui_color_t* wash = NULL;

        if (widget->button.pressed || widget->button.held) {
            wash = &theme->active;
        } else if (widget->button.hovered) {
            wash = &theme->hover;
        }

        if (wash) {
            fill = ui_color_blend(fill, *wash);
        }

    } else {

        fill = ui_draw_dim(fill, theme);
        text = ui_draw_dim(text, theme);
    }


    ui_draw_rounded_rect(cr, widget->rect, theme->corner_radius);
    ui_draw_set_color(cr, fill);
    cairo_fill(cr);


    const char* font = widget->button.weight == UI_FONT_BOLD ? theme->font_bold : theme->font_regular;

    ui_draw_text(cr, widget->rect, widget->button.text, font, widget->button.size, text, UI_ALIGN_CENTER);
}
