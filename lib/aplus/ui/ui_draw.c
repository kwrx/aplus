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

    /* A radius past half the shorter side would make the two arcs on that side overlap and
       cairo would draw the join inside out. Clamping is what lets a caller say "very
       round" without knowing how small the widget has ended up after a layout pass. */
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


/* One line, centred vertically in the rect and aligned horizontally as asked, clipped to
 * the rect so that a string too long for the space it was given spills nowhere.
 *
 * Vertical centring uses the font's own extents rather than the string's: centring on the
 * ink of the glyphs actually present would make a row of buttons reading "7 8 9 +" sit at
 * four different heights, because none of those glyphs has the same ink box.
 */
void ui_draw_text(cairo_t* cr, ui_rect_t rect, const char* text, const char* font, double size, ui_color_t color, ui_align_t align) {

    if (!text || !*text || rect.width <= 0 || rect.height <= 0) {
        return;
    }


    cairo_font_face_t* face = ui_font_face(font);

    if (!face) {
        return;
    }


    cairo_save(cr);

    cairo_set_font_face(cr, face);
    cairo_set_font_size(cr, size);

    cairo_rectangle(cr, rect.x, rect.y, rect.width, rect.height);
    cairo_clip(cr);


    cairo_font_extents_t fe;
    cairo_text_extents_t te;

    cairo_font_extents(cr, &fe);
    cairo_text_extents(cr, text, &te);


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


    ui_draw_set_color(cr, color);

    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);

    cairo_restore(cr);
}
