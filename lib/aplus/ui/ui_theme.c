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

#include <stdio.h>
#include <string.h>

#include <aplus/ui-widgets.h>

#include "ui_widget_internal.h"


/**
 * @brief The scheme a client wakes up with, continuing the frame the window manager draws around it.
 *
 * The chrome is hueless and `primary` is the one saturated colour, which the focus ring shares.
 */
static const ui_theme_t ui_theme_default = {

    .background     = UI_RGB(0x2B, 0x2B, 0x2B),
    .surface        = UI_RGB(0x34, 0x34, 0x34),
    .surface_sunken = UI_RGB(0x1C, 0x1C, 0x1C),

    .primary    = UI_RGB(0x1B, 0x6A, 0xCA),
    .on_primary = UI_RGB(0xFF, 0xFF, 0xFF),

    .secondary    = UI_RGB(0x3A, 0x3A, 0x3A),
    .on_secondary = UI_RGB(0xEE, 0xEE, 0xEE),

    .danger    = UI_RGB(0xC9, 0x36, 0x3C),
    .on_danger = UI_RGB(0xFF, 0xFF, 0xFF),

    .hover  = UI_RGBA(0xFF, 0xFF, 0xFF, 0.07),
    .active = UI_RGBA(0xFF, 0xFF, 0xFF, 0.14),

    .border     = UI_RGBA(0xFF, 0xFF, 0xFF, 0.07),
    .focus_ring = UI_RGB(0x35, 0x84, 0xE4),

    .text       = UI_RGB(0xEE, 0xEE, 0xEE),
    .text_muted = UI_RGB(0x9A, 0x9A, 0x9A),

    .disabled_fade = 0.55,

    .corner_radius = 8.0,
    .font_size     = 14.0,

    .font_regular = "/usr/share/fonts/ttf/NotoSans-Regular.ttf",
    .font_medium  = "/usr/share/fonts/ttf/NotoSans-Medium.ttf",
    .font_bold    = "/usr/share/fonts/ttf/NotoSans-Bold.ttf",
};


/**
 * @brief The theme the server sent, starting out as the dark default, and the name and font paths it carried.
 */
static ui_theme_t ui_theme_live;
static bool ui_theme_live_ready = false;

static char ui_theme_live_name[UI_THEME_NAME_MAX]       = "default";
static char ui_theme_live_variant[UI_THEME_VARIANT_MAX] = UI_THEME_VARIANT_DARK;
static char ui_theme_live_fonts[5][UI_THEME_PATH_MAX];


static const ui_theme_t* ui_theme_current = &ui_theme_live;


/**
 * @brief Reports the system theme, filling it in with the dark default the first time.
 *
 * @return The theme.
 */
static ui_theme_t* ui_theme_live_get(void) {

    if (!ui_theme_live_ready) {

        ui_theme_live       = ui_theme_default;
        ui_theme_live_ready = true;
    }

    return &ui_theme_live;
}


const ui_theme_t* ui_theme_dark(void) {
    return &ui_theme_default;
}


const ui_theme_t* ui_theme_system(void) {
    return ui_theme_live_get();
}


const char* ui_theme_name(void) {
    return ui_theme_live_name;
}


const char* ui_theme_variant(void) {
    return ui_theme_live_variant;
}


bool ui_theme_is_light(void) {
    return strcmp(ui_theme_live_variant, UI_THEME_VARIANT_LIGHT) == 0;
}


void ui_theme_set(const ui_theme_t* theme) {
    ui_theme_current = theme ? theme : ui_theme_live_get();
}


const ui_theme_t* ui_theme(void) {

    ui_theme_live_get();

    return ui_theme_current;
}


/**
 * @brief Holds a colour component between 0 and 1.
 *
 * @param v The component.
 * @return The component, clamped.
 */
static double ui_theme_unit(double v) {

    if (v < 0.0) {
        return 0.0;
    }

    if (v > 1.0) {
        return 1.0;
    }

    return v;
}


/**
 * @brief Converts a colour off the wire into the one widgets draw with, clamping every component to 0..1.
 *
 * @param color The colour as sent.
 * @return The colour.
 */
static ui_color_t ui_theme_color(ui_msg_color_t color) {

    ui_color_t c = {ui_theme_unit(color.r), ui_theme_unit(color.g), ui_theme_unit(color.b), ui_theme_unit(color.a)};

    return c;
}


/**
 * @brief Copies a font path off the wire into the system theme's own storage.
 *
 * @param slot Which of the theme's five font buffers to fill.
 * @param path The path as sent, not necessarily terminated.
 * @param fallback What to keep when the path is empty.
 * @return The path to draw with.
 */
static const char* ui_theme_font(size_t slot, const char* path, const char* fallback) {

    char* out = ui_theme_live_fonts[slot];

    snprintf(out, UI_THEME_PATH_MAX, "%.*s", UI_THEME_PATH_MAX - 1, path);

    return out[0] ? out : fallback;
}


void ui_theme_adopt(const ui_msg_theme_t* msg) {

    ui_theme_t* theme = ui_theme_live_get();

    theme->background     = ui_theme_color(msg->background);
    theme->surface        = ui_theme_color(msg->surface);
    theme->surface_sunken = ui_theme_color(msg->surface_sunken);
    theme->primary        = ui_theme_color(msg->primary);
    theme->on_primary     = ui_theme_color(msg->on_primary);
    theme->secondary      = ui_theme_color(msg->secondary);
    theme->on_secondary   = ui_theme_color(msg->on_secondary);
    theme->danger         = ui_theme_color(msg->danger);
    theme->on_danger      = ui_theme_color(msg->on_danger);
    theme->hover          = ui_theme_color(msg->hover);
    theme->active         = ui_theme_color(msg->active);
    theme->border         = ui_theme_color(msg->border);
    theme->focus_ring     = ui_theme_color(msg->focus_ring);
    theme->text           = ui_theme_color(msg->text);
    theme->text_muted     = ui_theme_color(msg->text_muted);

    theme->disabled_fade = msg->disabled_fade >= 0.0 && msg->disabled_fade <= 1.0 ? msg->disabled_fade : ui_theme_default.disabled_fade;
    theme->corner_radius = msg->corner_radius >= 0.0 ? msg->corner_radius : ui_theme_default.corner_radius;
    theme->font_size     = msg->font_size > 0.0 ? msg->font_size : ui_theme_default.font_size;

    theme->font_regular = ui_theme_font(0, msg->font_regular, ui_theme_default.font_regular);
    theme->font_medium  = ui_theme_font(1, msg->font_medium, theme->font_regular);
    theme->font_bold    = ui_theme_font(2, msg->font_bold, theme->font_regular);

    theme->font_mono      = ui_theme_font(3, msg->font_mono, NULL);
    theme->font_mono_bold = ui_theme_font(4, msg->font_mono_bold, theme->font_mono);
    theme->font_mono_size = msg->font_mono_size > 0.0 ? msg->font_mono_size : 0.0;

    snprintf(ui_theme_live_name, sizeof(ui_theme_live_name), "%.*s", UI_THEME_NAME_MAX - 1, msg->name);

    const bool light = strncmp(msg->variant, UI_THEME_VARIANT_LIGHT, sizeof(msg->variant)) == 0;

    snprintf(ui_theme_live_variant, sizeof(ui_theme_live_variant), "%s", light ? UI_THEME_VARIANT_LIGHT : UI_THEME_VARIANT_DARK);
}


int ui_theme_update(ui_connection_t* conn) {

    if (!conn || !conn->theme.valid) {
        return 0;
    }

    conn->theme.valid = false;

    ui_theme_adopt(&conn->theme.msg);

    return 1;
}


ui_color_t ui_rgb(uint8_t r, uint8_t g, uint8_t b) {

    ui_color_t c = {r / 255.0, g / 255.0, b / 255.0, 1.0};

    return c;
}


ui_color_t ui_rgba(uint8_t r, uint8_t g, uint8_t b, double a) {

    ui_color_t c = {r / 255.0, g / 255.0, b / 255.0, a};

    return c;
}


ui_color_t ui_color_blend(ui_color_t under, ui_color_t over) {

    const double a = over.a;

    ui_color_t c = {

        .r = under.r * (1.0 - a) + over.r * a,
        .g = under.g * (1.0 - a) + over.g * a,
        .b = under.b * (1.0 - a) + over.b * a,

        .a = under.a,
    };

    return c;
}
