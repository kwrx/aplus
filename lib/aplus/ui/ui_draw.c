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


void ui_draw_set_color(cairo_t* cr, ui_color_t color) {
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
}


ui_color_t ui_draw_dim(ui_color_t color, const ui_theme_t* theme) {

    ui_color_t towards = theme->background;

    towards.a = theme->disabled_fade;

    return ui_color_blend(color, towards);
}


void ui_draw_rounded_rect(cairo_t* cr, ui_rect_t rect, double radius) {
    ui_draw_rounded_rect_d(cr, rect.x, rect.y, rect.width, rect.height, radius);
}


void ui_draw_rounded_rect_d(cairo_t* cr, double x, double y, double w, double h, double radius) {

    const double limit = (w < h ? w : h) / 2.0;

    if (radius > limit) {
        radius = limit;
    }

    if (radius <= 0.0) {
        cairo_rectangle(cr, x, y, w, h);
        return;
    }


    cairo_new_sub_path(cr);

    cairo_arc(cr, x + w - radius, y + radius, radius, -M_PI / 2.0, 0.0);
    cairo_arc(cr, x + w - radius, y + h - radius, radius, 0.0, M_PI / 2.0);
    cairo_arc(cr, x + radius, y + h - radius, radius, M_PI / 2.0, M_PI);
    cairo_arc(cr, x + radius, y + radius, radius, M_PI, 3.0 * M_PI / 2.0);

    cairo_close_path(cr);
}


/**
 * @brief Draws one line of text, centred vertically in a rect and clipped to it.
 *
 * @param cr The cairo context to draw with.
 * @param rect The rectangle to draw in.
 * @param text The string to draw.
 * @param font The font file to draw it with.
 * @param size The font size in pixels.
 * @param color The colour of the text.
 * @param align How to align the text horizontally.
 */
void ui_draw_text(cairo_t* cr, ui_rect_t rect, const char* text, const char* font, double size, ui_color_t color, ui_align_t align) {

    if (!text || !*text || rect.width <= 0 || rect.height <= 0) {
        return;
    }


    cairo_font_face_t* face = ui_font_face(font);

    if (!face) {
        return;
    }


    cairo_scaled_font_t* scaled = ui_font_scaled(face, size);

    if (!scaled) {
        return;
    }


    cairo_glyph_t* glyphs = NULL;
    int count             = 0;

    if (cairo_scaled_font_text_to_glyphs(scaled, 0.0, 0.0, text, -1, &glyphs, &count, NULL, NULL, NULL) != CAIRO_STATUS_SUCCESS) {
        return;
    }

    if (!glyphs || count <= 0) {
        cairo_glyph_free(glyphs);
        return;
    }


    cairo_save(cr);

    cairo_set_scaled_font(cr, scaled);

    cairo_rectangle(cr, rect.x, rect.y, rect.width, rect.height);
    cairo_clip(cr);


    cairo_font_extents_t fe;
    cairo_text_extents_t te;

    cairo_scaled_font_extents(scaled, &fe);
    cairo_scaled_font_glyph_extents(scaled, glyphs, count, &te);


    double x = rect.x;

    switch (align) {

        case UI_ALIGN_CENTER:
            x = rect.x + (rect.width - te.width) / 2.0 - te.x_bearing;
            break;

        case UI_ALIGN_RIGHT:
            x = rect.x + rect.width - te.width - te.x_bearing;
            break;

        case UI_ALIGN_LEFT:
        default:
            x = rect.x - te.x_bearing;
            break;
    }

    const double y = rect.y + (rect.height - (fe.ascent + fe.descent)) / 2.0 + fe.ascent;


    for (int i = 0; i < count; i++) {

        glyphs[i].x += x;
        glyphs[i].y += y;
    }


    ui_draw_set_color(cr, color);

    cairo_show_glyphs(cr, glyphs, count);

    cairo_restore(cr);

    cairo_glyph_free(glyphs);
}
