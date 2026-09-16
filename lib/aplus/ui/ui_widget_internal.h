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

#include <aplus/ui-widgets.h>
#include <aplus/ui.h>


#define UI_LABEL_TEXT_MAX 128


typedef enum {

    UI_WIDGET_PANEL = 0,
    UI_WIDGET_LABEL,
    UI_WIDGET_BUTTON,

} ui_widget_kind_t;


struct ui_widget {

    ui_view_t* view;
    ui_widget_kind_t kind;

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

    ui_layout_fn layout;
    void* layout_user;

    ui_key_fn key;
    void* key_user;

    struct {

        bool valid;

        int x0;
        int y0;
        int x1;
        int y1;

    } damage;

    bool closed;
};


/**
 * @brief Loads a font face from a file. Implemented in ui_font.c.
 *
 * @param path The font file to load.
 * @return The face, or NULL when the file cannot be loaded, in which case the caller draws no text.
 */
cairo_font_face_t* ui_font_face(const char* path);

/**
 * @brief Implemented in ui_widget.c.
 */
ui_widget_t* ui_widget_new(ui_view_t* view, ui_widget_kind_t kind, bool interactive);
void ui_widget_draw(ui_widget_t* widget, cairo_t* cr);
bool ui_widget_hit(const ui_widget_t* widget, int x, int y);

/**
 * @brief Implemented in ui_draw.c.
 */
void ui_draw_rounded_rect(cairo_t* cr, ui_rect_t rect, double radius);

/**
 * @brief The same path in device units, for the half-pixel insets a centred stroke needs.
 */
void ui_draw_rounded_rect_d(cairo_t* cr, double x, double y, double w, double h, double radius);
void ui_draw_set_color(cairo_t* cr, ui_color_t color);
void ui_draw_text(cairo_t* cr, ui_rect_t rect, const char* text, const char* font, double size, ui_color_t color, ui_align_t align);

/**
 * @brief Pulls a colour back towards the backdrop, which is how a disabled control is drawn.
 *
 * @param color The colour to fade.
 * @param theme The theme holding the backdrop.
 * @return The faded colour.
 */
ui_color_t ui_draw_dim(ui_color_t color, const ui_theme_t* theme);

/**
 * @brief Implemented in ui_panel.c, ui_label.c and ui_button.c.
 */
void ui_panel_draw(ui_widget_t* widget, cairo_t* cr);
void ui_label_draw(ui_widget_t* widget, cairo_t* cr);
void ui_button_draw(ui_widget_t* widget, cairo_t* cr);

/**
 * @brief Pointer state transitions, reported back so that the view only repaints a button that changed.
 */
bool ui_button_set_hovered(ui_widget_t* widget, bool hovered);
bool ui_button_set_pressed(ui_widget_t* widget, bool pressed);

#endif
