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

#ifndef _APLUS_UI_WIDGET_INTERNAL_H
#define _APLUS_UI_WIDGET_INTERNAL_H

#include <stdbool.h>

#include <cairo/cairo.h>

#include <aplus/ui-draw.h>
#include <aplus/ui-keymap.h>
#include <aplus/ui-widgets.h>
#include <aplus/ui.h>

#include "ui_internal.h"


#define UI_LABEL_TEXT_MAX 128

/**
 * @brief The widest row a list will shape, past which a name is ellipsised anyway.
 */
#define UI_LIST_TEXT_MAX 256

/**
 * @brief How much text an entry holds, terminator included.
 */
#define UI_ENTRY_TEXT_MAX 256


/**
 * @brief One row of a list, owning its own strings since a caller's may not outlive the widget.
 */

typedef struct {

    char* text;
    char* detail;

    //? The icon's name rather than the icon, resolved at paint time out of the shared
    //? cache: a row outlives any one surface the cache is holding.
    char* icon;

    void* user;

} ui_list_item_t;


typedef enum {

    UI_WIDGET_PANEL = 0,
    UI_WIDGET_LABEL,
    UI_WIDGET_BUTTON,
    UI_WIDGET_LIST,
    UI_WIDGET_ENTRY,

} ui_widget_kind_t;


/**
 * @brief What a widget kind provides, so that the view drives every kind through one table.
 *
 * Every hook is optional, and every hook that reacts to state returns whether a repaint is due.
 */

typedef struct {

    void (*draw)(ui_widget_t* widget, cairo_t* cr);

    bool (*on_hover)(ui_widget_t* widget, bool hovered);
    bool (*on_press)(ui_widget_t* widget, int x, int y);

    //? Motion while the widget holds the press, whether or not the pointer is still over it.
    bool (*on_drag)(ui_widget_t* widget, int x, int y, bool inside);

    //? `clicks` is 0 when the press was cancelled rather than released over the widget.
    bool (*on_release)(ui_widget_t* widget, int x, int y, bool inside, int clicks);

    bool (*on_scroll)(ui_widget_t* widget, int delta);

    //? Reports whether the key was handled, and invalidates itself if it changed anything.
    bool (*on_key)(ui_widget_t* widget, uint16_t vkey, bool down);
    bool (*on_focus)(ui_widget_t* widget, bool focused);

    //? Asked before the loop blocks, and again every time the deadline it named passes.
    //? Reports the milliseconds until the widget next has to be repainted, or -1 when it
    //? is not animating; whatever it changed it invalidates for itself.
    int (*tick)(ui_widget_t* widget, uint64_t now);

    void (*destroy)(ui_widget_t* widget);

} ui_widget_ops_t;


struct ui_widget {

    ui_view_t* view;
    ui_widget_kind_t kind;

    const ui_widget_ops_t* ops;

    ui_rect_t rect;

    bool visible;
    bool enabled;

    //? Whether the widget is in the running for pointer events at all. A panel and a label
    //? are not, which is what lets them sit under a button without swallowing its clicks.
    bool interactive;

    void* user;

    //? The view's theme, copied here so that a widget never has to reach back through the
    //? view to paint itself.
    const ui_theme_t* theme;

    union {

        struct {

            ui_color_t color;
            ui_color_t border_color;

            double radius;
            double border_width;

        } panel;

        struct {

            char text[UI_LABEL_TEXT_MAX];

            ui_color_t color;
            ui_align_t align;

            ui_font_weight_t weight;
            double size;

            int padding;

        } label;

        struct {

            char text[UI_LABEL_TEXT_MAX];

            ui_button_style_t style;

            ui_font_weight_t weight;
            double size;

            ui_action_fn on_click;

            //? Pointer state, kept apart from `held`: a key binding lights a button without
            //? the pointer being anywhere near it, and the two must not cancel each other.
            bool hovered;
            bool pressed;
            bool held;

        } button;

        struct {

            ui_list_item_t* items;
            size_t count;
            size_t capacity;

            int selected;
            int row_height;

            //? The gutter the icons are drawn in, 0 for the one the row height implies,
            //? and only reserved at all once a row has been given an icon.
            int icon_size;
            bool icons;

            //? Pixels of content scrolled off the top, never past what the content allows.
            int scroll;

            bool focused;

            //? Set while the scrollbar thumb is being dragged, with the grab point inside it.
            bool dragging;
            int drag_grab;

            ui_list_fn on_select;
            void* on_select_user;

            ui_list_fn on_activate;
            void* on_activate_user;

        } list;

        struct {

            char text[UI_ENTRY_TEXT_MAX];
            char placeholder[UI_LABEL_TEXT_MAX];

            ui_font_weight_t weight;
            double size;

            //? Where the caret sits, as a byte offset into the text.
            size_t caret;

            //? Pixels of text scrolled off the left, kept at whatever holds the caret in
            //? view: a field narrower than what has been typed into it has to move.
            int scroll;

            bool focused;

            //? Whether the caret is in the shown half of its blink, and when that half
            //? began. Editing restarts it shown: a caret that vanishes under the keys
            //? being typed reads as a dropped keystroke.
            bool caret_on;
            uint64_t caret_at;

            //? Opened the first time a key arrives rather than at construction, so a view
            //? that never takes text never reads a file.
            ui_keymap_t* keymap;
            bool keymap_tried;

            ui_action_fn on_change;
            void* on_change_user;

            ui_action_fn on_submit;
            void* on_submit_user;

        } entry;
    };

    struct ui_widget* next;
};


struct ui_view {

    ui_window_t* window;

    //? Snapshotted when the view is created rather than read through ui_theme() at paint
    //? time, so that a window cannot change appearance underneath itself because something
    //? else swapped the global theme.
    const ui_theme_t* theme;

    cairo_surface_t* surface;
    cairo_t* cr;

    //? In paint order, head first. Hit testing walks it to the end and keeps the last
    //? match, which is the topmost one.
    ui_widget_t* widgets;
    ui_widget_t* widgets_tail;

    ui_widget_t* hovered;
    ui_widget_t* pressed;
    ui_widget_t* focused;

    //? The last press, against which the next one is counted as a double click.
    ui_widget_t* click_widget;
    uint64_t click_time;
    int click_x;
    int click_y;
    int click_count;

    ui_layout_fn layout;
    void* layout_user;

    ui_key_fn key;
    void* key_user;

    ui_damage_t damage;

    bool closed;
};


/**
 * @brief Implemented in ui_view.c.
 */
uint64_t ui_now_ms(void);

/**
 * @brief Implemented in ui_widget.c.
 */
ui_widget_t* ui_widget_new(ui_view_t* view, ui_widget_kind_t kind, const ui_widget_ops_t* ops, bool interactive);
void ui_widget_draw(ui_widget_t* widget, cairo_t* cr);
bool ui_widget_hit(const ui_widget_t* widget, int x, int y);

/**
 * @brief Implemented in ui_draw.c. The rounded rectangle and the fonts are in <aplus/ui-draw.h>.
 */
void ui_draw_set_color(cairo_t* cr, ui_color_t color);
void ui_draw_text(cairo_t* cr, ui_rect_t rect, const char* text, const char* font, double size, ui_color_t color, ui_align_t align);
double ui_draw_text_width(const char* text, const char* font, double size);
void ui_draw_ellipsize(char* out, size_t size, const char* text, const char* font, double fsize, double max_width);

/**
 * @brief Pulls a colour back towards the backdrop, which is how a disabled control is drawn.
 *
 * @param color The colour to fade.
 * @param theme The theme holding the backdrop.
 * @return The faded colour.
 */
ui_color_t ui_draw_dim(ui_color_t color, const ui_theme_t* theme);

#endif
