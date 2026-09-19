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

#include <stdlib.h>
#include <string.h>

#include "ui_widget_internal.h"


ui_widget_t* ui_widget_new(ui_view_t* view, ui_widget_kind_t kind, const ui_widget_ops_t* ops, bool interactive) {

    if (!view || !ops) {
        return NULL;
    }


    ui_widget_t* widget = (ui_widget_t*)calloc(1, sizeof(ui_widget_t));

    if (!widget) {
        return NULL;
    }

    widget->view        = view;
    widget->kind        = kind;
    widget->ops         = ops;
    widget->visible     = true;
    widget->enabled     = true;
    widget->interactive = interactive;
    widget->theme       = view->theme;


    if (view->widgets_tail) {
        view->widgets_tail->next = widget;
    } else {
        view->widgets = widget;
    }

    view->widgets_tail = widget;

    return widget;
}


void ui_widget_destroy(ui_widget_t* widget) {

    if (!widget) {
        return;
    }


    ui_view_t* view = widget->view;

    ui_widget_invalidate(widget);


    ui_widget_t** it = &view->widgets;

    while (*it) {

        if (*it == widget) {
            *it = widget->next;
            break;
        }

        it = &(*it)->next;
    }

    view->widgets_tail = NULL;

    for (ui_widget_t* w = view->widgets; w; w = w->next) {
        view->widgets_tail = w;
    }


    if (view->hovered == widget) {
        view->hovered = NULL;
    }

    if (view->pressed == widget) {
        view->pressed = NULL;
    }

    if (view->focused == widget) {
        view->focused = NULL;
    }

    if (view->click_widget == widget) {
        view->click_widget = NULL;
    }


    if (widget->ops->destroy) {
        widget->ops->destroy(widget);
    }

    free(widget);
}


void ui_widget_draw(ui_widget_t* widget, cairo_t* cr) {

    if (widget->ops->draw) {
        widget->ops->draw(widget, cr);
    }
}


bool ui_widget_hit(const ui_widget_t* widget, int x, int y) {

    if (!widget->visible || !widget->enabled || !widget->interactive) {
        return false;
    }

    return x >= widget->rect.x && y >= widget->rect.y && x < widget->rect.x + widget->rect.width && y < widget->rect.y + widget->rect.height;
}


void ui_widget_set_rect(ui_widget_t* widget, int x, int y, int width, int height) {

    if (!widget) {
        return;
    }

    if (widget->rect.x == x && widget->rect.y == y && widget->rect.width == width && widget->rect.height == height) {
        return;
    }


    ui_widget_invalidate(widget);

    widget->rect.x      = x;
    widget->rect.y      = y;
    widget->rect.width  = width;
    widget->rect.height = height;

    ui_widget_invalidate(widget);
}


void ui_widget_place(ui_widget_t* widget, ui_rect_t rect) {
    ui_widget_set_rect(widget, rect.x, rect.y, rect.width, rect.height);
}


ui_rect_t ui_widget_rect(const ui_widget_t* widget) {

    if (!widget) {

        ui_rect_t empty = {0, 0, 0, 0};

        return empty;
    }

    return widget->rect;
}


/**
 * @brief Gives up the press, the hover and the keyboard focus a widget holds in its view.
 *
 * @param widget The widget leaving the running for input, by being hidden or disabled.
 */

static void ui_widget_release(ui_widget_t* widget) {

    if (widget->view->pressed == widget) {

        if (widget->ops->on_release) {
            widget->ops->on_release(widget, 0, 0, false, 0);
        }

        widget->view->pressed = NULL;
    }

    if (widget->view->hovered == widget) {

        if (widget->ops->on_hover) {
            widget->ops->on_hover(widget, false);
        }

        widget->view->hovered = NULL;
    }

    if (widget->view->focused == widget) {

        if (widget->ops->on_focus) {
            widget->ops->on_focus(widget, false);
        }

        widget->view->focused = NULL;
    }
}


void ui_widget_set_visible(ui_widget_t* widget, bool visible) {

    if (!widget || widget->visible == visible) {
        return;
    }

    widget->visible = visible;

    if (!visible) {
        ui_widget_release(widget);
    }

    ui_widget_invalidate(widget);
}


bool ui_widget_visible(const ui_widget_t* widget) {
    return widget ? widget->visible : false;
}


void ui_widget_set_enabled(ui_widget_t* widget, bool enabled) {

    if (!widget || widget->enabled == enabled) {
        return;
    }

    widget->enabled = enabled;

    if (!enabled) {
        ui_widget_release(widget);
    }

    ui_widget_invalidate(widget);
}


bool ui_widget_enabled(const ui_widget_t* widget) {
    return widget ? widget->enabled : false;
}


void ui_widget_set_user(ui_widget_t* widget, void* user) {

    if (widget) {
        widget->user = user;
    }
}


void* ui_widget_user(const ui_widget_t* widget) {
    return widget ? widget->user : NULL;
}


ui_rect_t ui_rect_inset(ui_rect_t rect, int inset) {

    ui_rect_t r = {

        .x      = rect.x + inset,
        .y      = rect.y + inset,
        .width  = rect.width - 2 * inset,
        .height = rect.height - 2 * inset,
    };

    if (r.width < 0) {
        r.width = 0;
    }

    if (r.height < 0) {
        r.height = 0;
    }

    return r;
}


ui_grid_t ui_grid(ui_rect_t bounds, int columns, int rows, int gap) {

    ui_grid_t grid = {

        .bounds     = bounds,
        .columns    = columns > 0 ? columns : 1,
        .rows       = rows > 0 ? rows : 1,
        .column_gap = gap,
        .row_gap    = gap,
    };

    return grid;
}


/**
 * @brief Reports where one track of a grid starts, sharing the remainder out across the tracks.
 *
 * @param origin The near edge of the axis.
 * @param extent The pixels to fill, gaps included.
 * @param gap The space between two tracks.
 * @param count The number of tracks.
 * @param index The track to place.
 * @return The near edge of that track.
 */
static int ui_grid_track(int origin, int extent, int gap, int count, int index) {

    const int inner = extent - gap * (count - 1);

    return origin + (inner * index) / count + gap * index;
}


ui_rect_t ui_grid_cell(const ui_grid_t* grid, int column, int row, int colspan, int rowspan) {

    ui_rect_t cell = {0, 0, 0, 0};

    if (!grid) {
        return cell;
    }

    if (colspan < 1) {
        colspan = 1;
    }

    if (rowspan < 1) {
        rowspan = 1;
    }


    if (column < 0 || row < 0 || column + colspan > grid->columns || row + rowspan > grid->rows) {
        return cell;
    }


    const int x0 = ui_grid_track(grid->bounds.x, grid->bounds.width, grid->column_gap, grid->columns, column);
    const int y0 = ui_grid_track(grid->bounds.y, grid->bounds.height, grid->row_gap, grid->rows, row);

    const int x1 = ui_grid_track(grid->bounds.x, grid->bounds.width, grid->column_gap, grid->columns, column + colspan) - grid->column_gap;
    const int y1 = ui_grid_track(grid->bounds.y, grid->bounds.height, grid->row_gap, grid->rows, row + rowspan) - grid->row_gap;

    cell.x      = x0;
    cell.y      = y0;
    cell.width  = x1 - x0;
    cell.height = y1 - y0;

    return cell;
}
