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

#include <aplus/input.h>

#include "ui_widget_internal.h"


#define UI_LIST_SCROLLBAR_WIDTH 10
#define UI_LIST_SCROLLBAR_MIN   24

#define UI_LIST_PADDING 8
#define UI_LIST_GAP     12

/**
 * @brief How much shorter than its row an icon is drawn, and how far the name sits from it.
 */
#define UI_LIST_ICON_INSET 4
#define UI_LIST_ICON_GAP   8

/**
 * @brief How many rows one wheel detent moves.
 */
#define UI_LIST_WHEEL_ROWS 3


static void ui_list_draw(ui_widget_t* widget, cairo_t* cr);
static bool ui_list_on_press(ui_widget_t* widget, int x, int y);
static bool ui_list_on_drag(ui_widget_t* widget, int x, int y, bool inside);
static bool ui_list_on_release(ui_widget_t* widget, int x, int y, bool inside, int clicks);
static bool ui_list_on_scroll(ui_widget_t* widget, int delta);
static bool ui_list_on_key(ui_widget_t* widget, uint16_t vkey, bool down);
static bool ui_list_on_focus(ui_widget_t* widget, bool focused);
static void ui_list_on_destroy(ui_widget_t* widget);


static const ui_widget_ops_t ui_list_ops = {

    .draw       = ui_list_draw,
    .on_press   = ui_list_on_press,
    .on_drag    = ui_list_on_drag,
    .on_release = ui_list_on_release,
    .on_scroll  = ui_list_on_scroll,
    .on_key     = ui_list_on_key,
    .on_focus   = ui_list_on_focus,
    .destroy    = ui_list_on_destroy,
};


/**
 * @brief Reports the area the rows occupy, which is the widget inset by its border and padding.
 *
 * @param widget The list.
 * @return The rectangle, which may be empty.
 */

static ui_rect_t ui_list_rows_rect(const ui_widget_t* widget) {

    ui_rect_t rows = ui_rect_inset(widget->rect, 1);

    rows.x += UI_LIST_PADDING;
    rows.width -= UI_LIST_PADDING * 2;

    if (rows.width < 0) {
        rows.width = 0;
    }

    return rows;
}


/**
 * @brief Reports how wide the gutter the icons are drawn in is, which is nothing until a row carries one.
 *
 * @param widget The list.
 * @return The width in pixels.
 */

static int ui_list_icon_width(const ui_widget_t* widget) {

    if (!widget->list.icons) {
        return 0;
    }

    if (widget->list.icon_size > 0) {
        return widget->list.icon_size;
    }


    const int size = widget->list.row_height - UI_LIST_ICON_INSET * 2;

    return size > 8 ? size : 8;
}


static int ui_list_content_height(const ui_widget_t* widget) {
    return (int)widget->list.count * widget->list.row_height;
}


static int ui_list_max_scroll(const ui_widget_t* widget) {

    const ui_rect_t rows = ui_list_rows_rect(widget);

    const int content = ui_list_content_height(widget);

    return content > rows.height ? content - rows.height : 0;
}


/**
 * @brief Reports the scrollbar's track and thumb.
 *
 * @param widget The list.
 * @param track Receives the track, or NULL if it is not wanted.
 * @param thumb Receives the thumb, or NULL if it is not wanted.
 * @return Whether there is anything to scroll at all.
 */

static bool ui_list_scrollbar(const ui_widget_t* widget, ui_rect_t* track, ui_rect_t* thumb) {

    const int max = ui_list_max_scroll(widget);

    if (max <= 0) {
        return false;
    }


    const ui_rect_t inner = ui_rect_inset(widget->rect, 1);

    ui_rect_t t = {inner.x + inner.width - UI_LIST_SCROLLBAR_WIDTH, inner.y, UI_LIST_SCROLLBAR_WIDTH, inner.height};

    if (track) {
        *track = t;
    }


    if (thumb) {

        const int content = ui_list_content_height(widget);

        int height = content > 0 ? (t.height * t.height) / content : t.height;

        if (height < UI_LIST_SCROLLBAR_MIN) {
            height = UI_LIST_SCROLLBAR_MIN;
        }

        if (height > t.height) {
            height = t.height;
        }


        const int travel = t.height - height;

        ui_rect_t h = {t.x + 2, t.y + (travel * widget->list.scroll) / max, UI_LIST_SCROLLBAR_WIDTH - 4, height};

        *thumb = h;
    }

    return true;
}


static bool ui_list_set_scroll(ui_widget_t* widget, int scroll) {

    const int max = ui_list_max_scroll(widget);

    if (scroll > max) {
        scroll = max;
    }

    if (scroll < 0) {
        scroll = 0;
    }

    if (widget->list.scroll == scroll) {
        return false;
    }

    widget->list.scroll = scroll;

    ui_widget_invalidate(widget);

    return true;
}


/**
 * @brief Reports the row at a point, in widget coordinates.
 *
 * @param widget The list.
 * @param y The coordinate to test.
 * @return The row index, or -1 when the point is past the last row.
 */

static int ui_list_row_at(const ui_widget_t* widget, int y) {

    const ui_rect_t rows = ui_list_rows_rect(widget);

    if (widget->list.row_height <= 0) {
        return -1;
    }

    const int index = (y - rows.y + widget->list.scroll) / widget->list.row_height;

    if (index < 0 || (size_t)index >= widget->list.count) {
        return -1;
    }

    return index;
}


ui_widget_t* ui_list_create(ui_view_t* view) {

    ui_widget_t* widget = ui_widget_new(view, UI_WIDGET_LIST, &ui_list_ops, true);

    if (!widget) {
        return NULL;
    }

    widget->list.selected   = -1;
    widget->list.row_height = (int)(widget->theme->font_size * 2.0);

    return widget;
}


void ui_list_clear(ui_widget_t* widget) {

    if (!widget || widget->kind != UI_WIDGET_LIST) {
        return;
    }


    for (size_t i = 0; i < widget->list.count; i++) {

        free(widget->list.items[i].text);
        free(widget->list.items[i].detail);
        free(widget->list.items[i].icon);
    }

    widget->list.count    = 0;
    widget->list.selected = -1;
    widget->list.scroll   = 0;
    widget->list.icons    = false;

    ui_widget_invalidate(widget);
}


int ui_list_add(ui_widget_t* widget, const char* text, const char* detail, void* user) {
    return ui_list_add_icon(widget, NULL, text, detail, user);
}


int ui_list_add_icon(ui_widget_t* widget, const char* icon, const char* text, const char* detail, void* user) {

    if (!widget || widget->kind != UI_WIDGET_LIST) {
        return -1;
    }


    if (widget->list.count == widget->list.capacity) {

        const size_t capacity = widget->list.capacity ? widget->list.capacity * 2 : 32;

        ui_list_item_t* items = (ui_list_item_t*)realloc(widget->list.items, capacity * sizeof(ui_list_item_t));

        if (!items) {
            return -1;
        }

        widget->list.items    = items;
        widget->list.capacity = capacity;
    }


    ui_list_item_t* item = &widget->list.items[widget->list.count];

    item->text   = text ? strdup(text) : NULL;
    item->detail = detail ? strdup(detail) : NULL;
    item->icon   = icon ? strdup(icon) : NULL;
    item->user   = user;

    if ((text && !item->text) || (icon && !item->icon)) {

        free(item->text);
        free(item->detail);
        free(item->icon);

        return -1;
    }

    widget->list.icons = widget->list.icons || item->icon != NULL;

    ui_widget_invalidate(widget);

    return (int)widget->list.count++;
}


void ui_list_set_icon(ui_widget_t* widget, int index, const char* icon) {

    if (!widget || widget->kind != UI_WIDGET_LIST) {
        return;
    }

    if (index < 0 || (size_t)index >= widget->list.count) {
        return;
    }


    char* name = icon ? strdup(icon) : NULL;

    if (icon && !name) {
        return;
    }

    free(widget->list.items[index].icon);

    widget->list.items[index].icon = name;

    widget->list.icons = widget->list.icons || name != NULL;

    ui_widget_invalidate(widget);
}


const char* ui_list_icon(const ui_widget_t* widget, int index) {

    if (!widget || widget->kind != UI_WIDGET_LIST) {
        return NULL;
    }

    if (index < 0 || (size_t)index >= widget->list.count) {
        return NULL;
    }

    return widget->list.items[index].icon;
}


void ui_list_set_icon_size(ui_widget_t* widget, int size) {

    if (!widget || widget->kind != UI_WIDGET_LIST) {
        return;
    }

    if (size < 0) {
        size = 0;
    }

    if (widget->list.icon_size == size) {
        return;
    }

    widget->list.icon_size = size;

    ui_widget_invalidate(widget);
}


size_t ui_list_count(const ui_widget_t* widget) {

    if (!widget || widget->kind != UI_WIDGET_LIST) {
        return 0;
    }

    return widget->list.count;
}


const char* ui_list_text(const ui_widget_t* widget, int index) {

    if (!widget || widget->kind != UI_WIDGET_LIST) {
        return NULL;
    }

    if (index < 0 || (size_t)index >= widget->list.count) {
        return NULL;
    }

    return widget->list.items[index].text;
}


void* ui_list_item_user(const ui_widget_t* widget, int index) {

    if (!widget || widget->kind != UI_WIDGET_LIST) {
        return NULL;
    }

    if (index < 0 || (size_t)index >= widget->list.count) {
        return NULL;
    }

    return widget->list.items[index].user;
}


int ui_list_selected(const ui_widget_t* widget) {

    if (!widget || widget->kind != UI_WIDGET_LIST) {
        return -1;
    }

    return widget->list.selected;
}


void ui_list_scroll_to(ui_widget_t* widget, int index) {

    if (!widget || widget->kind != UI_WIDGET_LIST) {
        return;
    }

    if (index < 0 || (size_t)index >= widget->list.count) {
        return;
    }


    const ui_rect_t rows = ui_list_rows_rect(widget);

    const int top    = index * widget->list.row_height;
    const int bottom = top + widget->list.row_height;

    if (top < widget->list.scroll) {

        ui_list_set_scroll(widget, top);

    } else if (bottom > widget->list.scroll + rows.height) {

        ui_list_set_scroll(widget, bottom - rows.height);
    }
}


void ui_list_select(ui_widget_t* widget, int index) {

    if (!widget || widget->kind != UI_WIDGET_LIST) {
        return;
    }

    if (index < -1 || (size_t)index >= widget->list.count) {
        index = -1;
    }

    if (widget->list.selected == index) {
        return;
    }

    widget->list.selected = index;

    ui_list_scroll_to(widget, index);

    ui_widget_invalidate(widget);

    if (widget->list.on_select) {
        widget->list.on_select(widget, index, widget->list.on_select_user);
    }
}


void ui_list_set_row_height(ui_widget_t* widget, int height) {

    if (!widget || widget->kind != UI_WIDGET_LIST) {
        return;
    }

    if (height <= 0) {
        height = (int)(widget->theme->font_size * 2.0);
    }

    if (widget->list.row_height == height) {
        return;
    }

    widget->list.row_height = height;

    ui_list_set_scroll(widget, widget->list.scroll);

    ui_widget_invalidate(widget);
}


void ui_list_on_select(ui_widget_t* widget, ui_list_fn fn, void* user) {

    if (!widget || widget->kind != UI_WIDGET_LIST) {
        return;
    }

    widget->list.on_select      = fn;
    widget->list.on_select_user = user;
}


void ui_list_on_activate(ui_widget_t* widget, ui_list_fn fn, void* user) {

    if (!widget || widget->kind != UI_WIDGET_LIST) {
        return;
    }

    widget->list.on_activate      = fn;
    widget->list.on_activate_user = user;
}


/**
 * @brief Runs whatever a double click or Enter on the selected row is bound to.
 *
 * @param widget The list.
 */

static void ui_list_activate(ui_widget_t* widget) {

    if (widget->list.selected < 0 || !widget->list.on_activate) {
        return;
    }

    widget->list.on_activate(widget, widget->list.selected, widget->list.on_activate_user);
}


static void ui_list_draw(ui_widget_t* widget, cairo_t* cr) {

    const ui_theme_t* theme = widget->theme;

    const double radius = theme->corner_radius;


    ui_draw_rounded_rect(cr, widget->rect, radius);
    ui_draw_set_color(cr, theme->surface_sunken);
    cairo_fill(cr);


    const ui_rect_t rows = ui_list_rows_rect(widget);

    ui_rect_t track;
    ui_rect_t thumb;

    const bool bar = ui_list_scrollbar(widget, &track, &thumb);

    const int width = bar ? rows.width - UI_LIST_SCROLLBAR_WIDTH : rows.width;


    cairo_save(cr);

    ui_draw_rounded_rect(cr, widget->rect, radius);
    cairo_clip(cr);


    const int height = widget->list.row_height;

    const int icons = ui_list_icon_width(widget);

    int first = height > 0 ? widget->list.scroll / height : 0;

    if (first < 0) {
        first = 0;
    }


    for (size_t i = (size_t)first; i < widget->list.count && width > 0; i++) {

        const int top = rows.y + (int)i * height - widget->list.scroll;

        if (top >= rows.y + rows.height) {
            break;
        }


        ui_rect_t row = {rows.x, top, width, height};

        ui_color_t text = theme->text;

        if (widget->list.selected == (int)i) {

            ui_rect_t fill = {row.x - UI_LIST_PADDING / 2, row.y, row.width + UI_LIST_PADDING, row.height};

            ui_draw_rounded_rect(cr, fill, radius);
            ui_draw_set_color(cr, widget->list.focused ? theme->primary : theme->secondary);
            cairo_fill(cr);

            text = widget->list.focused ? theme->on_primary : theme->on_secondary;
        }


        if (!widget->enabled) {
            text = ui_draw_dim(text, theme);
        }


        ui_rect_t name_rect = row;

        if (icons > 0) {

            const ui_rect_t box = {row.x, row.y + (row.height - icons) / 2, icons, icons};

            ui_draw_icon(cr, box, ui_icon_load(widget->list.items[i].icon, icons));

            name_rect.x += icons + UI_LIST_ICON_GAP;
            name_rect.width -= icons + UI_LIST_ICON_GAP;
        }


        double taken = 0.0;

        const char* detail = widget->list.items[i].detail;

        if (detail && *detail) {

            taken = ui_draw_text_width(detail, theme->font_regular, theme->font_size) + UI_LIST_GAP;

            ui_color_t muted = widget->list.selected == (int)i ? text : theme->text_muted;

            if (!widget->enabled) {
                muted = ui_draw_dim(muted, theme);
            }

            ui_draw_text(cr, row, detail, theme->font_regular, theme->font_size, muted, UI_ALIGN_RIGHT);
        }


        char name[UI_LIST_TEXT_MAX];

        ui_draw_ellipsize(name, sizeof(name), widget->list.items[i].text, theme->font_regular, theme->font_size, (double)name_rect.width - taken);

        ui_draw_text(cr, name_rect, name, theme->font_regular, theme->font_size, text, UI_ALIGN_LEFT);
    }

    cairo_restore(cr);


    if (bar) {

        ui_draw_rounded_rect(cr, thumb, (double)thumb.width / 2.0);
        ui_draw_set_color(cr, theme->border);
        cairo_fill(cr);
    }


    ui_draw_rounded_rect_d(cr, widget->rect.x + 0.5, widget->rect.y + 0.5, widget->rect.width - 1.0, widget->rect.height - 1.0, radius);
    ui_draw_set_color(cr, widget->list.focused ? theme->focus_ring : theme->border);

    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
}


/**
 * @brief Moves the selection by whole rows, clamped at both ends.
 *
 * @param widget The list.
 * @param delta How many rows to move by.
 */

static void ui_list_move(ui_widget_t* widget, int delta) {

    if (widget->list.count == 0) {
        return;
    }


    int index = widget->list.selected < 0 ? 0 : widget->list.selected + delta;

    if (index < 0) {
        index = 0;
    }

    if ((size_t)index >= widget->list.count) {
        index = (int)widget->list.count - 1;
    }

    ui_list_select(widget, index);
}


static bool ui_list_on_press(ui_widget_t* widget, int x, int y) {

    ui_rect_t track;
    ui_rect_t thumb;

    if (ui_list_scrollbar(widget, &track, &thumb) && x >= track.x) {

        if (y >= thumb.y && y < thumb.y + thumb.height) {

            widget->list.dragging  = true;
            widget->list.drag_grab = y - thumb.y;

            return false;
        }


        const ui_rect_t rows = ui_list_rows_rect(widget);

        return ui_list_set_scroll(widget, widget->list.scroll + (y < thumb.y ? -rows.height : rows.height));
    }


    const int index = ui_list_row_at(widget, y);

    if (index < 0) {
        return false;
    }

    ui_list_select(widget, index);

    return false;
}


static bool ui_list_on_drag(ui_widget_t* widget, int x, int y, bool inside) {

    (void)x;
    (void)inside;

    if (!widget->list.dragging) {
        return false;
    }


    ui_rect_t track;
    ui_rect_t thumb;

    if (!ui_list_scrollbar(widget, &track, &thumb)) {
        return false;
    }


    const int travel = track.height - thumb.height;

    if (travel <= 0) {
        return false;
    }

    return ui_list_set_scroll(widget, ((y - widget->list.drag_grab - track.y) * ui_list_max_scroll(widget)) / travel);
}


static bool ui_list_on_release(ui_widget_t* widget, int x, int y, bool inside, int clicks) {

    (void)x;

    const bool dragged = widget->list.dragging;

    widget->list.dragging = false;

    if (dragged || !inside || clicks < 2) {
        return false;
    }

    if (ui_list_row_at(widget, y) == widget->list.selected) {
        ui_list_activate(widget);
    }

    return false;
}


static bool ui_list_on_scroll(ui_widget_t* widget, int delta) {
    return ui_list_set_scroll(widget, widget->list.scroll - delta * widget->list.row_height * UI_LIST_WHEEL_ROWS);
}


static bool ui_list_on_key(ui_widget_t* widget, uint16_t vkey, bool down) {

    if (!down) {
        return false;
    }


    const ui_rect_t rows = ui_list_rows_rect(widget);

    const int page = widget->list.row_height > 0 && rows.height > widget->list.row_height ? rows.height / widget->list.row_height : 1;


    switch (vkey) {

        case KEY_UP:
            ui_list_move(widget, -1);
            return true;

        case KEY_DOWN:
            ui_list_move(widget, 1);
            return true;

        case KEY_PAGEUP:
            ui_list_move(widget, -page);
            return true;

        case KEY_PAGEDOWN:
            ui_list_move(widget, page);
            return true;

        case KEY_HOME:
            ui_list_move(widget, -(int)widget->list.count);
            return true;

        case KEY_END:
            ui_list_move(widget, (int)widget->list.count);
            return true;

        case KEY_ENTER:
        case KEY_KPENTER:
            ui_list_activate(widget);
            return true;

        default:
            return false;
    }
}


static bool ui_list_on_focus(ui_widget_t* widget, bool focused) {

    if (widget->list.focused == focused) {
        return false;
    }

    widget->list.focused = focused;

    return true;
}


static void ui_list_on_destroy(ui_widget_t* widget) {

    for (size_t i = 0; i < widget->list.count; i++) {

        free(widget->list.items[i].text);
        free(widget->list.items[i].detail);
        free(widget->list.items[i].icon);
    }

    free(widget->list.items);
}
