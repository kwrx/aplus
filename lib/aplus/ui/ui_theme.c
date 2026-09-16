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

#include <aplus/ui-widgets.h>

#include "ui_widget_internal.h"


/**
 * @brief The scheme a client wakes up with, continuing the frame the window manager draws around it.
 *
 * The chrome is hueless and `primary` is the one saturated colour; hover and active are white washes.
 */
static const ui_theme_t ui_theme_default = {

    .background     = UI_RGB(0x2B, 0x2B, 0x2B),
    .surface        = UI_RGB(0x35, 0x35, 0x35),
    .surface_sunken = UI_RGB(0x16, 0x16, 0x16),

    .primary    = UI_RGB(0x3D, 0x7E, 0xE0),
    .on_primary = UI_RGB(0xFF, 0xFF, 0xFF),

    .secondary    = UI_RGB(0x3C, 0x3C, 0x3C),
    .on_secondary = UI_RGB(0xE8, 0xE8, 0xE8),

    .danger    = UI_RGB(0xD6, 0x45, 0x4D),
    .on_danger = UI_RGB(0xFF, 0xFF, 0xFF),

    .hover  = UI_RGBA(0xFF, 0xFF, 0xFF, 0.09),
    .active = UI_RGBA(0xFF, 0xFF, 0xFF, 0.20),

    .border     = UI_RGBA(0xFF, 0xFF, 0xFF, 0.08),
    .focus_ring = UI_RGBA(0xFF, 0xFF, 0xFF, 0.45),

    .text       = UI_RGB(0xE8, 0xE8, 0xE8),
    .text_muted = UI_RGB(0x7F, 0x7F, 0x7F),

    .disabled_fade = 0.55,

    .corner_radius = 6.0,
    .font_size     = 14.0,

    .font_regular = "/usr/share/fonts/ttf/Ubuntu-R.ttf",
    .font_bold    = "/usr/share/fonts/ttf/Ubuntu-B.ttf",
};


static const ui_theme_t* ui_theme_current = &ui_theme_default;


const ui_theme_t* ui_theme_dark(void) {
    return &ui_theme_default;
}


void ui_theme_set(const ui_theme_t* theme) {
    ui_theme_current = theme ? theme : &ui_theme_default;
}


const ui_theme_t* ui_theme(void) {
    return ui_theme_current;
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
