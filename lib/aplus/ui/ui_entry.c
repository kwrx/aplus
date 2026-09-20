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
 * @brief One line of editable text, and the only widget that reads a keymap.
 */

#include <string.h>

#include <aplus/input.h>

#include "ui_widget_internal.h"


/**
 * @brief How far the text sits inside the well, on both sides.
 */
#define UI_ENTRY_PADDING 10

/**
 * @brief How wide the caret is drawn.
 */
#define UI_ENTRY_CARET_WIDTH 2

/**
 * @brief How long the caret spends shown, and then hidden, in milliseconds.
 */
#define UI_ENTRY_BLINK_MS 530


static void ui_entry_draw(ui_widget_t* widget, cairo_t* cr);
static bool ui_entry_on_press(ui_widget_t* widget, int x, int y);
static bool ui_entry_on_key(ui_widget_t* widget, uint16_t vkey, bool down);
static bool ui_entry_on_focus(ui_widget_t* widget, bool focused);
static int ui_entry_tick(ui_widget_t* widget, uint64_t now);
static void ui_entry_on_destroy(ui_widget_t* widget);


static const ui_widget_ops_t ui_entry_ops = {

    .draw     = ui_entry_draw,
    .on_press = ui_entry_on_press,
    .on_key   = ui_entry_on_key,
    .on_focus = ui_entry_on_focus,
    .tick     = ui_entry_tick,
    .destroy  = ui_entry_on_destroy,
};


ui_widget_t* ui_entry_create(ui_view_t* view, const char* placeholder) {

    ui_widget_t* widget = ui_widget_new(view, UI_WIDGET_ENTRY, &ui_entry_ops, true);

    if (!widget) {
        return NULL;
    }

    widget->entry.weight = UI_FONT_REGULAR;
    widget->entry.size   = widget->theme->font_size;

    if (placeholder) {
        strncpy(widget->entry.placeholder, placeholder, sizeof(widget->entry.placeholder) - 1);
    }

    return widget;
}


/**
 * @brief Reports the font an entry draws with.
 *
 * @param widget The entry.
 * @return The path of the font file.
 */
static const char* ui_entry_font(const ui_widget_t* widget) {

    return widget->entry.weight == UI_FONT_BOLD ? widget->theme->font_bold : widget->theme->font_regular;
}


/**
 * @brief Reports where the text is drawn, which is the well less its padding.
 *
 * @param widget The entry.
 * @return The rectangle.
 */
static ui_rect_t ui_entry_text_rect(const ui_widget_t* widget) {

    return ui_rect_inset(widget->rect, UI_ENTRY_PADDING);
}


/**
 * @brief Reports how wide the text is up to a byte offset.
 *
 * @param widget The entry.
 * @param offset How far into the text to measure.
 * @return The width in pixels.
 */
static double ui_entry_width_to(const ui_widget_t* widget, size_t offset) {

    char head[UI_ENTRY_TEXT_MAX];

    if (offset >= sizeof(head)) {
        offset = sizeof(head) - 1;
    }

    memcpy(head, widget->entry.text, offset);

    head[offset] = '\0';

    return ui_draw_text_width(head, ui_entry_font(widget), widget->entry.size);
}


/**
 * @brief Puts the caret back in view: scrolls the least that brings it into the field, and shows it.
 *
 * Every path that moves the caret or changes the text comes through here, which is also
 * where the blink has to start over: a caret hidden by its own cycle just as a key lands
 * reads as a dropped keystroke.
 *
 * @param widget The entry.
 */
static void ui_entry_reveal_caret(ui_widget_t* widget) {

    widget->entry.caret_on = true;
    widget->entry.caret_at = ui_now_ms();


    const ui_rect_t text = ui_entry_text_rect(widget);

    if (text.width <= 0) {

        widget->entry.scroll = 0;
        return;
    }


    const int caret = (int)ui_entry_width_to(widget, widget->entry.caret);
    const int full  = (int)ui_draw_text_width(widget->entry.text, ui_entry_font(widget), widget->entry.size);

    if (caret - widget->entry.scroll > text.width - UI_ENTRY_CARET_WIDTH) {
        widget->entry.scroll = caret - text.width + UI_ENTRY_CARET_WIDTH;
    }

    if (caret - widget->entry.scroll < 0) {
        widget->entry.scroll = caret;
    }

    if (widget->entry.scroll > full - text.width) {
        widget->entry.scroll = full - text.width;
    }

    if (widget->entry.scroll < 0) {
        widget->entry.scroll = 0;
    }
}


/**
 * @brief Reports the offset of the character before one, stepping over a UTF-8 sequence whole.
 *
 * @param widget The entry.
 * @param offset Where to step back from.
 * @return The offset of the previous character, or the same one at the start of the text.
 */
static size_t ui_entry_prev(const ui_widget_t* widget, size_t offset) {

    while (offset > 0) {

        offset--;

        if (((unsigned char)widget->entry.text[offset] & 0xC0) != 0x80) {
            break;
        }
    }

    return offset;
}


/**
 * @brief Reports the offset of the character after one, stepping over a UTF-8 sequence whole.
 *
 * @param widget The entry.
 * @param offset Where to step forward from.
 * @return The offset of the next character, or the end of the text.
 */
static size_t ui_entry_next(const ui_widget_t* widget, size_t offset) {

    const size_t length = strlen(widget->entry.text);

    while (offset < length) {

        offset++;

        if (offset >= length || ((unsigned char)widget->entry.text[offset] & 0xC0) != 0x80) {
            break;
        }
    }

    return offset;
}


/**
 * @brief Runs the change callback, which is where a search field refilters.
 *
 * @param widget The entry.
 */
static void ui_entry_changed(ui_widget_t* widget) {

    ui_entry_reveal_caret(widget);
    ui_widget_invalidate(widget);

    if (widget->entry.on_change) {
        widget->entry.on_change(widget, widget->entry.on_change_user);
    }
}


/**
 * @brief Inserts text at the caret, dropping the control bytes a keymap also produces.
 *
 * @param widget The entry.
 * @param text The bytes to insert.
 * @param size How many of them there are.
 * @return true when anything was inserted.
 */
static bool ui_entry_insert(ui_widget_t* widget, const char* text, size_t size) {

    char clean[8];
    size_t taken = 0;

    for (size_t i = 0; i < size && taken < sizeof(clean); i++) {

        const unsigned char byte = (unsigned char)text[i];

        if (byte >= 0x20 && byte != 0x7F) {
            clean[taken++] = text[i];
        }
    }

    if (taken == 0) {
        return false;
    }


    const size_t length = strlen(widget->entry.text);

    if (length + taken >= sizeof(widget->entry.text)) {
        return false;
    }

    memmove(widget->entry.text + widget->entry.caret + taken, widget->entry.text + widget->entry.caret, length - widget->entry.caret + 1);
    memcpy(widget->entry.text + widget->entry.caret, clean, taken);

    widget->entry.caret += taken;

    ui_entry_changed(widget);

    return true;
}


/**
 * @brief Removes the text between two offsets.
 *
 * @param widget The entry.
 * @param from The first byte to remove.
 * @param to One past the last.
 * @return true when anything was removed.
 */
static bool ui_entry_erase(ui_widget_t* widget, size_t from, size_t to) {

    if (from >= to) {
        return false;
    }

    memmove(widget->entry.text + from, widget->entry.text + to, strlen(widget->entry.text) - to + 1);

    widget->entry.caret = from;

    ui_entry_changed(widget);

    return true;
}


/**
 * @brief Reports the entry's keymap, loading it the first time it is asked for.
 *
 * @param widget The entry.
 * @return The keymap, or NULL when there is none to be had.
 */
static ui_keymap_t* ui_entry_keymap(ui_widget_t* widget) {

    if (!widget->entry.keymap_tried) {

        widget->entry.keymap_tried = true;
        widget->entry.keymap       = ui_keymap_open(NULL);
    }

    return widget->entry.keymap;
}


void ui_entry_set_text(ui_widget_t* widget, const char* text) {

    if (!widget || widget->kind != UI_WIDGET_ENTRY) {
        return;
    }


    char next[UI_ENTRY_TEXT_MAX] = {0};

    if (text) {
        strncpy(next, text, sizeof(next) - 1);
    }

    if (strcmp(next, widget->entry.text) == 0) {
        return;
    }

    memcpy(widget->entry.text, next, sizeof(next));

    widget->entry.caret = strlen(widget->entry.text);

    ui_entry_reveal_caret(widget);
    ui_widget_invalidate(widget);
}


const char* ui_entry_text(const ui_widget_t* widget) {

    if (!widget || widget->kind != UI_WIDGET_ENTRY) {
        return NULL;
    }

    return widget->entry.text;
}


void ui_entry_set_placeholder(ui_widget_t* widget, const char* text) {

    if (!widget || widget->kind != UI_WIDGET_ENTRY) {
        return;
    }


    char next[UI_LABEL_TEXT_MAX] = {0};

    if (text) {
        strncpy(next, text, sizeof(next) - 1);
    }

    if (strcmp(next, widget->entry.placeholder) == 0) {
        return;
    }

    memcpy(widget->entry.placeholder, next, sizeof(next));

    ui_widget_invalidate(widget);
}


void ui_entry_set_font(ui_widget_t* widget, ui_font_weight_t weight, double size) {

    if (!widget || widget->kind != UI_WIDGET_ENTRY) {
        return;
    }

    widget->entry.weight = weight;
    widget->entry.size   = size > 0.0 ? size : widget->theme->font_size;

    ui_entry_reveal_caret(widget);
    ui_widget_invalidate(widget);
}


void ui_entry_on_change(ui_widget_t* widget, ui_action_fn fn, void* user) {

    if (!widget || widget->kind != UI_WIDGET_ENTRY) {
        return;
    }

    widget->entry.on_change      = fn;
    widget->entry.on_change_user = user;
}


void ui_entry_on_submit(ui_widget_t* widget, ui_action_fn fn, void* user) {

    if (!widget || widget->kind != UI_WIDGET_ENTRY) {
        return;
    }

    widget->entry.on_submit      = fn;
    widget->entry.on_submit_user = user;
}


static void ui_entry_draw(ui_widget_t* widget, cairo_t* cr) {

    const ui_theme_t* theme = widget->theme;

    const double radius = theme->corner_radius;


    ui_draw_rounded_rect(cr, widget->rect, radius);
    ui_draw_set_color(cr, theme->surface_sunken);
    cairo_fill(cr);


    const ui_rect_t text = ui_entry_text_rect(widget);

    const bool empty = widget->entry.text[0] == '\0';

    ui_color_t color = empty ? theme->text_muted : theme->text;

    if (!widget->enabled) {
        color = ui_draw_dim(color, theme);
    }


    cairo_save(cr);

    cairo_rectangle(cr, text.x, text.y, text.width, text.height);
    cairo_clip(cr);


    const ui_rect_t scrolled = {text.x - widget->entry.scroll, text.y, text.width + widget->entry.scroll, text.height};

    ui_draw_text(cr, scrolled, empty ? widget->entry.placeholder : widget->entry.text, ui_entry_font(widget), widget->entry.size, color, UI_ALIGN_LEFT);


    if (widget->entry.focused && widget->entry.caret_on && widget->enabled) {

        const int caret = text.x + (int)ui_entry_width_to(widget, widget->entry.caret) - widget->entry.scroll;

        cairo_rectangle(cr, caret, text.y, UI_ENTRY_CARET_WIDTH, text.height);
        ui_draw_set_color(cr, theme->text_muted);
        cairo_fill(cr);
    }

    cairo_restore(cr);


    ui_draw_rounded_rect_d(cr, widget->rect.x + 0.5, widget->rect.y + 0.5, widget->rect.width - 1.0, widget->rect.height - 1.0, radius);
    ui_draw_set_color(cr, widget->entry.focused ? theme->focus_ring : theme->border);

    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
}


static bool ui_entry_on_press(ui_widget_t* widget, int x, int y) {

    (void)y;

    const ui_rect_t text = ui_entry_text_rect(widget);

    const int target = x - text.x + widget->entry.scroll;

    const size_t length = strlen(widget->entry.text);

    size_t caret = 0;

    while (caret < length) {

        const size_t next = ui_entry_next(widget, caret);

        if (ui_entry_width_to(widget, next) > (double)target) {
            break;
        }

        caret = next;
    }

    widget->entry.caret = caret;

    ui_entry_reveal_caret(widget);

    return true;
}


static bool ui_entry_on_key(ui_widget_t* widget, uint16_t vkey, bool down) {

    char produced[8];

    const size_t size = ui_keymap_translate(ui_entry_keymap(widget), vkey, down, produced, sizeof(produced));

    if (!down) {
        return false;
    }


    switch (vkey) {

        case KEY_BACKSPACE:
            ui_entry_erase(widget, ui_entry_prev(widget, widget->entry.caret), widget->entry.caret);
            return true;

        case KEY_DELETE:
            ui_entry_erase(widget, widget->entry.caret, ui_entry_next(widget, widget->entry.caret));
            return true;

        case KEY_LEFT:
            widget->entry.caret = ui_entry_prev(widget, widget->entry.caret);
            ui_entry_reveal_caret(widget);
            ui_widget_invalidate(widget);
            return true;

        case KEY_RIGHT:
            widget->entry.caret = ui_entry_next(widget, widget->entry.caret);
            ui_entry_reveal_caret(widget);
            ui_widget_invalidate(widget);
            return true;

        case KEY_HOME:
            widget->entry.caret = 0;
            ui_entry_reveal_caret(widget);
            ui_widget_invalidate(widget);
            return true;

        case KEY_END:
            widget->entry.caret = strlen(widget->entry.text);
            ui_entry_reveal_caret(widget);
            ui_widget_invalidate(widget);
            return true;

        case KEY_ENTER:
        case KEY_KPENTER:

            if (widget->entry.on_submit) {
                widget->entry.on_submit(widget, widget->entry.on_submit_user);
            }

            return true;

        case KEY_ESC:
        case KEY_TAB:
        case KEY_UP:
        case KEY_DOWN:
        case KEY_PAGEUP:
        case KEY_PAGEDOWN:
            return false;

        default:
            break;
    }

    return ui_entry_insert(widget, produced, size);
}


static bool ui_entry_on_focus(ui_widget_t* widget, bool focused) {

    if (widget->entry.focused == focused) {
        return false;
    }

    widget->entry.focused = focused;

    if (focused) {
        ui_entry_reveal_caret(widget);
    } else {
        ui_keymap_reset(widget->entry.keymap);
    }

    return true;
}


/**
 * @brief Blinks the caret, which is the only thing in the library that paints on a clock.
 *
 * @param widget The entry.
 * @param now The loop's reading of the monotonic clock, in milliseconds.
 * @return How long until the caret flips again, or -1 when there is no caret to blink.
 */
static int ui_entry_tick(ui_widget_t* widget, uint64_t now) {

    if (!widget->entry.focused || !widget->enabled) {
        return -1;
    }


    uint64_t elapsed = now - widget->entry.caret_at;

    if (elapsed >= UI_ENTRY_BLINK_MS) {

        widget->entry.caret_on = !widget->entry.caret_on;
        widget->entry.caret_at = now;

        elapsed = 0;

        ui_widget_invalidate(widget);
    }

    return (int)(UI_ENTRY_BLINK_MS - elapsed);
}


static void ui_entry_on_destroy(ui_widget_t* widget) {

    ui_keymap_close(widget->entry.keymap);

    widget->entry.keymap = NULL;
}
