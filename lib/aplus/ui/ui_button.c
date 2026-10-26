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
#include <string.h>

#include "ui_widget_internal.h"


static void ui_button_draw(ui_widget_t* widget, cairo_t* cr);
static bool ui_button_on_hover(ui_widget_t* widget, bool hovered);
static bool ui_button_on_press(ui_widget_t* widget, int x, int y);
static bool ui_button_on_drag(ui_widget_t* widget, int x, int y, bool inside);
static bool ui_button_on_release(ui_widget_t* widget, int x, int y, bool inside, int clicks);
static int ui_button_tick(ui_widget_t* widget, uint64_t now);
static void ui_button_restyle(ui_widget_t* widget);


/**
 * @brief How far a held button shrinks towards its centre, as a fraction of its size.
 */
#define UI_BUTTON_PRESS_SCALE 0.04

/**
 * @brief How quickly a press lands; the release eases back at the normal pace.
 */
#define UI_BUTTON_PRESS_MS 60

/**
 * @brief How far a button's text sits from its icon.
 */
#define UI_BUTTON_ICON_GAP 6


static const ui_widget_ops_t ui_button_ops = {

    .draw       = ui_button_draw,
    .on_hover   = ui_button_on_hover,
    .on_press   = ui_button_on_press,
    .on_drag    = ui_button_on_drag,
    .on_release = ui_button_on_release,
    .tick       = ui_button_tick,
    .restyle    = ui_button_restyle,
};


ui_widget_t* ui_button_create(ui_view_t* view, const char* text, ui_action_fn on_click, void* user) {

    ui_widget_t* widget = ui_widget_new(view, UI_WIDGET_BUTTON, &ui_button_ops, true);

    if (!widget) {
        return NULL;
    }

    widget->button.style     = UI_BUTTON_STYLE_DEFAULT;
    widget->button.weight    = UI_FONT_MEDIUM;
    widget->button.size      = widget->theme->font_size;
    widget->button.icon_size = UI_BUTTON_ICON_SIZE;
    widget->button.on_click  = on_click;

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

    widget->button.weight   = weight;
    widget->button.size     = size > 0.0 ? size : widget->theme->font_size;
    widget->button.size_set = size > 0.0;

    ui_widget_invalidate(widget);
}


/**
 * @brief Follows the theme's font size, unless the app named one.
 *
 * @param widget The button.
 */
static void ui_button_restyle(ui_widget_t* widget) {

    if (!widget->button.size_set) {
        widget->button.size = widget->theme->font_size;
    }
}


void ui_button_set_icon(ui_widget_t* widget, const char* icon) {

    if (!widget || widget->kind != UI_WIDGET_BUTTON) {
        return;
    }


    char next[UI_ICON_NAME_MAX] = {0};

    if (icon && strlen(icon) < sizeof(next)) {
        strcpy(next, icon);
    }

    if (strcmp(next, widget->button.icon) == 0) {
        return;
    }

    memcpy(widget->button.icon, next, sizeof(next));

    ui_widget_invalidate(widget);
}


void ui_button_set_icon_size(ui_widget_t* widget, int size) {

    if (!widget || widget->kind != UI_WIDGET_BUTTON) {
        return;
    }

    widget->button.icon_size = size > 0 ? size : UI_BUTTON_ICON_SIZE;

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


/**
 * @brief Points the press transition at whatever the pointer and the key binding say together.
 *
 * @param widget The button.
 */
static void ui_button_update_press(ui_widget_t* widget) {

    const bool down = widget->button.pressed || widget->button.held;

    ui_anim_to(&widget->button.press_anim, down ? 1.0 : 0.0, down ? UI_BUTTON_PRESS_MS : UI_MOTION_NORMAL, ui_anim_now());
}


void ui_button_set_held(ui_widget_t* widget, bool held) {

    if (!widget || widget->kind != UI_WIDGET_BUTTON || widget->button.held == held) {
        return;
    }

    widget->button.held = held;

    ui_button_update_press(widget);
    ui_widget_invalidate(widget);
}


bool ui_button_held(const ui_widget_t* widget) {

    if (!widget || widget->kind != UI_WIDGET_BUTTON) {
        return false;
    }

    return widget->button.held;
}


/**
 * @brief Lights the button while the pointer is over it.
 *
 * @param widget The button.
 * @param hovered Whether the pointer is over it.
 * @return Whether the button has to be repainted.
 */

static bool ui_button_on_hover(ui_widget_t* widget, bool hovered) {

    if (widget->button.hovered == hovered) {
        return false;
    }

    widget->button.hovered = hovered;

    ui_anim_to(&widget->button.hover_anim, hovered ? 1.0 : 0.0, UI_MOTION_FAST, ui_anim_now());

    return true;
}


static bool ui_button_set_pressed(ui_widget_t* widget, bool pressed) {

    if (widget->button.pressed == pressed) {
        return false;
    }

    widget->button.pressed = pressed;

    ui_button_update_press(widget);

    return true;
}


/**
 * @brief Keeps painting while the hover or the press is still easing in or out.
 *
 * @param widget The button.
 * @param now The loop's clock reading.
 * @return The delay until the next frame, or -1 when the button is still.
 */
static int ui_button_tick(ui_widget_t* widget, uint64_t now) {

    (void)now;

    if (!ui_widget_animating(widget, &widget->button.hover_anim) && !ui_widget_animating(widget, &widget->button.press_anim)) {
        return -1;
    }

    ui_widget_invalidate(widget);

    return UI_ANIM_FRAME_MS;
}


static bool ui_button_on_press(ui_widget_t* widget, int x, int y) {

    (void)x;
    (void)y;

    return ui_button_set_pressed(widget, true);
}


static bool ui_button_on_drag(ui_widget_t* widget, int x, int y, bool inside) {

    (void)x;
    (void)y;

    return ui_button_set_pressed(widget, inside);
}


static bool ui_button_on_release(ui_widget_t* widget, int x, int y, bool inside, int clicks) {

    (void)x;
    (void)y;

    const bool changed = ui_button_set_pressed(widget, false);

    if (inside && clicks > 0) {
        ui_button_activate(widget);
    }

    return changed;
}


/**
 * @brief Resolves a button's style to a fill and the colour of the text that goes on it.
 *
 * @param widget The button to resolve.
 * @param fill Receives the fill colour.
 * @param text Receives the label colour.
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


/**
 * @brief Draws a button's icon and its text as one group centred in it, the icon faded like the fill when disabled.
 *
 * @param widget The button.
 * @param cr The cairo context to draw with.
 * @param color The colour of the text.
 */
static void ui_button_draw_icon(ui_widget_t* widget, cairo_t* cr, ui_color_t color) {

    const char* font = ui_draw_font(widget->theme, widget->button.weight);
    const int size   = widget->button.icon_size;

    const int label = widget->button.text[0] ? (int)ceil(ui_draw_text_width(widget->button.text, font, widget->button.size)) : 0;
    const int width = label > 0 ? size + UI_BUTTON_ICON_GAP + label : size;

    const int x = width < widget->rect.width ? widget->rect.x + (widget->rect.width - width) / 2 : widget->rect.x;

    const ui_rect_t box = {x, widget->rect.y + (widget->rect.height - size) / 2, size, size};

    cairo_surface_t* icon = ui_icon_load(widget->button.icon, size);

    if (widget->enabled) {

        ui_draw_icon(cr, box, icon);

    } else {

        cairo_push_group(cr);
        ui_draw_icon(cr, box, icon);
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, 1.0 - widget->theme->disabled_fade);
    }

    if (label > 0) {

        const int left = box.x + size + UI_BUTTON_ICON_GAP;

        const ui_rect_t rect = {left, widget->rect.y, widget->rect.x + widget->rect.width - left, widget->rect.height};

        ui_draw_text(cr, rect, widget->button.text, font, widget->button.size, color, UI_ALIGN_LEFT);
    }
}


static void ui_button_draw(ui_widget_t* widget, cairo_t* cr) {

    const ui_theme_t* theme = widget->theme;

    ui_color_t fill;
    ui_color_t text;

    ui_button_colors(widget, &fill, &text);


    double press = 0.0;

    if (widget->enabled) {

        press = ui_widget_anim(widget, &widget->button.press_anim);

        fill = ui_color_blend(fill, ui_color_alpha(theme->hover, ui_widget_anim(widget, &widget->button.hover_anim)));
        fill = ui_color_blend(fill, ui_color_alpha(theme->active, press));

    } else {

        fill = ui_draw_dim(fill, theme);
        text = ui_draw_dim(text, theme);
    }


    if (press > 0.0) {

        const double cx    = widget->rect.x + widget->rect.width / 2.0;
        const double cy    = widget->rect.y + widget->rect.height / 2.0;
        const double scale = 1.0 - UI_BUTTON_PRESS_SCALE * press;

        cairo_translate(cr, cx, cy);
        cairo_scale(cr, scale, scale);
        cairo_translate(cr, -cx, -cy);
    }


    ui_draw_rounded_rect(cr, widget->rect, theme->corner_radius);
    ui_draw_set_color(cr, fill);
    cairo_fill(cr);


    if (widget->button.icon[0]) {
        ui_button_draw_icon(widget, cr, text);
    } else {
        ui_draw_text(cr, widget->rect, widget->button.text, ui_draw_font(theme, widget->button.weight), widget->button.size, text, UI_ALIGN_CENTER);
    }
}
