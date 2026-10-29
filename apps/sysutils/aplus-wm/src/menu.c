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
 * @brief The context menu: a column of rows that slides out of the rail beside a taskbar item, its corners flaring into
 * the rail's edge like the popout's, or drops out of the point a client asked for. Only one is ever out; the taskbar acts
 * on what its own rows do, and a client is told which of its rows was picked.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <wm.h>


/**
 * @brief The colours and font a menu is drawn in.
 */
typedef struct {

    ui_color_t fill;
    ui_color_t rule;
    ui_color_t text;
    ui_color_t muted;
    ui_color_t dim;
    ui_color_t danger;
    ui_color_t hover;
    ui_color_t down;

    double radius;

    const char* font;
    double size;

} wm_menu_palette_t;


/**
 * @brief Picks the palette of the menu out now: the taskbar's for the rail's, and for a client's the one its windows and widgets use.
 *
 * @return The palette.
 */
static wm_menu_palette_t wm_menu_palette(void) {

    if (wm.menu.anchor == WM_MENU_RAIL) {

        ui_color_t dim = wm.theme.title_active;

        dim.a *= 0.4;

        return (wm_menu_palette_t){
            .fill   = wm.theme.taskbar,
            .rule   = wm.theme.taskbar_edge,
            .text   = wm.theme.title_active,
            .muted  = wm.theme.title_idle,
            .dim    = dim,
            .danger = wm.theme.close_over,
            .hover  = wm.theme.taskbar_hover,
            .down   = wm.theme.taskbar_down,
            .radius = WM_TASKBAR_ITEM_RADIUS,
            .font   = wm.theme.font,
            .size   = WM_TASKBAR_FONT_SIZE,
        };
    }

    const ui_msg_theme_t* client = &wm.theme.client;

    ui_color_t fill = wm_theme_role(client->surface);
    ui_color_t fade = wm_theme_role(client->background);

    fill.a = WM_BLUR_TINT;
    fade.a = client->disabled_fade;

    return (wm_menu_palette_t){
        .fill   = fill,
        .rule   = wm_theme_role(client->border),
        .text   = wm_theme_role(client->text),
        .muted  = wm_theme_role(client->text_muted),
        .dim    = ui_color_blend(wm_theme_role(client->text), fade),
        .danger = wm_theme_role(client->danger),
        .hover  = wm_theme_role(client->hover),
        .down   = wm_theme_role(client->active),
        .radius = WM_MENU_ROW_RADIUS,
        .font   = client->font_regular,
        .size   = WM_MAX(client->font_size - WM_MENU_FONT_STEP, 1.0),
    };
}


/**
 * @brief Reports how much of the column a row takes, the gap after it included.
 *
 * @param index The row's place in the menu.
 * @return Its pitch in pixels.
 */
static int wm_menu_pitch(int index) {
    return wm.menu.rows[index].flags & UI_MENU_SEPARATOR ? WM_MENU_RULE : WM_TASKBAR_ITEM + WM_TASKBAR_GAP;
}


/**
 * @brief Reports where a row is once the menu is all the way out.
 *
 * @param index The row's place in the menu.
 * @return The row, under the title when there is one and inset from the menu's edges.
 */
static wm_rect_t wm_menu_row_rect(int index) {

    const wm_rect_t panel = wm.menu.rect;

    int y = panel.y + WM_TASKBAR_PAD + (wm.menu.title[0] ? WM_TASKBAR_ITEM + WM_MENU_RULE : 0);

    for (int i = 0; i < index; i++) {
        y += wm_menu_pitch(i);
    }

    const wm_rect_t r = {panel.x + WM_TASKBAR_PAD, y, panel.width - 2 * WM_TASKBAR_PAD, wm.menu.rows[index].flags & UI_MENU_SEPARATOR ? WM_MENU_RULE - WM_TASKBAR_GAP : WM_TASKBAR_ITEM};

    return r;
}


/**
 * @brief Tells the client that asked for the menu what came of it, once.
 *
 * @param id The id of the row picked, or 0 when it went away without one.
 */
static void wm_menu_answer(uint32_t id) {

    if (!wm.menu.pending) {
        return;
    }

    wm.menu.pending = false;

    wm_window_t* win = wm_window_from_id(wm.menu.window_id);

    if (!win) {
        return;
    }

    const ui_msg_menu_t msg = {.window_id = win->id, .serial = wm.menu.serial, .id = id};

    wm_client_queue(win->client, UI_EV_MENU, &msg, sizeof(msg));
}


/**
 * @brief Lets go of the icons the rows were drawn with.
 */
static void wm_menu_release(void) {

    for (int i = 0; i < wm.menu.count; i++) {

        if (wm.menu.rows[i].icon) {
            cairo_surface_destroy(wm.menu.rows[i].icon);
            wm.menu.rows[i].icon = NULL;
        }
    }

    wm.menu.count = 0;
}


/**
 * @brief Takes up a title and rows, answering whoever the menu was out for before.
 *
 * @param anchor Where the menu comes out of, which picks its palette.
 * @param title What the menu is about, cut to fit, or NULL for none.
 * @param rows Its rows, top to bottom.
 * @param count How many there are.
 */
static void wm_menu_set(wm_menu_anchor_t anchor, const char* title, const wm_menu_row_t* rows, int count) {

    wm_menu_answer(0);
    wm_menu_release();

    wm.menu.anchor = anchor;

    const wm_menu_palette_t palette = wm_menu_palette();

    ui_draw_ellipsize(wm.menu.title, sizeof(wm.menu.title), title ? title : "", palette.font, palette.size, WM_TASKBAR_POPOUT_MAX - 2 * WM_TASKBAR_POPOUT_PAD);

    bool marked = false;

    for (int i = 0; i < WM_CLAMP(count, 0, WM_MENU_ROWS_MAX); i++) {

        const bool rule = rows[i].flags & UI_MENU_SEPARATOR;

        wm.menu.rows[i].action = rows[i].action;
        wm.menu.rows[i].flags  = rows[i].flags;
        wm.menu.rows[i].glyph  = rule ? WM_MENU_GLYPH_NONE : rows[i].glyph;
        wm.menu.rows[i].icon   = rule ? NULL : wm_icon_find(rows[i].icon, WM_MENU_ICON);

        snprintf(wm.menu.rows[i].label, sizeof(wm.menu.rows[i].label), "%s", rows[i].label && !rule ? rows[i].label : "");
        snprintf(wm.menu.rows[i].confirm, sizeof(wm.menu.rows[i].confirm), "%s", rows[i].confirm && !rule ? rows[i].confirm : "");

        if (!wm.menu.rows[i].confirm[0] && (rows[i].flags & UI_MENU_CONFIRM)) {
            snprintf(wm.menu.rows[i].confirm, sizeof(wm.menu.rows[i].confirm), "%.*s?", (int)sizeof(wm.menu.rows[i].confirm) - 2, rows[i].label ? rows[i].label : "");
        }

        wm_utf8_sanitize(wm.menu.rows[i].label);
        wm_utf8_sanitize(wm.menu.rows[i].confirm);

        wm.menu.rows[i].armed = false;

        marked |= wm.menu.rows[i].glyph != WM_MENU_GLYPH_NONE || wm.menu.rows[i].icon;

        wm.menu.count = i + 1;
    }

    wm.menu.indent = marked ? WM_TASKBAR_ITEM : WM_TASKBAR_POPOUT_PAD - WM_TASKBAR_PAD;

    for (int i = 0; i < WM_MENU_ROWS_MAX; i++) {
        ui_anim_set(&wm.menu.hover[i], 0.0);
    }
}


/**
 * @brief Measures the menu around its title and its rows.
 *
 * @param width Receives how wide it is, between WM_MENU_MIN_WIDTH and WM_TASKBAR_POPOUT_MAX.
 * @param height Receives how tall it is.
 */
static void wm_menu_measure(int* width, int* height) {

    const wm_menu_palette_t palette = wm_menu_palette();

    double w = wm.menu.title[0] ? ui_draw_text_width(wm.menu.title, palette.font, palette.size) + 2 * WM_TASKBAR_POPOUT_PAD : 0.0;
    int h    = 2 * WM_TASKBAR_PAD + (wm.menu.title[0] ? WM_TASKBAR_ITEM + WM_MENU_RULE : 0);

    for (int i = 0; i < wm.menu.count; i++) {

        h += wm_menu_pitch(i);

        if (!(wm.menu.rows[i].flags & UI_MENU_SEPARATOR)) {
            w = WM_MAX(w, 2 * WM_TASKBAR_PAD + wm.menu.indent + ui_draw_text_width(wm.menu.rows[i].label, palette.font, palette.size) + WM_TASKBAR_POPOUT_PAD);
            w = WM_MAX(w, 2 * WM_TASKBAR_PAD + wm.menu.indent + ui_draw_text_width(wm.menu.rows[i].confirm, palette.font, palette.size) + WM_TASKBAR_POPOUT_PAD);
        }
    }

    if (wm.menu.count > 0 && !(wm.menu.rows[wm.menu.count - 1].flags & UI_MENU_SEPARATOR)) {
        h -= WM_TASKBAR_GAP;
    }

    *width  = (int)WM_CLAMP(ceil(w), WM_MENU_MIN_WIDTH, WM_TASKBAR_POPOUT_MAX);
    *height = h;
}


/**
 * @brief Starts the menu on its way out from nothing.
 *
 * @param motion How long it takes.
 */
static void wm_menu_start(uint32_t motion) {

    ui_anim_set(&wm.menu.open, 0.0);
    ui_anim_to(&wm.menu.open, 1.0, motion, ui_anim_now());
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

    wm_menu_set(WM_MENU_RAIL, title, rows, count);

    int width;
    int height;

    wm_menu_measure(&width, &height);

    const wm_rect_t work = wm_workarea();
    const int lowest     = work.y + work.height - WM_TASKBAR_POPOUT_RADIUS - height;
    const int top        = WM_MAX(WM_MIN(anchor - WM_TASKBAR_PAD, lowest), work.y + WM_TASKBAR_POPOUT_RADIUS);

    const wm_rect_t r = {WM_TASKBAR_WIDTH, top, width, height};

    wm.menu.upwards   = false;
    wm.menu.leftwards = false;
    wm.menu.owner     = owner;
    wm.menu.window_id = 0;
    wm.menu.rect      = r;

    wm_menu_start(UI_MOTION_NORMAL);
}


/**
 * @brief Drops a client's menu out of a point in its window, below and to the right of it unless that runs off the work area.
 *
 * @param win The window it belongs to.
 * @param serial What the client will know the answer by.
 * @param x The point, in the content area.
 * @param y The point, in the content area.
 * @param rows Its rows, top to bottom.
 * @param count How many there are, at most WM_MENU_ROWS_MAX.
 */
void wm_menu_popup(wm_window_t* win, uint32_t serial, int x, int y, const wm_menu_row_t* rows, int count) {

    wm_menu_set(WM_MENU_POINTER, NULL, rows, count);

    int width;
    int height;

    wm_menu_measure(&width, &height);

    const wm_rect_t work = wm_workarea();

    const int px         = win->x + x;
    const int py         = win->y + y;
    const bool upwards   = py + 1 + height > work.y + work.height;
    const bool leftwards = px + 1 + width > work.x + work.width;
    const int left       = leftwards ? px - width : px + 1;
    const int top        = upwards ? py - height : py + 1;

    const wm_rect_t r = {WM_CLAMP(left, work.x, work.x + work.width - width), WM_MAX(WM_MIN(top, work.y + work.height - height), work.y), width, height};

    wm.menu.upwards   = upwards;
    wm.menu.leftwards = leftwards;
    wm.menu.owner     = 0;
    wm.menu.window_id = win->id;
    wm.menu.serial    = serial;
    wm.menu.pending   = true;
    wm.menu.rect      = r;

    wm_menu_start(UI_MOTION_FAST);
}


/**
 * @brief Turns a row that asks first into its confirm text, which the menu was already measured wide enough for.
 *
 * @param row The row's place in the menu.
 * @return true when the row asked, which leaves the menu out for a second press; false when it can be picked now.
 */
bool wm_menu_arm(int row) {

    if (row < 0 || row >= wm.menu.count || !(wm.menu.rows[row].flags & UI_MENU_CONFIRM) || wm.menu.rows[row].armed) {
        return false;
    }

    wm.menu.rows[row].armed = true;

    return true;
}


/**
 * @brief Puts the menu away after a row was picked, telling the client that asked for it which one.
 *
 * @param row The row's place in the menu.
 * @return false for a menu of the taskbar's, whose rows the taskbar acts on itself.
 */
bool wm_menu_choose(int row) {

    if (wm.menu.anchor != WM_MENU_POINTER || row < 0 || row >= wm.menu.count) {
        return false;
    }

    wm_menu_answer(wm.menu.rows[row].action);
    wm_menu_hide();

    return true;
}


/**
 * @brief Puts away a menu a window asked for when the window goes, with no one left to answer.
 *
 * @param win The window.
 */
void wm_menu_forget(const wm_window_t* win) {

    if (wm.menu.window_id != win->id) {
        return;
    }

    wm.menu.pending   = false;
    wm.menu.window_id = 0;

    wm_menu_hide();
}


/**
 * @brief Slides the menu back in, telling a client that asked for it that nothing was picked.
 */
void wm_menu_hide(void) {

    wm_menu_answer(0);

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
 * @return The row's place in the menu, or -1 for none, a rule or a row greyed out.
 */
int wm_menu_row_at(int x, int y) {

    if (ui_anim_target(&wm.menu.open) < 1.0) {
        return -1;
    }

    for (int i = 0; i < wm.menu.count; i++) {

        const wm_rect_t r = wm_menu_row_rect(i);

        if (!(wm.menu.rows[i].flags & (UI_MENU_SEPARATOR | UI_MENU_DISABLED)) && wm_rect_contains_point(&r, x, y)) {
            return i;
        }
    }

    return -1;
}


/**
 * @brief Finds the row the keyboard moves to: the next one that can be picked in a direction, round past either end.
 *
 * @param from The row highlighted now, or -1 for none, which starts from the top going down and from the bottom going up.
 * @param step 1 to go down, -1 to go up.
 * @return The row's place in the menu, or -1 when no row can be picked.
 */
int wm_menu_next(int from, int step) {

    const int count = wm.menu.count;

    for (int i = 1; i <= count; i++) {

        const int at = from < 0 ? (step > 0 ? i - 1 : count - i) : ((from + step * i) % count + count) % count;

        if (!(wm.menu.rows[at].flags & (UI_MENU_SEPARATOR | UI_MENU_DISABLED))) {
            return at;
        }
    }

    return -1;
}


/**
 * @brief Reports how far out the rail's menu is for an item, which that item's wash is held lit by.
 *
 * @param owner The item.
 * @param now The clock reading.
 * @return 0 to 1, and 0 when the menu belongs to another item or to a client.
 */
double wm_menu_hold(uint32_t owner, uint64_t now) {
    return wm.menu.anchor == WM_MENU_RAIL && owner && owner == wm.menu.owner ? ui_anim_value(&wm.menu.open, now) : 0.0;
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

    if (wm.menu.anchor == WM_MENU_POINTER) {

        const wm_rect_t r = {panel.x - 1, panel.y - 1, panel.width + 2, panel.height + 2};

        return wm_taskbar_shadow_extent(r);
    }

    const wm_rect_t r = {panel.x, panel.y - WM_TASKBAR_POPOUT_RADIUS - 2, (int)ceil(panel.width * open) + 2, panel.height + 2 * WM_TASKBAR_POPOUT_RADIUS + 4};

    return wm_taskbar_shadow_extent(r);
}


/**
 * @brief Reports whether a client's menu is frosted this frame.
 *
 * @param shape Receives its panel.
 * @return true while a client's menu is on its way out, out, or on its way in.
 */
bool wm_menu_frosted(wm_rect_t* shape) {

    if (wm.menu.anchor != WM_MENU_POINTER || ui_anim_value(&wm.menu.open, wm.frame_time) <= 0.0) {
        return false;
    }

    *shape = wm.menu.rect;

    return true;
}


/**
 * @brief Draws a row's mark: a plus to open, a pushpin to pin, the pushpin struck through to unpin, a red cross to quit.
 *
 * @param cr The cairo context.
 * @param palette The menu's palette.
 * @param glyph The mark.
 * @param x The centre.
 * @param y The centre.
 * @param alpha How opaque to draw it.
 */
static void wm_menu_paint_glyph(cairo_t* cr, const wm_menu_palette_t* palette, wm_menu_glyph_t glyph, double x, double y, double alpha) {

    const double g = WM_TASKBAR_SESSION_GLYPH;
    const double d = g - 2.0;

    if (glyph == WM_MENU_GLYPH_NONE) {
        return;
    }

    cairo_save(cr);

    cairo_set_line_width(cr, glyph == WM_MENU_GLYPH_PIN || glyph == WM_MENU_GLYPH_UNPIN ? 1.5 : 2.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    wm_source(cr, glyph == WM_MENU_GLYPH_QUIT ? palette->danger : palette->text, alpha);

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
 * @brief Fills a row's rounded wash: the pressed one while it is held, else the hover one as lit as it is.
 *
 * @param cr The cairo context.
 * @param palette The menu's palette.
 * @param r The row.
 * @param hover How lit it is, 0 to 1.
 * @param held Whether it is held down.
 */
static void wm_menu_paint_wash(cairo_t* cr, const wm_menu_palette_t* palette, const wm_rect_t* r, double hover, bool held) {

    if (!held && hover <= 0.0) {
        return;
    }

    wm_source(cr, held ? palette->down : palette->hover, held ? 1.0 : hover);
    ui_draw_rounded_rect_d(cr, r->x, r->y, r->width, r->height, palette->radius);
    cairo_fill(cr);
}


/**
 * @brief Draws one row: a rule, or its wash, its mark or icon and its label, greyed out or in red as its flags say.
 *
 * @param cr The cairo context, clipped to the menu, its font set.
 * @param palette The menu's palette.
 * @param index The row's place in the menu.
 * @param dx How far the menu still is from all the way out across, which the row is slid back by.
 * @param dy How far it still is from all the way out down or up.
 * @param alpha How opaque to draw it.
 * @param held Whether the row is pressed with the pointer still on it.
 */
static void wm_menu_paint_row(cairo_t* cr, const wm_menu_palette_t* palette, int index, int dx, int dy, double alpha, bool held) {

    wm_rect_t r = wm_menu_row_rect(index);

    r.x += dx;
    r.y += dy;

    const uint32_t flags = wm.menu.rows[index].flags;

    if (flags & UI_MENU_SEPARATOR) {

        const double y = floor(r.y + r.height / 2.0) + 0.5;

        cairo_new_path(cr);
        cairo_move_to(cr, r.x, y);
        cairo_line_to(cr, r.x + r.width, y);
        cairo_set_line_width(cr, 1.0);
        wm_source(cr, palette->rule, alpha);
        cairo_stroke(cr);

        return;
    }


    const double fade = flags & UI_MENU_DISABLED ? alpha * 0.4 : alpha;

    wm_menu_paint_wash(cr, palette, &r, ui_anim_value(&wm.menu.hover[index], wm.frame_time), held);
    wm_menu_paint_glyph(cr, palette, wm.menu.rows[index].glyph, r.x + WM_TASKBAR_ITEM / 2.0, r.y + r.height / 2.0, fade);

    cairo_surface_t* icon = wm.menu.rows[index].icon;

    if (icon) {

        cairo_save(cr);
        cairo_set_source_surface(cr, icon, r.x + (WM_TASKBAR_ITEM - WM_MENU_ICON) / 2, r.y + (WM_TASKBAR_ITEM - WM_MENU_ICON) / 2);
        cairo_paint_with_alpha(cr, fade);
        cairo_restore(cr);
    }


    cairo_font_extents_t font;

    cairo_font_extents(cr, &font);

    wm_source(cr, flags & UI_MENU_DISABLED ? palette->dim : flags & UI_MENU_DANGER ? palette->danger : palette->text, alpha);
    cairo_move_to(cr, r.x + wm.menu.indent, round(r.y + (r.height - (font.ascent + font.descent)) / 2.0 + font.ascent));
    cairo_show_text(cr, wm.menu.rows[index].armed ? wm.menu.rows[index].confirm : wm.menu.rows[index].label);
}


/**
 * @brief Traces a client's menu as a rounded rectangle whose corner at the point it opened from is left square.
 *
 * @param cr The cairo context.
 * @param x The left edge.
 * @param y The top edge.
 * @param width The width.
 * @param height The height.
 * @param radius The radius of the other three corners.
 */
static void wm_menu_trace(cairo_t* cr, double x, double y, double width, double height, double radius) {

    const bool up   = wm.menu.upwards;
    const bool left = wm.menu.leftwards;

    const double tl = !up && !left ? 0.0 : radius;
    const double tr = !up && left ? 0.0 : radius;
    const double br = up && left ? 0.0 : radius;
    const double bl = up && !left ? 0.0 : radius;

    cairo_new_sub_path(cr);
    cairo_arc(cr, x + width - tr, y + tr, tr, -M_PI / 2.0, 0.0);
    cairo_arc(cr, x + width - br, y + height - br, br, 0.0, M_PI / 2.0);
    cairo_arc(cr, x + bl, y + height - bl, bl, M_PI / 2.0, M_PI);
    cairo_arc(cr, x + tl, y + tl, tl, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}


/**
 * @brief Reports how much of a client's menu has dropped out at a moment.
 *
 * @param open How far out it is, 0 to 1.
 * @param top Receives the top edge of what is out.
 * @param radius Receives the radius of its rounded corners.
 * @return How tall what is out is.
 */
static double wm_menu_reach(double open, double* top, double* radius) {

    const wm_rect_t panel = wm.menu.rect;
    const double reach    = panel.height * open;

    *top    = wm.menu.upwards ? panel.y + panel.height - reach : panel.y;
    *radius = WM_MIN(WM_TASKBAR_POPOUT_RADIUS, reach / 2.0);

    return reach;
}


/**
 * @brief Casts a client's menu's shadow as a focused window's, kept inside the work area like the windows' own.
 *
 * @param cr The cairo context.
 * @param shape What casts it.
 * @param strength How far out the menu is, 0 to 1, which the shadow darkens with.
 */
static void wm_menu_shadow(cairo_t* cr, const wm_rect_t* shape, double strength) {

    const wm_rect_t work = wm_workarea();

    cairo_save(cr);

    cairo_new_path(cr);
    ui_draw_rounded_rect_d(cr, work.x, work.y, work.width, work.height, WM_TASKBAR_RADIUS);
    cairo_clip(cr);

    wm_shadow_paint(cr, &wm.shadow_key, shape, WM_SHADOW_KEY_OFFSET, wm.theme.shadow_key * strength);
    wm_shadow_paint(cr, &wm.shadow_ambient, shape, 0, wm.theme.shadow_ambient * strength);

    cairo_restore(cr);
}


/**
 * @brief Strokes a client's menu's edges inside its shape as a focused window's: the outline, and the highlight within it.
 *
 * @param cr The cairo context.
 * @param open How far out it is, 0 to 1.
 */
static void wm_menu_paint_edges(cairo_t* cr, double open) {

    const wm_rect_t panel = wm.menu.rect;

    double top;
    double radius;

    const double reach = wm_menu_reach(open, &top, &radius);

    if (reach < 4.0) {
        return;
    }

    cairo_save(cr);

    cairo_set_line_width(cr, 1.0);

    cairo_new_path(cr);
    wm_menu_trace(cr, panel.x + 0.5, top + 0.5, panel.width - 1.0, reach - 1.0, WM_MAX(radius - 0.5, 0.0));
    wm_source(cr, wm.theme.outline, 1.0);
    cairo_stroke(cr);

    cairo_new_path(cr);
    wm_menu_trace(cr, panel.x + 1.5, top + 1.5, panel.width - 3.0, reach - 3.0, WM_MAX(radius - 1.5, 0.0));
    wm_source(cr, wm.theme.highlight, 1.0);
    cairo_stroke(cr);

    cairo_restore(cr);
}


/**
 * @brief Traces the menu's shape at a moment, casts its shadow, then fills it and clips to it.
 *
 * @param cr The cairo context.
 * @param palette The menu's palette.
 * @param open How far out it is, 0 to 1.
 * @param dx Receives how far its contents are slid back across.
 * @param dy Receives how far its contents are slid back down or up.
 * @return false while too little of it is out to draw, with nothing drawn.
 */
static bool wm_menu_paint_shape(cairo_t* cr, const wm_menu_palette_t* palette, double open, int* dx, int* dy) {

    const wm_rect_t panel = wm.menu.rect;

    if (wm.menu.anchor == WM_MENU_POINTER) {

        double top;
        double radius;

        const double reach = wm_menu_reach(open, &top, &radius);

        if (reach < 1.0) {
            return false;
        }

        const wm_rect_t shape = {panel.x, (int)top, panel.width, (int)ceil(reach)};
        const bool frosted    = wm_blur_capture(&wm.menu.backdrop, &panel);

        wm_menu_shadow(cr, &shape, open);

        cairo_new_path(cr);
        wm_menu_trace(cr, panel.x, top, panel.width, reach, radius);
        cairo_clip(cr);

        if (frosted) {
            wm_blur_paint(cr, &wm.menu.backdrop);
        }

        cairo_set_source_rgba(cr, palette->fill.r, palette->fill.g, palette->fill.b, frosted ? palette->fill.a : 1.0);
        cairo_paint(cr);

        *dx = 0;
        *dy = (int)round(wm.menu.upwards ? panel.height - reach : reach - panel.height);

        return true;
    }


    const double x0 = panel.x;
    const double x1 = x0 + panel.width * open;

    if (x1 - x0 < 1.0) {
        return false;
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

    cairo_new_path(cr);
    wm_taskbar_popout_outline(cr, x0, x1, top, bottom, fillet, corner, 0.0);
    cairo_close_path(cr);

    wm_source(cr, palette->fill, 1.0);
    cairo_fill_preserve(cr);
    cairo_clip(cr);

    *dx = -(int)round(panel.width - (x1 - x0));
    *dy = 0;

    return true;
}


/**
 * @brief Draws the menu on its way out or in: its title and a rule under it when it has one, its rows, and a client's menu's edges over them.
 *
 * @param cr The cairo context.
 * @param held The row pressed with the pointer still on it, or -1.
 */
void wm_menu_paint(cairo_t* cr, int held) {

    wm.menu.painted = wm_menu_extent(wm.frame_time);

    const double open     = ui_anim_value(&wm.menu.open, wm.frame_time);
    const wm_rect_t panel = wm.menu.rect;

    const wm_menu_palette_t palette = wm_menu_palette();

    cairo_font_face_t* face = ui_font_face(palette.font);

    if (open <= 0.0 || !face) {
        return;
    }


    int dx = 0;
    int dy = 0;

    cairo_save(cr);

    if (!wm_menu_paint_shape(cr, &palette, open, &dx, &dy)) {
        cairo_restore(cr);
        return;
    }


    cairo_font_extents_t font;

    cairo_set_font_face(cr, face);
    cairo_set_font_size(cr, palette.size);
    cairo_font_extents(cr, &font);

    if (wm.menu.title[0]) {

        const double top  = panel.y + dy + WM_TASKBAR_PAD;
        const double rule = top + WM_TASKBAR_ITEM + WM_MENU_RULE / 2 + 0.5;

        wm_source(cr, palette.muted, open);
        cairo_move_to(cr, panel.x + dx + WM_TASKBAR_POPOUT_PAD, round(top + (WM_TASKBAR_ITEM - (font.ascent + font.descent)) / 2.0 + font.ascent));
        cairo_show_text(cr, wm.menu.title);

        cairo_new_path(cr);
        cairo_move_to(cr, panel.x + dx + WM_TASKBAR_PAD, rule);
        cairo_line_to(cr, panel.x + dx + panel.width - WM_TASKBAR_PAD, rule);
        cairo_set_line_width(cr, 1.0);
        wm_source(cr, palette.rule, open);
        cairo_stroke(cr);
    }

    for (int i = 0; i < wm.menu.count; i++) {
        wm_menu_paint_row(cr, &palette, i, dx, dy, open, i == held);
    }

    if (wm.menu.anchor == WM_MENU_POINTER) {
        wm_menu_paint_edges(cr, open);
    }

    cairo_restore(cr);
}


void wm_menu_fini(void) {

    wm.menu.pending = false;

    wm_menu_release();

    wm_blur_fini(&wm.menu.backdrop);
}
