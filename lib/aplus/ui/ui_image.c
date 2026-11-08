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


static void ui_image_draw(ui_widget_t* widget, cairo_t* cr);


static const ui_widget_ops_t ui_image_ops = {
    .draw = ui_image_draw,
};


ui_widget_t* ui_image_create(ui_view_t* view, const char* icon) {

    ui_widget_t* widget = ui_widget_new(view, UI_WIDGET_IMAGE, &ui_image_ops, false);

    if (!widget) {
        return NULL;
    }

    if (icon && strlen(icon) < sizeof(widget->image.icon)) {
        strcpy(widget->image.icon, icon);
    }

    return widget;
}


void ui_image_set_icon(ui_widget_t* widget, const char* icon) {

    if (!ui_widget_is(widget, UI_WIDGET_IMAGE)) {
        return;
    }


    char next[UI_ICON_NAME_MAX] = {0};

    if (icon && strlen(icon) < sizeof(next)) {
        strcpy(next, icon);
    }

    if (strcmp(next, widget->image.icon) == 0) {
        return;
    }

    memcpy(widget->image.icon, next, sizeof(next));

    ui_widget_invalidate(widget);
}


void ui_image_set_size(ui_widget_t* widget, int size) {

    if (!ui_widget_is(widget, UI_WIDGET_IMAGE)) {
        return;
    }

    widget->image.size = size > 0 ? size : 0;

    ui_widget_invalidate(widget);
}


static void ui_image_draw(ui_widget_t* widget, cairo_t* cr) {

    if (!widget->image.icon[0]) {
        return;
    }


    int size = widget->rect.width < widget->rect.height ? widget->rect.width : widget->rect.height;

    if (widget->image.size > 0 && widget->image.size < size) {
        size = widget->image.size;
    }

    if (size <= 0) {
        return;
    }


    const ui_rect_t box = {widget->rect.x + (widget->rect.width - size) / 2, widget->rect.y + (widget->rect.height - size) / 2, size, size};

    cairo_surface_t* icon = ui_icon_load(widget->image.icon, size);

    if (widget->enabled) {

        ui_draw_icon(cr, box, icon);

    } else {

        cairo_push_group(cr);
        ui_draw_icon(cr, box, icon);
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, 1.0 - widget->theme->disabled_fade);
    }
}
