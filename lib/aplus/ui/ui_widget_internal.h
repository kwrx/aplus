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

#include <aplus/ui-anim.h>
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
    bool (*on_motion)(ui_widget_t* widget, int x, int y);
    bool (*on_press)(ui_widget_t* widget, int x, int y);

    bool (*on_drag)(ui_widget_t* widget, int x, int y, bool inside);

    bool (*on_release)(ui_widget_t* widget, int x, int y, bool inside, int clicks);

    bool (*on_scroll)(ui_widget_t* widget, int delta);

    bool (*on_key)(ui_widget_t* widget, uint16_t vkey, bool down);
    bool (*on_focus)(ui_widget_t* widget, bool focused);

    int (*tick)(ui_widget_t* widget, uint64_t now);

    void (*restyle)(ui_widget_t* widget);

    void (*destroy)(ui_widget_t* widget);

} ui_widget_ops_t;


struct ui_widget {

    ui_view_t* view;
    ui_widget_kind_t kind;

    const ui_widget_ops_t* ops;

    ui_rect_t rect;

    bool visible;
    bool enabled;

    bool interactive;

    ui_cursor_t cursor;

    void* user;

    const ui_theme_t* theme;

    union {

        struct {

            ui_color_t color;
            ui_color_t border_color;

            bool color_set;
            bool border_set;

            double radius;
            double border_width;

        } panel;

        struct {

            char text[UI_LABEL_TEXT_MAX];

            ui_color_t color;
            bool color_set;

            ui_align_t align;

            ui_font_weight_t weight;

            double size;
            bool size_set;

            int padding;

        } label;

        struct {

            char text[UI_LABEL_TEXT_MAX];

            ui_button_style_t style;

            ui_font_weight_t weight;

            double size;
            bool size_set;

            char icon[UI_ICON_NAME_MAX];
            int icon_size;

            ui_action_fn on_click;

            bool hovered;
            bool pressed;
            bool held;

            ui_anim_t hover_anim;
            ui_anim_t press_anim;

        } button;

        struct {

            ui_list_item_t* items;
            size_t count;
            size_t capacity;

            int selected;

            int row_height;
            bool row_height_set;

            int icon_size;
            bool icons;

            int scroll;
            ui_anim_t scroll_anim;

            int hover;
            ui_anim_t hover_anim;

            int hover_prev;
            ui_anim_t hover_prev_anim;

            ui_anim_t select_anim;

            int pointer_x;
            int pointer_y;
            bool pointer_in;

            bool focused;

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
            bool size_set;

            size_t caret;

            int scroll;

            bool focused;

            bool caret_on;
            uint64_t caret_at;

            ui_anim_t focus_anim;

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

    const ui_theme_t* theme;

    cairo_surface_t* surface;
    cairo_t* cr;

    ui_widget_t* widgets;
    ui_widget_t* widgets_tail;

    ui_widget_t* hovered;
    ui_widget_t* pressed;
    ui_widget_t* focused;

    ui_cursor_t cursor;

    ui_widget_t* click_widget;
    uint64_t click_time;
    int click_x;
    int click_y;
    int click_count;

    ui_layout_fn layout;
    void* layout_user;

    ui_key_fn key;
    void* key_user;

    ui_theme_fn restyle;
    void* restyle_user;

    ui_damage_t damage;

    uint64_t frame_time;

    bool active;

    bool closed;
};


/**
 * @brief Repaints part of a widget, clipped to it. Implemented in ui_view.c.
 *
 * @param widget The widget.
 * @param rect The part to repaint, in view coordinates.
 */
void ui_widget_invalidate_rect(ui_widget_t* widget, ui_rect_t rect);

/**
 * @brief Implemented in ui_widget.c.
 */
ui_widget_t* ui_widget_new(ui_view_t* view, ui_widget_kind_t kind, const ui_widget_ops_t* ops, bool interactive);
void ui_widget_draw(ui_widget_t* widget, cairo_t* cr);
bool ui_widget_hit(const ui_widget_t* widget, int x, int y);

/**
 * @brief Reports whether a transition has frames the last paint has not shown yet.
 *
 * @param widget The widget the transition belongs to.
 * @param anim The transition.
 * @return true while the view still has to paint it.
 */
bool ui_widget_animating(const ui_widget_t* widget, const ui_anim_t* anim);

/**
 * @brief Reports a transition's value at the frame being painted.
 *
 * @param widget The widget the transition belongs to.
 * @param anim The transition.
 * @return The value.
 */
double ui_widget_anim(const ui_widget_t* widget, const ui_anim_t* anim);

/**
 * @brief Implemented in ui_draw.c. The rounded rectangle and the fonts are in <aplus/ui-draw.h>.
 */
void ui_draw_set_color(cairo_t* cr, ui_color_t color);
const char* ui_draw_font(const ui_theme_t* theme, ui_font_weight_t weight);
ui_color_t ui_color_alpha(ui_color_t color, double alpha);
void ui_draw_text(cairo_t* cr, ui_rect_t rect, const char* text, const char* font, double size, ui_color_t color, ui_align_t align);

/**
 * @brief Pulls a colour back towards the backdrop, which is how a disabled control is drawn.
 *
 * @param color The colour to fade.
 * @param theme The theme holding the backdrop.
 * @return The faded colour.
 */
ui_color_t ui_draw_dim(ui_color_t color, const ui_theme_t* theme);

#endif
