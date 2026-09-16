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


ui_widget_t* ui_label_create(ui_view_t* view, const char* text) {

    ui_widget_t* widget = ui_widget_new(view, UI_WIDGET_LABEL, false);

    if (!widget) {
        return NULL;
    }

    widget->label.color   = widget->theme->text;
    widget->label.align   = UI_ALIGN_LEFT;
    widget->label.weight  = UI_FONT_REGULAR;
    widget->label.size    = widget->theme->font_size;
    widget->label.padding = 0;

    if (text) {
        strncpy(widget->label.text, text, sizeof(widget->label.text) - 1);
    }

    return widget;
}


void ui_label_set_text(ui_widget_t* widget, const char* text) {

    if (!widget || widget->kind != UI_WIDGET_LABEL) {
        return;
    }


    char next[UI_LABEL_TEXT_MAX] = {0};

    if (text) {
        strncpy(next, text, sizeof(next) - 1);
    }

    if (strcmp(next, widget->label.text) == 0) {
        return;
    }

    memcpy(widget->label.text, next, sizeof(next));

    ui_widget_invalidate(widget);
}


const char* ui_label_text(const ui_widget_t* widget) {

    if (!widget || widget->kind != UI_WIDGET_LABEL) {
        return NULL;
    }

    return widget->label.text;
}


void ui_label_set_align(ui_widget_t* widget, ui_align_t align) {

    if (!widget || widget->kind != UI_WIDGET_LABEL) {
        return;
    }

    widget->label.align = align;

    ui_widget_invalidate(widget);
}


void ui_label_set_color(ui_widget_t* widget, ui_color_t color) {

    if (!widget || widget->kind != UI_WIDGET_LABEL) {
        return;
    }

    widget->label.color = color;

    ui_widget_invalidate(widget);
}


void ui_label_set_font(ui_widget_t* widget, ui_font_weight_t weight, double size) {

    if (!widget || widget->kind != UI_WIDGET_LABEL) {
        return;
    }

    widget->label.weight = weight;
    widget->label.size   = size > 0.0 ? size : widget->theme->font_size;

    ui_widget_invalidate(widget);
}


void ui_label_set_padding(ui_widget_t* widget, int padding) {

    if (!widget || widget->kind != UI_WIDGET_LABEL) {
        return;
    }

    widget->label.padding = padding;

    ui_widget_invalidate(widget);
}


void ui_label_draw(ui_widget_t* widget, cairo_t* cr) {

    const char* font = widget->label.weight == UI_FONT_BOLD ? widget->theme->font_bold : widget->theme->font_regular;

    ui_color_t color = widget->label.color;

    if (!widget->enabled) {
        color = ui_draw_dim(color, widget->theme);
    }

    ui_draw_text(cr, ui_rect_inset(widget->rect, widget->label.padding), widget->label.text, font, widget->label.size, color, widget->label.align);
}
