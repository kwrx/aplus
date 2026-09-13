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

#include "ui_widget_internal.h"


ui_widget_t* ui_panel_create(ui_view_t* view) {

    ui_widget_t* widget = ui_widget_new(view, UI_WIDGET_PANEL, false);

    if (!widget) {
        return NULL;
    }

    widget->panel.color        = widget->theme->surface;
    widget->panel.radius       = widget->theme->corner_radius;
    widget->panel.border_color = widget->theme->border;
    widget->panel.border_width = 0.0;

    return widget;
}


void ui_panel_set_color(ui_widget_t* widget, ui_color_t color) {

    if (!widget || widget->kind != UI_WIDGET_PANEL) {
        return;
    }

    widget->panel.color = color;

    ui_widget_invalidate(widget);
}


void ui_panel_set_radius(ui_widget_t* widget, double radius) {

    if (!widget || widget->kind != UI_WIDGET_PANEL) {
        return;
    }

    widget->panel.radius = radius;

    ui_widget_invalidate(widget);
}


void ui_panel_set_border(ui_widget_t* widget, ui_color_t color, double width) {

    if (!widget || widget->kind != UI_WIDGET_PANEL) {
        return;
    }

    widget->panel.border_color = color;
    widget->panel.border_width = width;

    ui_widget_invalidate(widget);
}


void ui_panel_draw(ui_widget_t* widget, cairo_t* cr) {

    ui_draw_rounded_rect(cr, widget->rect, widget->panel.radius);
    ui_draw_set_color(cr, widget->panel.color);

    if (widget->panel.border_width <= 0.0) {
        cairo_fill(cr);
        return;
    }

    cairo_fill_preserve(cr);


    /* Inset by half the pen, because cairo centres a stroke on the path: on the path
       itself half of a one-pixel border would land outside the widget, over whatever the
       layout put next to it. */
    const double half = widget->panel.border_width / 2.0;

    cairo_new_path(cr);

    ui_draw_rounded_rect_d(cr, widget->rect.x + half, widget->rect.y + half, widget->rect.width - widget->panel.border_width, widget->rect.height - widget->panel.border_width, widget->panel.radius - half);

    ui_draw_set_color(cr, widget->panel.border_color);

    cairo_set_line_width(cr, widget->panel.border_width);
    cairo_stroke(cr);
}
