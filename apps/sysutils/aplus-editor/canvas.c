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
 * @brief The text area, drawn by hand into the frame the view leaves for it.
 */

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aplus/ui-anim.h>
#include <aplus/ui-draw.h>

#include "editor.h"


/**
 * @brief How far inside the frame the text area is drawn, so that the frame's border stays visible.
 */
#define EDITOR_FRAME_INSET 2

/**
 * @brief The space either side of the line numbers, and how many digits the gutter is at least wide for.
 */
#define EDITOR_GUTTER_PAD    12
#define EDITOR_GUTTER_DIGITS 3

/**
 * @brief The space between the gutter and the text, and above the first line.
 */
#define EDITOR_TEXT_PAD 8
#define EDITOR_TEXT_TOP 6

/**
 * @brief The space added between lines, on top of the font's own height.
 */
#define EDITOR_LINE_SPACING 4

/**
 * @brief The scrollbar: the strip it takes, the width of its thumb, how far it stays off the ends, and how short the thumb gets.
 */
#define EDITOR_SCROLLBAR_WIDTH 12
#define EDITOR_SCROLLBAR_THUMB 6
#define EDITOR_SCROLLBAR_INSET 4
#define EDITOR_SCROLLBAR_MIN   24

/**
 * @brief How wide the caret is, and how long it spends shown, then hidden.
 */
#define EDITOR_CARET_WIDTH 2
#define EDITOR_BLINK_MS    530

/**
 * @brief How many columns are kept past the caret when the text scrolls sideways to follow it.
 */
#define EDITOR_REVEAL_MARGIN 4


/**
 * @brief The washes the text area paints under the text.
 */
static const ui_color_t editor_color_match = UI_RGBA(0xF5, 0xC2, 0x11, 0.28);
static const ui_color_t editor_color_idle  = UI_RGBA(0xFF, 0xFF, 0xFF, 0.12);

#define EDITOR_SELECTION_ALPHA    0.40
#define EDITOR_CURRENT_LINE_ALPHA 0.045
#define EDITOR_THUMB_ALPHA        0.22
#define EDITOR_THUMB_HOT_ALPHA    0.45


/**
 * @brief Sets the source to a colour.
 * @param cr The context.
 * @param color The colour.
 */
static void editor_canvas_color(cairo_t* cr, ui_color_t color) {

    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
}


/**
 * @brief Reports an opaque colour at another alpha.
 * @param color The colour.
 * @param alpha The alpha.
 * @return The colour.
 */
static ui_color_t editor_canvas_alpha(ui_color_t color, double alpha) {

    color.a = alpha;

    return color;
}


/**
 * @brief Reports the glyph a codepoint stands for, out of a cache for the codepoints a document mostly holds.
 * @param codepoint The codepoint.
 * @return The glyph index, 0 when the face has none.
 */
static unsigned long editor_canvas_glyph(uint32_t codepoint) {

    editor_canvas_t* canvas = &editor.canvas;

    if (codepoint < EDITOR_GLYPH_CACHE_MAX && canvas->glyphs[codepoint] >= 0) {
        return (unsigned long)canvas->glyphs[codepoint];
    }


    char utf8[4];

    const size_t size = editor_utf8_encode(codepoint, utf8);

    cairo_glyph_t buffer[4];
    cairo_glyph_t* glyphs = buffer;

    int count = (int)(sizeof(buffer) / sizeof(buffer[0]));

    unsigned long index = 0;

    if (cairo_scaled_font_text_to_glyphs(canvas->font, 0.0, 0.0, utf8, (int)size, &glyphs, &count, NULL, NULL, NULL) == CAIRO_STATUS_SUCCESS && count > 0) {
        index = glyphs[0].index;
    }

    if (glyphs != buffer) {
        cairo_glyph_free(glyphs);
    }

    if (codepoint < EDITOR_GLYPH_CACHE_MAX) {
        canvas->glyphs[codepoint] = (int32_t)index;
    }

    return index;
}


/**
 * @brief Reports how many digits the gutter is wide for.
 * @return The count.
 */
static size_t editor_canvas_count_digits(void) {

    size_t digits = 1;

    for (size_t n = editor.buffer.count; n >= 10; n /= 10) {
        digits++;
    }

    return digits < EDITOR_GUTTER_DIGITS ? EDITOR_GUTTER_DIGITS : digits;
}


int editor_canvas_init(void) {

    editor_canvas_t* canvas = &editor.canvas;

    cairo_font_face_t* face = ui_font_face(EDITOR_FONT_PATH);

    if (!face) {
        errno = ENOENT;
        return -1;
    }

    cairo_scaled_font_t* scaled = ui_font_scaled(face, EDITOR_FONT_SIZE);

    if (!scaled) {
        errno = ENOMEM;
        return -1;
    }

    canvas->font = cairo_scaled_font_reference(scaled);

    memset(canvas->glyphs, 0xFF, sizeof(canvas->glyphs));


    cairo_font_extents_t extents;
    cairo_text_extents_t letter;

    cairo_scaled_font_extents(canvas->font, &extents);
    cairo_scaled_font_text_extents(canvas->font, "M", &letter);

    canvas->cell     = (int)ceil(letter.x_advance > 0.0 ? letter.x_advance : extents.max_x_advance);
    canvas->line     = (int)ceil(extents.ascent) + (int)ceil(extents.descent) + EDITOR_LINE_SPACING;
    canvas->baseline = EDITOR_LINE_SPACING / 2 + (int)ceil(extents.ascent);

    if (canvas->cell < 1) {
        canvas->cell = 1;
    }

    if (canvas->line < 1) {
        canvas->line = 1;
    }


    canvas->replacement = editor_canvas_glyph(EDITOR_UTF8_INVALID);

    if (!canvas->replacement) {
        canvas->replacement = editor_canvas_glyph('?');
    }

    canvas->digits   = editor_canvas_count_digits();
    canvas->caret_on = true;
    canvas->caret_at = ui_anim_now();

    return 0;
}


void editor_canvas_free(void) {

    editor_canvas_t* canvas = &editor.canvas;

    if (canvas->font) {
        cairo_scaled_font_destroy(canvas->font);
    }

    free(canvas->run);

    canvas->font         = NULL;
    canvas->run          = NULL;
    canvas->run_capacity = 0;
}


/**
 * @brief Reports the part of the frame the text area paints, which is all of it but the border.
 * @return The rectangle, in window coordinates.
 */
static ui_rect_t editor_canvas_inner(void) {

    return ui_rect_inset(editor.canvas.area, EDITOR_FRAME_INSET);
}


/**
 * @brief Reports how wide the gutter is.
 * @return The width in pixels.
 */
static int editor_canvas_gutter(void) {

    return (int)editor.canvas.digits * editor.canvas.cell + EDITOR_GUTTER_PAD * 2;
}


ui_rect_t editor_canvas_text_rect(void) {

    const ui_rect_t inner = editor_canvas_inner();

    ui_rect_t text = {

        .x      = inner.x + editor_canvas_gutter() + EDITOR_TEXT_PAD,
        .y      = inner.y + EDITOR_TEXT_TOP,
        .width  = inner.x + inner.width - EDITOR_SCROLLBAR_WIDTH - (inner.x + editor_canvas_gutter() + EDITOR_TEXT_PAD),
        .height = inner.height - EDITOR_TEXT_TOP,
    };

    if (text.width < 0) {
        text.width = 0;
    }

    if (text.height < 0) {
        text.height = 0;
    }

    return text;
}


size_t editor_canvas_rows(void) {

    const size_t rows = (size_t)(editor_canvas_text_rect().height / editor.canvas.line);

    return rows > 0 ? rows : 1;
}


size_t editor_canvas_columns(void) {

    const size_t columns = (size_t)(editor_canvas_text_rect().width / editor.canvas.cell);

    return columns > 0 ? columns : 1;
}


/**
 * @brief Reports how far down the text can be scrolled, which stops with the last line at the bottom.
 * @return The highest first visible line.
 */
static size_t editor_canvas_max_top(void) {

    const size_t rows = editor_canvas_rows();

    return editor.buffer.count > rows ? editor.buffer.count - rows : 0;
}


bool editor_canvas_focused(void) {

    return editor.active && ui_view_focused(editor.view) == NULL;
}


void editor_canvas_dirty_all(void) {

    const ui_rect_t inner = editor_canvas_inner();

    if (inner.width > 0 && inner.height > 0) {
        ui_damage_add(&editor.canvas.dirty, inner);
    }
}


void editor_canvas_dirty_lines(size_t first, size_t last) {

    const editor_canvas_t* canvas = &editor.canvas;

    if (last < canvas->top) {
        return;
    }


    const size_t visible = editor_canvas_rows() + 1;

    if (first >= canvas->top + visible) {
        return;
    }


    const ui_rect_t inner = editor_canvas_inner();
    const ui_rect_t text  = editor_canvas_text_rect();

    const int bottom = inner.y + inner.height;

    const int y0 = first <= canvas->top ? inner.y : text.y + (int)(first - canvas->top) * canvas->line;

    int y1 = last >= canvas->top + visible ? bottom : text.y + (int)(last - canvas->top + 1) * canvas->line;

    if (y1 > bottom) {
        y1 = bottom;
    }

    if (y1 <= y0 || inner.width <= 0) {
        return;
    }


    ui_rect_t rect = {inner.x, y0, inner.width, y1 - y0};

    ui_damage_add(&editor.canvas.dirty, rect);
}


void editor_canvas_dirty_scrollbar(void) {

    const ui_rect_t inner = editor_canvas_inner();

    ui_rect_t rect = {inner.x + inner.width - EDITOR_SCROLLBAR_WIDTH, inner.y, EDITOR_SCROLLBAR_WIDTH, inner.height};

    if (rect.width > 0 && rect.height > 0) {
        ui_damage_add(&editor.canvas.dirty, rect);
    }
}


/**
 * @brief Scrolls to a line and a column, repainting everything when either moves.
 * @param top The first visible line, clamped to the scroll range.
 * @param left The first visible column.
 */
static void editor_canvas_scroll_to(size_t top, size_t left) {

    editor_canvas_t* canvas = &editor.canvas;

    const size_t max = editor_canvas_max_top();

    if (top > max) {
        top = max;
    }

    if (top == canvas->top && left == canvas->left) {
        return;
    }

    canvas->top  = top;
    canvas->left = left;

    editor_canvas_dirty_all();
}


void editor_canvas_sync(void) {

    editor_canvas_t* canvas = &editor.canvas;

    const size_t digits = editor_canvas_count_digits();

    if (digits != canvas->digits) {

        canvas->digits = digits;

        editor_canvas_dirty_all();
    }

    editor_canvas_scroll_to(canvas->top, canvas->left);
    editor_canvas_dirty_scrollbar();
}


void editor_canvas_place(ui_rect_t area) {

    editor.canvas.area = area;

    editor_canvas_scroll_to(editor.canvas.top, editor.canvas.left);
    editor_canvas_dirty_all();
}


void editor_canvas_home(void) {

    editor.canvas.top    = 0;
    editor.canvas.left   = 0;
    editor.canvas.digits = editor_canvas_count_digits();

    editor_canvas_dirty_all();
}


bool editor_canvas_contains(int x, int y) {

    const ui_rect_t inner = editor_canvas_inner();

    return x >= inner.x && y >= inner.y && x < inner.x + inner.width && y < inner.y + inner.height;
}


bool editor_canvas_scrollbar(ui_rect_t* track, ui_rect_t* thumb) {

    const size_t max = editor_canvas_max_top();

    if (max == 0) {
        return false;
    }


    const ui_rect_t inner = editor_canvas_inner();

    ui_rect_t t = {inner.x + inner.width - EDITOR_SCROLLBAR_WIDTH, inner.y + EDITOR_SCROLLBAR_INSET, EDITOR_SCROLLBAR_WIDTH, inner.height - EDITOR_SCROLLBAR_INSET * 2};

    if (t.height <= 0) {
        return false;
    }

    if (track) {
        *track = t;
    }


    if (thumb) {

        long long height = (long long)t.height * (long long)editor_canvas_rows() / (long long)editor.buffer.count;

        if (height < EDITOR_SCROLLBAR_MIN) {
            height = EDITOR_SCROLLBAR_MIN;
        }

        if (height > t.height) {
            height = t.height;
        }


        const long long travel = t.height - height;

        ui_rect_t h = {

            .x      = t.x + (EDITOR_SCROLLBAR_WIDTH - EDITOR_SCROLLBAR_THUMB) / 2,
            .y      = t.y + (int)(travel * (long long)editor.canvas.top / (long long)max),
            .width  = EDITOR_SCROLLBAR_THUMB,
            .height = (int)height,
        };

        *thumb = h;
    }

    return true;
}


void editor_canvas_drag_thumb(int y, int grab) {

    ui_rect_t track;
    ui_rect_t thumb;

    if (!editor_canvas_scrollbar(&track, &thumb)) {
        return;
    }


    const long long travel = track.height - thumb.height;

    if (travel <= 0) {
        return;
    }


    long long offset = y - grab - track.y;

    if (offset < 0) {
        offset = 0;
    }

    if (offset > travel) {
        offset = travel;
    }

    editor_canvas_scroll_to((size_t)((offset * (long long)editor_canvas_max_top() + travel / 2) / travel), editor.canvas.left);
}


void editor_canvas_set_thumb_hot(bool hot) {

    if (editor.canvas.thumb_hot == hot) {
        return;
    }

    editor.canvas.thumb_hot = hot;

    editor_canvas_dirty_scrollbar();
}


void editor_canvas_scroll_rows(long delta) {

    long top = (long)editor.canvas.top + delta;

    if (top < 0) {
        top = 0;
    }

    editor_canvas_scroll_to((size_t)top, editor.canvas.left);
}


void editor_canvas_scroll_columns(long delta) {

    const editor_canvas_t* canvas = &editor.canvas;

    const size_t rows    = editor_canvas_rows() + 1;
    const size_t columns = editor_canvas_columns();

    size_t widest = 0;

    for (size_t i = canvas->top; i < canvas->top + rows && i < editor.buffer.count; i++) {

        const size_t width = editor_text_column(editor.buffer.lines[i].text, editor.buffer.lines[i].length, editor.buffer.lines[i].length);

        if (width > widest) {
            widest = width;
        }
    }


    size_t limit = widest > columns / 2 ? widest - columns / 2 : 0;

    if (limit < canvas->left) {
        limit = canvas->left;
    }

    long left = (long)canvas->left + delta;

    if (left < 0) {
        left = 0;
    }

    if ((size_t)left > limit) {
        left = (long)limit;
    }

    editor_canvas_scroll_to(canvas->top, (size_t)left);
}


void editor_canvas_reveal(bool center) {

    const editor_canvas_t* canvas = &editor.canvas;

    const size_t rows    = editor_canvas_rows();
    const size_t columns = editor_canvas_columns();

    const size_t line = editor.caret.line;

    size_t top = canvas->top;

    if (line < top || line >= top + rows) {

        if (center) {
            top = line > rows / 2 ? line - rows / 2 : 0;
        } else if (line < top) {
            top = line;
        } else {
            top = line - rows + 1;
        }
    }


    const editor_line_t* text = &editor.buffer.lines[line];

    const size_t column = editor_text_column(text->text, text->length, editor.caret.offset);
    const size_t margin = columns > EDITOR_REVEAL_MARGIN * 4 ? EDITOR_REVEAL_MARGIN : 0;

    size_t left = canvas->left;

    if (column < left) {
        left = column > margin ? column - margin : 0;
    } else if (column >= left + columns) {
        left = column - columns + 1 + margin;
    }

    editor_canvas_scroll_to(top, left);
}


editor_pos_t editor_canvas_hit(int x, int y) {

    const editor_canvas_t* canvas = &editor.canvas;

    const ui_rect_t text = editor_canvas_text_rect();

    long line = (long)canvas->top + (long)floor((double)(y - text.y) / (double)canvas->line);

    if (line < 0) {
        line = 0;
    }

    if (line >= (long)editor.buffer.count) {
        line = (long)editor.buffer.count - 1;
    }


    long column = (long)canvas->left + (long)floor((double)(x - text.x) / (double)canvas->cell + 0.5);

    if (column < 0) {
        column = 0;
    }


    const editor_line_t* row = &editor.buffer.lines[line];

    editor_pos_t pos = {(size_t)line, editor_text_offset(row->text, row->length, (size_t)column)};

    return pos;
}


void editor_canvas_wake(void) {

    editor_canvas_t* canvas = &editor.canvas;

    if (!canvas->caret_on) {
        editor_canvas_dirty_lines(editor.caret.line, editor.caret.line);
    }

    canvas->caret_on = true;
    canvas->caret_at = ui_anim_now();
}


int editor_canvas_tick(uint64_t now) {

    editor_canvas_t* canvas = &editor.canvas;

    if (!editor_canvas_focused()) {
        return -1;
    }


    uint64_t elapsed = now > canvas->caret_at ? now - canvas->caret_at : 0;

    if (elapsed >= EDITOR_BLINK_MS) {

        canvas->caret_on = !canvas->caret_on;
        canvas->caret_at = now;

        elapsed = 0;

        editor_canvas_dirty_lines(editor.caret.line, editor.caret.line);
    }

    return (int)(EDITOR_BLINK_MS - elapsed);
}


/**
 * @brief Walks a line from a known column to a later offset.
 * @param line The line.
 * @param offset Where the walk starts.
 * @param column The column at that offset.
 * @param to Where the walk stops.
 * @return The column at that offset.
 */
static size_t editor_canvas_advance(const editor_line_t* line, size_t offset, size_t column, size_t to) {

    while (offset < to && offset < line->length) {

        uint32_t codepoint;

        offset += editor_utf8_decode(line->text + offset, line->length - offset, &codepoint);

        column = codepoint == '\t' ? (column / EDITOR_TAB_WIDTH + 1) * EDITOR_TAB_WIDTH : column + 1;
    }

    return column;
}


/**
 * @brief Fills the cells of a line between two columns, as much of them as is scrolled into view.
 * @param cr The context.
 * @param from The first column.
 * @param to The column past the last one.
 * @param y The top of the line.
 * @param color The colour to fill with.
 */
static void editor_canvas_fill_columns(cairo_t* cr, size_t from, size_t to, int y, ui_color_t color) {

    const editor_canvas_t* canvas = &editor.canvas;

    if (to <= from || to <= canvas->left) {
        return;
    }


    const ui_rect_t text = editor_canvas_text_rect();

    const size_t start = from > canvas->left ? from - canvas->left : 0;
    const size_t stop  = to - canvas->left;

    cairo_rectangle(cr, text.x + (double)start * canvas->cell, y, (double)(stop - start) * canvas->cell, canvas->line);
    editor_canvas_color(cr, color);
    cairo_fill(cr);
}


/**
 * @brief Washes every match of the search query on a line.
 * @param cr The context.
 * @param line The line.
 * @param y The top of the line.
 */
static void editor_canvas_draw_matches(cairo_t* cr, const editor_line_t* line, int y) {

    const size_t length = strlen(editor.query);
    const size_t limit  = editor.canvas.left + editor_canvas_columns() + 1;

    size_t offset = 0;
    size_t column = 0;
    size_t found;

    while (editor_text_find(line->text, line->length, offset, editor.query, length, &found)) {

        const size_t start = editor_canvas_advance(line, offset, column, found);
        const size_t stop  = editor_canvas_advance(line, found, start, found + length);

        if (start > limit) {
            break;
        }

        editor_canvas_fill_columns(cr, start, stop, y, editor_color_match);

        offset = found + length;
        column = stop;
    }
}


/**
 * @brief Draws the characters of a line that are scrolled into view.
 * @param cr The context.
 * @param line The line.
 * @param baseline The baseline to draw on.
 */
static void editor_canvas_draw_text(cairo_t* cr, const editor_line_t* line, int baseline) {

    editor_canvas_t* canvas = &editor.canvas;

    const size_t columns = editor_canvas_columns() + 1;

    if (canvas->run_capacity < columns) {

        cairo_glyph_t* run = (cairo_glyph_t*)realloc(canvas->run, columns * sizeof(cairo_glyph_t));

        if (!run) {
            return;
        }

        canvas->run          = run;
        canvas->run_capacity = columns;
    }


    const ui_rect_t text = editor_canvas_text_rect();

    size_t count  = 0;
    size_t column = 0;

    for (size_t at = 0; at < line->length && column < canvas->left + columns;) {

        uint32_t codepoint;

        at += editor_utf8_decode(line->text + at, line->length - at, &codepoint);

        const size_t next = codepoint == '\t' ? (column / EDITOR_TAB_WIDTH + 1) * EDITOR_TAB_WIDTH : column + 1;

        if (column >= canvas->left && codepoint != '\t' && codepoint != ' ' && count < canvas->run_capacity) {

            const bool control = codepoint < 0x20 || codepoint == 0x7F || codepoint == EDITOR_UTF8_INVALID;

            canvas->run[count].index = control ? canvas->replacement : editor_canvas_glyph(codepoint);
            canvas->run[count].x     = text.x + (double)(column - canvas->left) * canvas->cell;
            canvas->run[count].y     = baseline;

            count++;
        }

        column = next;
    }


    if (count > 0) {

        cairo_set_scaled_font(cr, canvas->font);
        editor_canvas_color(cr, ui_theme()->text);

        cairo_show_glyphs(cr, canvas->run, (int)count);
    }
}


/**
 * @brief Draws a line's number, right aligned in the gutter.
 * @param cr The context.
 * @param index The line.
 * @param y The top of the line.
 */
static void editor_canvas_draw_number(cairo_t* cr, size_t index, int y) {

    const editor_canvas_t* canvas = &editor.canvas;

    char digits[24];

    const int length = snprintf(digits, sizeof(digits), "%zu", index + 1);

    if (length <= 0) {
        return;
    }


    const int right = editor_canvas_inner().x + editor_canvas_gutter() - EDITOR_GUTTER_PAD;

    cairo_glyph_t glyphs[sizeof(digits)];

    for (int i = 0; i < length; i++) {

        glyphs[i].index = editor_canvas_glyph((unsigned char)digits[i]);
        glyphs[i].x     = right - (length - i) * canvas->cell;
        glyphs[i].y     = y + canvas->baseline;
    }

    cairo_set_scaled_font(cr, canvas->font);
    editor_canvas_color(cr, index == editor.caret.line ? ui_theme()->text : ui_theme()->text_muted);

    cairo_show_glyphs(cr, glyphs, length);
}


/**
 * @brief Draws one line: its washes, its text, the caret when it stands on it, and its number.
 * @param cr The context.
 * @param index The line.
 * @param y The top of the line.
 * @param from The start of the selection, or NULL when nothing is selected.
 * @param to The end of the selection.
 */
static void editor_canvas_draw_line(cairo_t* cr, size_t index, int y, const editor_pos_t* from, const editor_pos_t* to) {

    const ui_theme_t* theme       = ui_theme();
    const editor_canvas_t* canvas = &editor.canvas;
    const editor_line_t* line     = &editor.buffer.lines[index];

    const ui_rect_t inner = editor_canvas_inner();
    const ui_rect_t text  = editor_canvas_text_rect();

    const int gutter = inner.x + editor_canvas_gutter() + 1;
    const int right  = inner.x + inner.width - EDITOR_SCROLLBAR_WIDTH;

    const bool current = index == editor.caret.line;


    if (current && !from && editor.active) {

        cairo_rectangle(cr, gutter, y, right - gutter, canvas->line);
        editor_canvas_color(cr, editor_canvas_alpha(theme->text, EDITOR_CURRENT_LINE_ALPHA));
        cairo_fill(cr);
    }


    cairo_save(cr);

    cairo_rectangle(cr, gutter, y, right - gutter, canvas->line);
    cairo_clip(cr);

    if (editor.prompt == EDITOR_PROMPT_FIND && editor.query[0]) {
        editor_canvas_draw_matches(cr, line, y);
    }

    if (from && index >= from->line && index <= to->line) {

        const size_t start = index == from->line ? editor_text_column(line->text, line->length, from->offset) : 0;
        const size_t stop  = index == to->line ? editor_text_column(line->text, line->length, to->offset) : editor_text_column(line->text, line->length, line->length) + 1;

        editor_canvas_fill_columns(cr, start, stop, y, editor.active ? editor_canvas_alpha(theme->primary, EDITOR_SELECTION_ALPHA) : editor_color_idle);
    }

    editor_canvas_draw_text(cr, line, y + canvas->baseline);


    if (current && canvas->caret_on && editor_canvas_focused()) {

        const size_t column = editor_text_column(line->text, line->length, editor.caret.offset);

        if (column >= canvas->left) {

            const double x = text.x + (double)(column - canvas->left) * canvas->cell - EDITOR_CARET_WIDTH / 2.0;

            ui_draw_rounded_rect_d(cr, x, y + 1, EDITOR_CARET_WIDTH, canvas->line - 2, EDITOR_CARET_WIDTH / 2.0);
            editor_canvas_color(cr, theme->text);
            cairo_fill(cr);
        }
    }

    cairo_restore(cr);


    editor_canvas_draw_number(cr, index, y);
}


/**
 * @brief Draws the scrollbar thumb, when there is anything to scroll.
 * @param cr The context.
 */
static void editor_canvas_draw_scrollbar(cairo_t* cr) {

    ui_rect_t thumb;

    if (!editor_canvas_scrollbar(NULL, &thumb)) {
        return;
    }

    ui_draw_rounded_rect(cr, thumb, thumb.width / 2.0);
    editor_canvas_color(cr, editor_canvas_alpha(ui_theme()->text, editor.canvas.thumb_hot ? EDITOR_THUMB_HOT_ALPHA : EDITOR_THUMB_ALPHA));
    cairo_fill(cr);
}


/**
 * @brief Draws the lines between two heights, and everything around them, inside a context already clipped to what is dirty.
 * @param cr The context.
 * @param y0 The top of the dirty region.
 * @param y1 The bottom of the dirty region.
 */
static void editor_canvas_draw(cairo_t* cr, int y0, int y1) {

    const ui_theme_t* theme       = ui_theme();
    const editor_canvas_t* canvas = &editor.canvas;

    const ui_rect_t inner = editor_canvas_inner();
    const ui_rect_t text  = editor_canvas_text_rect();


    editor_canvas_color(cr, theme->surface_sunken);
    cairo_paint(cr);

    cairo_rectangle(cr, inner.x + editor_canvas_gutter(), inner.y, 1, inner.height);
    editor_canvas_color(cr, theme->border);
    cairo_fill(cr);


    editor_pos_t from;
    editor_pos_t to;

    const bool selected = editor_selection(&from, &to);

    const size_t first = y0 <= text.y ? 0 : (size_t)((y0 - text.y) / canvas->line);
    const size_t last  = y1 <= text.y ? 0 : (size_t)((y1 - 1 - text.y) / canvas->line);

    for (size_t row = first; row <= last; row++) {

        const size_t index = canvas->top + row;

        const int y = text.y + (int)row * canvas->line;

        if (index >= editor.buffer.count || y >= inner.y + inner.height) {
            break;
        }

        editor_canvas_draw_line(cr, index, y, selected ? &from : NULL, &to);
    }

    editor_canvas_draw_scrollbar(cr);
}


void editor_canvas_paint(void) {

    editor_canvas_t* canvas = &editor.canvas;

    if (canvas->dirty.count == 0) {
        return;
    }

    ui_damage_clip(&canvas->dirty, ui_window_width(editor.window), ui_window_height(editor.window));


    const ui_damage_t dirty = canvas->dirty;

    ui_damage_reset(&canvas->dirty);

    const ui_rect_t inner = editor_canvas_inner();

    if (dirty.count == 0 || inner.width <= 0 || inner.height <= 0) {
        return;
    }


    const cairo_format_t format = ui_window_translucent(editor.window) ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;

    cairo_surface_t* surface = cairo_image_surface_create_for_data((unsigned char*)ui_window_pixels(editor.window), format, ui_window_width(editor.window), ui_window_height(editor.window), (int)ui_window_stride(editor.window));

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return;
    }


    cairo_t* cr = cairo_create(surface);

    const double radius = ui_theme()->corner_radius - EDITOR_FRAME_INSET;

    ui_draw_rounded_rect(cr, inner, radius > 0.0 ? radius : 0.0);
    cairo_clip(cr);


    const int right = inner.x + inner.width - EDITOR_SCROLLBAR_WIDTH;

    int y0 = inner.y + inner.height;
    int y1 = inner.y;

    for (size_t i = 0; i < dirty.count; i++) {

        cairo_rectangle(cr, dirty.rects[i].x, dirty.rects[i].y, dirty.rects[i].width, dirty.rects[i].height);

        if (dirty.rects[i].x >= right) {
            continue;
        }

        if (dirty.rects[i].y < y0) {
            y0 = dirty.rects[i].y;
        }

        if (dirty.rects[i].y + dirty.rects[i].height > y1) {
            y1 = dirty.rects[i].y + dirty.rects[i].height;
        }
    }

    cairo_clip(cr);

    editor_canvas_draw(cr, y0, y1);

    cairo_destroy(cr);

    cairo_surface_flush(surface);
    cairo_surface_destroy(surface);


    for (size_t i = 0; i < dirty.count; i++) {
        ui_window_damage(editor.window, dirty.rects[i].x, dirty.rects[i].y, dirty.rects[i].width, dirty.rects[i].height);
    }
}
