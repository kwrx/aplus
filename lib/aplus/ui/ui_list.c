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
#include <stdlib.h>
#include <string.h>

#include <aplus/input.h>

#include "ui_widget_internal.h"


#define UI_LIST_SCROLLBAR_WIDTH 10
#define UI_LIST_SCROLLBAR_THUMB 6
#define UI_LIST_SCROLLBAR_MIN   24

#define UI_LIST_PADDING 12
#define UI_LIST_GAP     12

/**
 * @brief How far a highlighted row's fill sits inside the list, and how its corners are rounded.
 */
#define UI_LIST_HIGHLIGHT_INSET  4
#define UI_LIST_HIGHLIGHT_RADIUS 6

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
static bool ui_list_on_hover(ui_widget_t* widget, bool hovered);
static bool ui_list_on_motion(ui_widget_t* widget, int x, int y);
static bool ui_list_on_press(ui_widget_t* widget, int x, int y);
static bool ui_list_on_drag(ui_widget_t* widget, int x, int y, bool inside);
static bool ui_list_on_release(ui_widget_t* widget, int x, int y, bool inside, int clicks);
static bool ui_list_on_scroll(ui_widget_t* widget, int delta);
static bool ui_list_on_key(ui_widget_t* widget, uint16_t vkey, bool down);
static bool ui_list_on_focus(ui_widget_t* widget, bool focused);
static int ui_list_tick(ui_widget_t* widget, uint64_t now);
static void ui_list_on_destroy(ui_widget_t* widget);


static const ui_widget_ops_t ui_list_ops = {

    .draw       = ui_list_draw,
    .on_hover   = ui_list_on_hover,
    .on_motion  = ui_list_on_motion,
    .on_press   = ui_list_on_press,
    .on_drag    = ui_list_on_drag,
    .on_release = ui_list_on_release,
    .on_scroll  = ui_list_on_scroll,
    .on_key     = ui_list_on_key,
    .on_focus   = ui_list_on_focus,
    .tick       = ui_list_tick,
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

    rows.y += UI_LIST_INSET;
    rows.height -= UI_LIST_INSET * 2;

    if (rows.width < 0) {
        rows.width = 0;
    }

    if (rows.height < 0) {
        rows.height = 0;
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

    ui_rect_t t = {inner.x + inner.width - UI_LIST_SCROLLBAR_WIDTH, inner.y + UI_LIST_INSET, UI_LIST_SCROLLBAR_WIDTH, inner.height - UI_LIST_INSET * 2};

    if (t.height <= 0) {
        return false;
    }

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

        ui_rect_t h = {t.x + (UI_LIST_SCROLLBAR_WIDTH - UI_LIST_SCROLLBAR_THUMB) / 2, t.y + (travel * widget->list.scroll) / max, UI_LIST_SCROLLBAR_THUMB, height};

        *thumb = h;
    }

    return true;
}


/**
 * @brief Holds an offset within what the content allows.
 *
 * @param widget The list.
 * @param scroll The offset.
 * @return The clamped offset.
 */
static int ui_list_clamp_scroll(const ui_widget_t* widget, int scroll) {

    const int max = ui_list_max_scroll(widget);

    if (scroll > max) {
        scroll = max;
    }

    if (scroll < 0) {
        scroll = 0;
    }

    return scroll;
}


/**
 * @brief Scrolls at once, cutting short any glide under way.
 *
 * @param widget The list.
 * @param scroll The offset to scroll to, clamped to the content.
 * @return Whether the offset changed.
 */
static bool ui_list_set_scroll(ui_widget_t* widget, int scroll) {

    scroll = ui_list_clamp_scroll(widget, scroll);

    ui_anim_set(&widget->list.scroll_anim, scroll);

    if (widget->list.scroll == scroll) {
        return false;
    }

    widget->list.scroll = scroll;

    ui_widget_invalidate(widget);

    return true;
}


/**
 * @brief Reports where the list is scrolling to, which is where it is when nothing is gliding.
 *
 * @param widget The list.
 * @return The offset.
 */
static int ui_list_scroll_target(const ui_widget_t* widget) {
    return (int)lround(ui_anim_target(&widget->list.scroll_anim));
}


/**
 * @brief Eases the list towards an offset, which the tick then steps through frame by frame.
 *
 * @param widget The list.
 * @param scroll The offset to end at, clamped to the content.
 */
static void ui_list_glide(ui_widget_t* widget, int scroll) {

    scroll = ui_list_clamp_scroll(widget, scroll);

    if (scroll == ui_list_scroll_target(widget)) {
        return;
    }

    ui_anim_to(&widget->list.scroll_anim, scroll, UI_MOTION_NORMAL, ui_anim_now());
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
    widget->list.hover      = -1;
    widget->list.hover_prev = -1;
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

    widget->list.count      = 0;
    widget->list.selected   = -1;
    widget->list.scroll     = 0;
    widget->list.hover      = -1;
    widget->list.hover_prev = -1;
    widget->list.icons      = false;

    ui_anim_set(&widget->list.scroll_anim, 0);
    ui_anim_set(&widget->list.hover_anim, 0);
    ui_anim_set(&widget->list.hover_prev_anim, 0);

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

    if (rows.height <= 0) {
        return;
    }


    const int top    = index * widget->list.row_height;
    const int bottom = top + widget->list.row_height;
    const int target = ui_list_scroll_target(widget);

    if (top < target) {

        ui_list_glide(widget, top);

    } else if (bottom > target + rows.height) {

        ui_list_glide(widget, bottom - rows.height);
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

    const int previous = widget->list.selected;

    widget->list.selected = index;

    if (index >= 0) {

        const double top = (double)index * widget->list.row_height;

        if (previous >= 0) {
            ui_anim_to(&widget->list.select_anim, top, UI_MOTION_FAST, ui_anim_now());
        } else {
            ui_anim_set(&widget->list.select_anim, top);
        }
    }

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

    ui_anim_set(&widget->list.select_anim, (double)widget->list.selected * height);

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


/**
 * @brief Reports the fill a highlighted row gets, inset from the list's sides and from its neighbours.
 *
 * @param widget The list.
 * @param top The row's top edge, in view coordinates.
 * @param width The width the rows are laid out at, less the scrollbar when there is one.
 * @return The rectangle.
 */
static ui_rect_t ui_list_highlight_rect(const ui_widget_t* widget, int top, int width) {

    const ui_rect_t rows = ui_list_rows_rect(widget);

    ui_rect_t fill = {

        .x      = rows.x - UI_LIST_PADDING + UI_LIST_HIGHLIGHT_INSET,
        .y      = top + 1,
        .width  = width + 2 * (UI_LIST_PADDING - UI_LIST_HIGHLIGHT_INSET),
        .height = widget->list.row_height - 2,
    };

    return fill;
}


/**
 * @brief Reports the strip one row covers across the whole list, for repainting it alone.
 *
 * @param widget The list.
 * @param index The row.
 * @return The rectangle, in view coordinates.
 */
static ui_rect_t ui_list_row_strip(const ui_widget_t* widget, int index) {

    const ui_rect_t rows = ui_list_rows_rect(widget);

    ui_rect_t strip = {

        .x      = widget->rect.x,
        .y      = rows.y + index * widget->list.row_height - widget->list.scroll,
        .width  = widget->rect.width,
        .height = widget->list.row_height,
    };

    return strip;
}


/**
 * @brief Reports whether the selection is drawn in the accent, which takes the keyboard and the window's focus both.
 *
 * @param widget The list.
 * @return true when the list is focused in a focused window.
 */
static bool ui_list_lit(const ui_widget_t* widget) {
    return widget->list.focused && widget->view->active;
}


/**
 * @brief Paints a hover wash under one row, at the strength its transition has reached.
 *
 * @param widget The list.
 * @param cr The cairo context to draw with.
 * @param index The row, or -1 for none.
 * @param anim The transition the strength comes from.
 * @param width The width the rows are laid out at.
 */
static void ui_list_draw_hover(ui_widget_t* widget, cairo_t* cr, int index, const ui_anim_t* anim, int width) {

    if (index < 0 || (size_t)index >= widget->list.count || index == widget->list.selected) {
        return;
    }


    const double strength = ui_widget_anim(widget, anim);

    if (strength <= 0.0) {
        return;
    }


    const ui_rect_t rows = ui_list_rows_rect(widget);

    ui_draw_rounded_rect(cr, ui_list_highlight_rect(widget, rows.y + index * widget->list.row_height - widget->list.scroll, width), UI_LIST_HIGHLIGHT_RADIUS);
    ui_draw_set_color(cr, ui_color_alpha(widget->theme->hover, strength));
    cairo_fill(cr);
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

    const int height = widget->list.row_height;


    cairo_save(cr);

    ui_draw_rounded_rect(cr, widget->rect, radius);
    cairo_clip(cr);


    if (width > 0 && height > 0) {

        ui_list_draw_hover(widget, cr, widget->list.hover_prev, &widget->list.hover_prev_anim, width);
        ui_list_draw_hover(widget, cr, widget->list.hover, &widget->list.hover_anim, width);


        if (widget->list.selected >= 0 && (size_t)widget->list.selected < widget->list.count) {

            const int top = rows.y + (int)lround(ui_widget_anim(widget, &widget->list.select_anim)) - widget->list.scroll;

            ui_draw_rounded_rect(cr, ui_list_highlight_rect(widget, top, width), UI_LIST_HIGHLIGHT_RADIUS);
            ui_draw_set_color(cr, ui_list_lit(widget) ? theme->primary : theme->secondary);
            cairo_fill(cr);
        }
    }


    double clip_x0;
    double clip_y0;
    double clip_x1;
    double clip_y1;

    cairo_clip_extents(cr, &clip_x0, &clip_y0, &clip_x1, &clip_y1);


    const int icons = ui_list_icon_width(widget);

    int first = height > 0 ? ((int)floor(clip_y0) - rows.y + widget->list.scroll) / height : 0;

    if (first < 0) {
        first = 0;
    }


    for (size_t i = (size_t)first; i < widget->list.count && width > 0 && height > 0; i++) {

        const int top = rows.y + (int)i * height - widget->list.scroll;

        if (top >= clip_y1) {
            break;
        }


        ui_rect_t row = {rows.x, top, width, height};

        const bool selected = widget->list.selected == (int)i;

        ui_color_t text = theme->text;

        if (selected) {
            text = ui_list_lit(widget) ? theme->on_primary : theme->on_secondary;
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

            ui_color_t muted = selected ? ui_color_alpha(text, 0.8) : theme->text_muted;

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

        const bool hot = widget->list.dragging || (widget->list.pointer_in && widget->list.pointer_x >= track.x);

        ui_draw_rounded_rect(cr, thumb, (double)thumb.width / 2.0);
        ui_draw_set_color(cr, ui_color_alpha(theme->text, hot ? 0.45 : 0.22));
        cairo_fill(cr);
    }


    ui_draw_rounded_rect_d(cr, widget->rect.x + 0.5, widget->rect.y + 0.5, widget->rect.width - 1.0, widget->rect.height - 1.0, radius);
    ui_draw_set_color(cr, theme->border);

    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
}


/**
 * @brief Moves the hover onto a row, fading the one it leaves out as the new one fades in.
 *
 * @param widget The list.
 * @param index The row under the pointer, or -1 for none.
 */
static void ui_list_set_hover(ui_widget_t* widget, int index) {

    if (widget->list.hover == index) {
        return;
    }


    const uint64_t now = ui_anim_now();

    if (widget->list.hover_prev >= 0) {
        ui_widget_invalidate_rect(widget, ui_list_row_strip(widget, widget->list.hover_prev));
    }


    const int leaving      = widget->list.hover;
    const ui_anim_t fading = widget->list.hover_anim;
    const bool coming_back = index >= 0 && index == widget->list.hover_prev;

    if (coming_back) {
        widget->list.hover_anim = widget->list.hover_prev_anim;
    } else {
        ui_anim_set(&widget->list.hover_anim, 0.0);
    }

    widget->list.hover = index;

    if (index >= 0) {
        ui_anim_to(&widget->list.hover_anim, 1.0, UI_MOTION_FAST, now);
    }


    widget->list.hover_prev      = leaving;
    widget->list.hover_prev_anim = fading;

    ui_anim_to(&widget->list.hover_prev_anim, 0.0, UI_MOTION_NORMAL, now);


    if (leaving >= 0) {
        ui_widget_invalidate_rect(widget, ui_list_row_strip(widget, leaving));
    }

    if (index >= 0) {
        ui_widget_invalidate_rect(widget, ui_list_row_strip(widget, index));
    }
}


/**
 * @brief Points the hover at whatever row is under the pointer now, which scrolling changes too.
 *
 * @param widget The list.
 */
static void ui_list_track_hover(ui_widget_t* widget) {

    int index = -1;

    if (widget->list.pointer_in && widget->enabled && !widget->list.dragging) {

        ui_rect_t track;

        if (!ui_list_scrollbar(widget, &track, NULL) || widget->list.pointer_x < track.x) {
            index = ui_list_row_at(widget, widget->list.pointer_y);
        }
    }

    ui_list_set_hover(widget, index);
}


/**
 * @brief Lets the hover go when the pointer leaves; arriving is left to the motion that follows.
 *
 * @param widget The list.
 * @param hovered Whether the pointer is over the list.
 * @return false, since whatever changed has been invalidated already.
 */
static bool ui_list_on_hover(ui_widget_t* widget, bool hovered) {

    if (hovered) {
        return false;
    }


    ui_rect_t track;

    if (ui_list_scrollbar(widget, &track, NULL)) {
        ui_widget_invalidate_rect(widget, track);
    }

    widget->list.pointer_in = false;

    ui_list_track_hover(widget);

    return false;
}


/**
 * @brief Follows the pointer from row to row, and on and off the scrollbar.
 *
 * @param widget The list.
 * @param x The pointer position.
 * @param y The pointer position.
 * @return false, since whatever changed has been invalidated already.
 */
static bool ui_list_on_motion(ui_widget_t* widget, int x, int y) {

    ui_rect_t track;

    const bool bar = ui_list_scrollbar(widget, &track, NULL);

    const bool was_hot = bar && widget->list.pointer_in && widget->list.pointer_x >= track.x;

    widget->list.pointer_x  = x;
    widget->list.pointer_y  = y;
    widget->list.pointer_in = true;

    ui_list_track_hover(widget);

    if (bar && was_hot != (x >= track.x)) {
        ui_widget_invalidate_rect(widget, track);
    }

    return false;
}


/**
 * @brief Steps a glide one frame on, and keeps painting while a hover or the selection is still moving.
 *
 * @param widget The list.
 * @param now The loop's clock reading.
 * @return The delay until the next frame, or -1 when the list is still.
 */
static int ui_list_tick(ui_widget_t* widget, uint64_t now) {

    int next = -1;


    if (widget->list.scroll_anim.duration) {

        const int scroll = ui_list_clamp_scroll(widget, (int)lround(ui_anim_value(&widget->list.scroll_anim, now)));

        if (scroll != widget->list.scroll) {

            widget->list.scroll = scroll;

            ui_widget_invalidate(widget);
            ui_list_track_hover(widget);
        }

        if (ui_anim_running(&widget->list.scroll_anim, now)) {
            next = UI_ANIM_FRAME_MS;
        } else {
            ui_anim_set(&widget->list.scroll_anim, widget->list.scroll);
        }
    }


    if (ui_widget_animating(widget, &widget->list.hover_anim) && widget->list.hover >= 0) {

        ui_widget_invalidate_rect(widget, ui_list_row_strip(widget, widget->list.hover));

        next = UI_ANIM_FRAME_MS;
    }

    if (ui_widget_animating(widget, &widget->list.hover_prev_anim) && widget->list.hover_prev >= 0) {

        ui_widget_invalidate_rect(widget, ui_list_row_strip(widget, widget->list.hover_prev));

        next = UI_ANIM_FRAME_MS;
    }


    if (ui_widget_animating(widget, &widget->list.select_anim)) {

        const ui_rect_t rows = ui_list_rows_rect(widget);

        const double from = widget->list.select_anim.from;
        const double to   = widget->list.select_anim.to;

        const int top    = rows.y + (int)floor(from < to ? from : to) - widget->list.scroll;
        const int bottom = rows.y + (int)ceil(from < to ? to : from) + widget->list.row_height - widget->list.scroll;

        ui_rect_t sweep = {widget->rect.x, top, widget->rect.width, bottom - top};

        ui_widget_invalidate_rect(widget, sweep);

        next = UI_ANIM_FRAME_MS;
    }

    return next;
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

            ui_list_set_hover(widget, -1);
            ui_widget_invalidate_rect(widget, track);

            return false;
        }


        const ui_rect_t rows = ui_list_rows_rect(widget);

        ui_list_glide(widget, ui_list_scroll_target(widget) + (y < thumb.y ? -rows.height : rows.height));

        return false;
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

    if (dragged) {

        ui_list_track_hover(widget);

        return true;
    }

    if (!inside || clicks < 2) {
        return false;
    }

    if (ui_list_row_at(widget, y) == widget->list.selected) {
        ui_list_activate(widget);
    }

    return false;
}


static bool ui_list_on_scroll(ui_widget_t* widget, int delta) {

    ui_list_glide(widget, ui_list_scroll_target(widget) - delta * widget->list.row_height * UI_LIST_WHEEL_ROWS);

    return false;
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
