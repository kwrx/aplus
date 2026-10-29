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

/**
 * @brief The context menu: a title over a column of rows that slides out of the rail beside the item it was opened on,
 * its corners flaring into the rail's edge like the popout's. What the rows do is the business of whoever opened it.
 */

#include <math.h>
#include <string.h>

#include <wm.h>


/**
 * @brief Reports where a row is, once the menu is all the way out.
 *
 * @param index The row's place in the menu.
 * @return The row, under the title and inset from the menu's edges.
 */
static wm_rect_t wm_menu_row_rect(int index) {

    const wm_rect_t panel = wm.menu.rect;

    const wm_rect_t r = {panel.x + WM_TASKBAR_PAD, panel.y + WM_TASKBAR_PAD + WM_TASKBAR_ITEM + WM_MENU_RULE + index * (WM_TASKBAR_ITEM + WM_TASKBAR_GAP), panel.width - 2 * WM_TASKBAR_PAD, WM_TASKBAR_ITEM};

    return r;
}


/**
 * @brief Slides the menu out of the rail with its title level with an item, nudged to stay inside the work area.
 *
 * @param owner The item it is opened on, which it is reported against.
 * @param anchor The top of that item.
 * @param title What it is about, cut to fit.
 * @param rows Its rows, top to bottom.
 * @param count How many there are, at most WM_MENU_ROWS_MAX.
 */
void wm_menu_show(uint32_t owner, int anchor, const char* title, const wm_menu_row_t* rows, int count) {

    const double fit = WM_TASKBAR_POPOUT_MAX - 2 * WM_TASKBAR_POPOUT_PAD;

    count = WM_CLAMP(count, 0, WM_MENU_ROWS_MAX);

    ui_draw_ellipsize(wm.menu.title, sizeof(wm.menu.title), title ? title : "", wm.theme.font, WM_TASKBAR_FONT_SIZE, fit);

    double width = ui_draw_text_width(wm.menu.title, wm.theme.font, WM_TASKBAR_FONT_SIZE) + 2 * WM_TASKBAR_POPOUT_PAD;

    for (int i = 0; i < count; i++) {
        width = WM_MAX(width, 2 * WM_TASKBAR_PAD + WM_TASKBAR_ITEM + ui_draw_text_width(rows[i].label, wm.theme.font, WM_TASKBAR_FONT_SIZE) + WM_TASKBAR_POPOUT_PAD);
    }


    const wm_rect_t work = wm_workarea();
    const int height     = 2 * WM_TASKBAR_PAD + WM_TASKBAR_ITEM + WM_MENU_RULE + count * (WM_TASKBAR_ITEM + WM_TASKBAR_GAP) - WM_TASKBAR_GAP;
    const int lowest     = work.y + work.height - WM_TASKBAR_POPOUT_RADIUS - height;
    const int top        = WM_MAX(WM_MIN(anchor - WM_TASKBAR_PAD, lowest), work.y + WM_TASKBAR_POPOUT_RADIUS);

    const wm_rect_t r = {WM_TASKBAR_WIDTH, top, (int)WM_CLAMP(ceil(width), WM_MENU_MIN_WIDTH, WM_TASKBAR_POPOUT_MAX), height};

    wm.menu.rect  = r;
    wm.menu.owner = owner;
    wm.menu.count = count;

    memcpy(wm.menu.rows, rows, count * sizeof(rows[0]));

    for (int i = 0; i < WM_MENU_ROWS_MAX; i++) {
        ui_anim_set(&wm.menu.hover[i], 0.0);
    }

    ui_anim_set(&wm.menu.open, 0.0);
    ui_anim_to(&wm.menu.open, 1.0, UI_MOTION_NORMAL, ui_anim_now());
}


/**
 * @brief Slides the menu back into the rail.
 */
void wm_menu_hide(void) {

    if (wm_menu_shown()) {
        ui_anim_to(&wm.menu.open, 0.0, UI_MOTION_FAST, ui_anim_now());
    }
}


/**
 * @brief Reports whether the menu is out, or on its way out.
 *
 * @return true from the press that opens it to the one that closes it.
 */
bool wm_menu_shown(void) {
    return ui_anim_target(&wm.menu.open) > 0.0;
}


/**
 * @brief Reports whether a point is on the menu while it is out.
 *
 * @param x The point.
 * @param y The point.
 * @return true inside its outline, leaving out the fillets.
 */
bool wm_menu_contains(int x, int y) {
    return wm_menu_shown() && wm_rect_contains_point(&wm.menu.rect, x, y);
}


/**
 * @brief Reports which row is under a point, once the menu is all the way out.
 *
 * @param x The point.
 * @param y The point.
 * @return The row's place in the menu, or -1 for none.
 */
int wm_menu_row_at(int x, int y) {

    if (ui_anim_target(&wm.menu.open) < 1.0) {
        return -1;
    }

    for (int i = 0; i < wm.menu.count; i++) {

        const wm_rect_t r = wm_menu_row_rect(i);

        if (wm_rect_contains_point(&r, x, y)) {
            return i;
        }
    }

    return -1;
}


/**
 * @brief Reports how far out the menu is for an item, which that item's wash is held lit by.
 *
 * @param owner The item.
 * @param now The clock reading.
 * @return 0 to 1, and 0 when the menu belongs to another item.
 */
double wm_menu_hold(uint32_t owner, uint64_t now) {
    return owner && owner == wm.menu.owner ? ui_anim_value(&wm.menu.open, now) : 0.0;
}


/**
 * @brief Reports where the menu is at a moment, with the fillets that join it to the rail and its shadow.
 *
 * @param now The clock reading.
 * @return Its bounding box, empty while it is all the way in.
 */
wm_rect_t wm_menu_extent(uint64_t now) {

    const double open = ui_anim_value(&wm.menu.open, now);

    if (open <= 0.0) {

        const wm_rect_t none = {0, 0, 0, 0};

        return none;
    }


    const wm_rect_t panel = wm.menu.rect;

    const wm_rect_t r = {panel.x, panel.y - WM_TASKBAR_POPOUT_RADIUS - 2, (int)ceil(panel.width * open) + 2, panel.height + 2 * WM_TASKBAR_POPOUT_RADIUS + 4};

    return wm_taskbar_shadow_extent(r);
}


/**
 * @brief Draws a row's mark: a plus to open, a pushpin to pin, the pushpin struck through to unpin, a red cross to quit.
 *
 * @param cr The cairo context.
 * @param glyph The mark.
 * @param x The centre.
 * @param y The centre.
 * @param alpha How opaque to draw it.
 */
static void wm_menu_paint_glyph(cairo_t* cr, wm_menu_glyph_t glyph, double x, double y, double alpha) {

    const double g = WM_TASKBAR_SESSION_GLYPH;
    const double d = g - 2.0;

    if (glyph == WM_MENU_GLYPH_NONE) {
        return;
    }

    cairo_save(cr);

    cairo_set_line_width(cr, glyph == WM_MENU_GLYPH_PIN || glyph == WM_MENU_GLYPH_UNPIN ? 1.5 : 2.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    wm_source(cr, glyph == WM_MENU_GLYPH_QUIT ? wm.theme.close_over : wm.theme.title_active, alpha);

    cairo_new_path(cr);

    switch (glyph) {

        case WM_MENU_GLYPH_OPEN:

            cairo_move_to(cr, x - d, y);
            cairo_line_to(cr, x + d, y);
            cairo_move_to(cr, x, y - d);
            cairo_line_to(cr, x, y + d);
            break;

        case WM_MENU_GLYPH_QUIT:

            cairo_move_to(cr, x - d, y - d);
            cairo_line_to(cr, x + d, y + d);
            cairo_move_to(cr, x + d, y - d);
            cairo_line_to(cr, x - d, y + d);
            break;

        default:

            cairo_save(cr);
            cairo_translate(cr, x, y);
            cairo_rotate(cr, M_PI / 4.0);

            cairo_move_to(cr, -2.5, -6.5);
            cairo_line_to(cr, 2.5, -6.5);
            cairo_line_to(cr, 2.5, -2.5);
            cairo_line_to(cr, 5.0, 0.0);
            cairo_line_to(cr, -5.0, 0.0);
            cairo_line_to(cr, -2.5, -2.5);
            cairo_close_path(cr);

            cairo_move_to(cr, 0.0, 0.0);
            cairo_line_to(cr, 0.0, 6.5);

            cairo_restore(cr);

            if (glyph == WM_MENU_GLYPH_UNPIN) {
                cairo_move_to(cr, x - g, y - g);
                cairo_line_to(cr, x + g, y + g);
            }

            break;
    }

    cairo_stroke(cr);
    cairo_restore(cr);
}


/**
 * @brief Draws one row: its wash, its mark and what it does.
 *
 * @param cr The cairo context, clipped to the menu.
 * @param index The row's place in the menu.
 * @param shift How far the menu still is from all the way out, which the row is slid back by.
 * @param alpha How opaque to draw the mark and the text.
 * @param held Whether the row is pressed with the pointer still on it.
 */
static void wm_menu_paint_row(cairo_t* cr, int index, int shift, double alpha, bool held) {

    const wm_menu_row_t* row = &wm.menu.rows[index];

    wm_rect_t r = wm_menu_row_rect(index);

    r.x -= shift;

    wm_taskbar_paint_wash(cr, &r, ui_anim_value(&wm.menu.hover[index], wm.frame_time), held, 0.0);
    wm_menu_paint_glyph(cr, row->glyph, r.x + WM_TASKBAR_ITEM / 2.0, r.y + r.height / 2.0, alpha);


    cairo_font_extents_t font;

    cairo_font_extents(cr, &font);

    wm_source(cr, wm.theme.title_active, alpha);
    cairo_move_to(cr, r.x + WM_TASKBAR_ITEM, round(r.y + (r.height - (font.ascent + font.descent)) / 2.0 + font.ascent));
    cairo_show_text(cr, row->label);
}


/**
 * @brief Draws the menu sliding out of the rail: its title, a rule, and its rows.
 *
 * @param cr The cairo context.
 * @param held The row pressed with the pointer still on it, or -1.
 */
void wm_menu_paint(cairo_t* cr, int held) {

    wm.menu.painted = wm_menu_extent(wm.frame_time);

    const double open     = ui_anim_value(&wm.menu.open, wm.frame_time);
    const wm_rect_t panel = wm.menu.rect;

    const double x0 = panel.x;
    const double x1 = x0 + panel.width * open;

    cairo_font_face_t* face = ui_font_face(wm.theme.font);

    if (x1 - x0 < 1.0 || !face) {
        return;
    }


    const double top    = panel.y;
    const double bottom = panel.y + panel.height;

    const double fillet = WM_MIN(WM_TASKBAR_POPOUT_RADIUS, x1 - x0);
    const double corner = WM_MIN(WM_TASKBAR_POPOUT_RADIUS, (x1 - x0) / 2.0);

    const wm_rect_t shape = {(int)x0, (int)top, (int)ceil(x1 - x0), (int)(bottom - top)};

    wm_taskbar_shadow(cr, &shape, open);

    cairo_new_path(cr);
    wm_taskbar_popout_outline(cr, x0, x1, top, bottom, fillet, corner, 0.5);
    wm_taskbar_edge(cr);

    cairo_save(cr);

    cairo_new_path(cr);
    wm_taskbar_popout_outline(cr, x0, x1, top, bottom, fillet, corner, 0.0);
    cairo_close_path(cr);

    wm_source(cr, wm.theme.taskbar, 1.0);
    cairo_fill_preserve(cr);
    cairo_clip(cr);


    const int shift = (int)round(panel.width - (x1 - x0));

    cairo_font_extents_t font;

    cairo_set_font_face(cr, face);
    cairo_set_font_size(cr, WM_TASKBAR_FONT_SIZE);
    cairo_font_extents(cr, &font);

    wm_source(cr, wm.theme.title_idle, open);
    cairo_move_to(cr, panel.x - shift + WM_TASKBAR_POPOUT_PAD, round(top + WM_TASKBAR_PAD + (WM_TASKBAR_ITEM - (font.ascent + font.descent)) / 2.0 + font.ascent));
    cairo_show_text(cr, wm.menu.title);


    const double rule = top + WM_TASKBAR_PAD + WM_TASKBAR_ITEM + WM_MENU_RULE / 2 + 0.5;

    cairo_new_path(cr);
    cairo_move_to(cr, panel.x - shift + WM_TASKBAR_PAD, rule);
    cairo_line_to(cr, panel.x - shift + panel.width - WM_TASKBAR_PAD, rule);
    cairo_set_line_width(cr, 1.0);
    wm_source(cr, wm.theme.taskbar_edge, open);
    cairo_stroke(cr);

    for (int i = 0; i < wm.menu.count; i++) {
        wm_menu_paint_row(cr, i, shift, open, i == held);
    }

    cairo_restore(cr);
}
