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
 * @brief A text editor: a toolbar, a prompt bar and a status line of widgets around a text area drawn by hand.
 */

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <aplus/input.h>
#include <aplus/ui-anim.h>
#include <aplus/ui-draw.h>
#include <aplus/ui-keymap.h>
#include <aplus/ui-widgets.h>
#include <aplus/ui.h>
#include <aplus/ulog.h>

#include "editor.h"


#define EDITOR_VERSION "0.1"

#define EDITOR_WINDOW_WIDTH  760
#define EDITOR_WINDOW_HEIGHT 540

#define EDITOR_MARGIN 12
#define EDITOR_GAP    8

#define EDITOR_TOOLBAR_HEIGHT 36
#define EDITOR_PROMPT_HEIGHT  36
#define EDITOR_STATUS_HEIGHT  20

/**
 * @brief The padding either side of a button's text, the narrowest a button gets, and the gap between two.
 */
#define EDITOR_BUTTON_PAD 14
#define EDITOR_BUTTON_MIN 56
#define EDITOR_BUTTON_GAP 4

/**
 * @brief The side of a toolbar button, and the size its icon is drawn at.
 */
#define EDITOR_TOOL_SIZE 28
#define EDITOR_TOOL_ICON 20

/**
 * @brief How far a label's text sits inside it.
 */
#define EDITOR_LABEL_PAD 8

/**
 * @brief How close together presses have to be, in milliseconds and in pixels, to count as a double or triple click.
 */
#define EDITOR_DOUBLE_CLICK_MS   400
#define EDITOR_DOUBLE_CLICK_SLOP 4

/**
 * @brief How often a selection dragged past the edge of the text scrolls it by a line.
 */
#define EDITOR_AUTOSCROLL_MS 40

/**
 * @brief How far one wheel detent scrolls, down and sideways.
 */
#define EDITOR_WHEEL_ROWS    3
#define EDITOR_WHEEL_COLUMNS 6

/**
 * @brief How many queued events one pass folds into a frame.
 */
#define EDITOR_DRAIN_MAX 256

#define EDITOR_UNTITLED "Untitled"

/**
 * @brief The modifier keys the editor follows, one bit per key.
 */
#define EDITOR_MOD_LEFTCTRL   (1 << 0)
#define EDITOR_MOD_RIGHTCTRL  (1 << 1)
#define EDITOR_MOD_LEFTSHIFT  (1 << 2)
#define EDITOR_MOD_RIGHTSHIFT (1 << 3)

#define EDITOR_MOD_CTRL  (EDITOR_MOD_LEFTCTRL | EDITOR_MOD_RIGHTCTRL)
#define EDITOR_MOD_SHIFT (EDITOR_MOD_LEFTSHIFT | EDITOR_MOD_RIGHTSHIFT)


/**
 * @brief The toolbar, laid out from the right edge in this order, each button an icon out of the theme.
 */
static const struct {

    editor_action_t action;

    const char* icon;

} editor_actions[] = {

    {EDITOR_ACTION_NEW,     "document-new"    },
    {EDITOR_ACTION_OPEN,    "document-open"   },
    {EDITOR_ACTION_SAVE,    "document-save"   },
    {EDITOR_ACTION_SAVE_AS, "document-save-as"},
    {EDITOR_ACTION_FIND,    "edit-find"       },
};


/**
 * @brief What each prompt shows: its label, its entry's placeholder when it has an entry, and its buttons.
 */
static const struct {

    const char* label;
    const char* placeholder;

    const char* buttons[EDITOR_PROMPT_BUTTONS];
    ui_button_style_t styles[EDITOR_PROMPT_BUTTONS];

} editor_prompts[EDITOR_PROMPT_COUNT] = {

    [EDITOR_PROMPT_FIND]    = {"Find",       "Text to look for",         {"Previous", "Next", "Close"}, {UI_BUTTON_STYLE_DEFAULT, UI_BUTTON_STYLE_PRIMARY, UI_BUTTON_STYLE_DEFAULT}},
    [EDITOR_PROMPT_OPEN]    = {"Open",       "Path of the file to open", {"Open", "Cancel", NULL},      {UI_BUTTON_STYLE_PRIMARY, UI_BUTTON_STYLE_DEFAULT, UI_BUTTON_STYLE_DEFAULT}},
    [EDITOR_PROMPT_SAVE_AS] = {"Save as",    "Path to save the file to", {"Save", "Cancel", NULL},      {UI_BUTTON_STYLE_PRIMARY, UI_BUTTON_STYLE_DEFAULT, UI_BUTTON_STYLE_DEFAULT}},
    [EDITOR_PROMPT_GOTO]    = {"Go to line", "Line, or line:column",     {"Go", "Cancel", NULL},        {UI_BUTTON_STYLE_PRIMARY, UI_BUTTON_STYLE_DEFAULT, UI_BUTTON_STYLE_DEFAULT}},
    [EDITOR_PROMPT_CONFIRM] = {NULL,         NULL,                       {"Save", "Discard", "Cancel"}, {UI_BUTTON_STYLE_PRIMARY, UI_BUTTON_STYLE_DANGER, UI_BUTTON_STYLE_DEFAULT} },
};


/**
 * @brief The state an event is handled against, compared afterwards to see what has to be redrawn.
 */
typedef struct {

    editor_pos_t caret;
    editor_pos_t anchor;

    size_t count;
    unsigned long edits;

    bool modified;
    bool focused;

} editor_snapshot_t;


editor_t editor;


static void editor_layout(ui_view_t* view, int width, int height, void* user);
static void editor_prompt_show(editor_prompt_t prompt, const char* text);
static void editor_prompt_close(void);
static void editor_on_prompt_change(ui_widget_t* widget, void* user);


bool editor_selection(editor_pos_t* from, editor_pos_t* to) {

    const int order = editor_pos_compare(editor.caret, editor.anchor);

    *from = order <= 0 ? editor.caret : editor.anchor;
    *to   = order <= 0 ? editor.anchor : editor.caret;

    return order != 0;
}


/**
 * @brief Reports a line of the document.
 * @param index The line.
 * @return The line.
 */
static const editor_line_t* editor_line(size_t index) {

    return &editor.buffer.lines[index];
}


/**
 * @brief Reports the column a position is drawn at.
 * @param pos The position.
 * @return The column.
 */
static size_t editor_column(editor_pos_t pos) {

    const editor_line_t* line = editor_line(pos.line);

    return editor_text_column(line->text, line->length, pos.offset);
}


/**
 * @brief Holds a position inside the document.
 * @param pos The position.
 * @return The nearest position that exists.
 */
static editor_pos_t editor_clamp(editor_pos_t pos) {

    if (pos.line >= editor.buffer.count) {
        return editor_buffer_end(&editor.buffer);
    }

    if (pos.offset > editor_line(pos.line)->length) {
        pos.offset = editor_line(pos.line)->length;
    }

    return pos;
}


/**
 * @brief Reports the position one character back, the end of the previous line from the start of one.
 * @param pos The position.
 * @return The previous position, or the same one at the start of the document.
 */
static editor_pos_t editor_prev_pos(editor_pos_t pos) {

    if (pos.offset > 0) {

        const editor_line_t* line = editor_line(pos.line);

        pos.offset = editor_text_prev(line->text, line->length, pos.offset);

    } else if (pos.line > 0) {

        pos.line--;
        pos.offset = editor_line(pos.line)->length;
    }

    return pos;
}


/**
 * @brief Reports the position one character on, the start of the next line from the end of one.
 * @param pos The position.
 * @return The next position, or the same one at the end of the document.
 */
static editor_pos_t editor_next_pos(editor_pos_t pos) {

    const editor_line_t* line = editor_line(pos.line);

    if (pos.offset < line->length) {

        pos.offset = editor_text_next(line->text, line->length, pos.offset);

    } else if (pos.line + 1 < editor.buffer.count) {

        pos.line++;
        pos.offset = 0;
    }

    return pos;
}


/**
 * @brief Sorts a character into blanks, word characters and punctuation, which is what word movement steps between.
 * @param codepoint The character.
 * @return 0 for a blank, 1 for a word character, 2 for anything else.
 */
static int editor_class(uint32_t codepoint) {

    if (codepoint == ' ' || codepoint == '\t') {
        return 0;
    }

    if (codepoint == '_' || codepoint >= 0x80 || (codepoint >= '0' && codepoint <= '9') || (codepoint >= 'a' && codepoint <= 'z') || (codepoint >= 'A' && codepoint <= 'Z')) {
        return 1;
    }

    return 2;
}


/**
 * @brief Reports the class of the character at an offset of a line.
 * @param line The line.
 * @param offset The offset, before the end of the line.
 * @return The class.
 */
static int editor_class_at(const editor_line_t* line, size_t offset) {

    uint32_t codepoint;

    editor_utf8_decode(line->text + offset, line->length - offset, &codepoint);

    return editor_class(codepoint);
}


/**
 * @brief Reports where Ctrl+Right lands: past the blanks, then past the run of characters after them.
 * @param pos Where to start.
 * @return The position.
 */
static editor_pos_t editor_word_right(editor_pos_t pos) {

    const editor_line_t* line = editor_line(pos.line);

    if (pos.offset >= line->length) {
        return editor_next_pos(pos);
    }


    size_t at = pos.offset;

    while (at < line->length && editor_class_at(line, at) == 0) {
        at = editor_text_next(line->text, line->length, at);
    }

    if (at < line->length) {

        const int class = editor_class_at(line, at);

        while (at < line->length && editor_class_at(line, at) == class) {
            at = editor_text_next(line->text, line->length, at);
        }
    }

    pos.offset = at;

    return pos;
}


/**
 * @brief Reports where Ctrl+Left lands: back over the blanks, then back to the start of the run before them.
 * @param pos Where to start.
 * @return The position.
 */
static editor_pos_t editor_word_left(editor_pos_t pos) {

    if (pos.offset == 0) {
        return editor_prev_pos(pos);
    }


    const editor_line_t* line = editor_line(pos.line);

    size_t at = pos.offset;

    while (at > 0 && editor_class_at(line, editor_text_prev(line->text, line->length, at)) == 0) {
        at = editor_text_prev(line->text, line->length, at);
    }

    if (at > 0) {

        const int class = editor_class_at(line, editor_text_prev(line->text, line->length, at));

        while (at > 0 && editor_class_at(line, editor_text_prev(line->text, line->length, at)) == class) {
            at = editor_text_prev(line->text, line->length, at);
        }
    }

    pos.offset = at;

    return pos;
}


/**
 * @brief Reports the run of same-class characters around a position, which is what a double click selects.
 * @param pos The position.
 * @param from Receives where the run starts.
 * @param to Receives where it ends.
 */
static void editor_word_at(editor_pos_t pos, editor_pos_t* from, editor_pos_t* to) {

    const editor_line_t* line = editor_line(pos.line);

    *from = pos;
    *to   = pos;

    if (line->length == 0) {
        return;
    }


    const size_t at = pos.offset < line->length ? pos.offset : editor_text_prev(line->text, line->length, line->length);

    const int class = editor_class_at(line, at);

    size_t start = at;
    size_t end   = at;

    while (start > 0 && editor_class_at(line, editor_text_prev(line->text, line->length, start)) == class) {
        start = editor_text_prev(line->text, line->length, start);
    }

    while (end < line->length && editor_class_at(line, end) == class) {
        end = editor_text_next(line->text, line->length, end);
    }

    from->offset = start;
    to->offset   = end;
}


/**
 * @brief Reports the name the document goes by.
 * @return The file name, or EDITOR_UNTITLED.
 */
static const char* editor_basename(void) {

    if (!editor.path[0]) {
        return EDITOR_UNTITLED;
    }

    const char* slash = strrchr(editor.path, '/');

    return slash && slash[1] ? slash + 1 : editor.path;
}


/**
 * @brief Shows the status message, cut short to fit its label.
 */
static void editor_show_status(void) {

    const ui_theme_t* theme = ui_theme();

    const int width = ui_widget_rect(editor.message).width;

    char text[sizeof(editor.status)];

    ui_draw_ellipsize(text, sizeof(text), editor.status, theme->font_regular, theme->font_size, width > 0 ? width : 0);

    ui_label_set_text(editor.message, text);
}


/**
 * @brief Sets the status message.
 * @param format A printf(3) format.
 */
static void editor_message(const char* format, ...) __attribute__((format(printf, 1, 2)));

static void editor_message(const char* format, ...) {

    va_list args;

    va_start(args, format);
    vsnprintf(editor.status, sizeof(editor.status), format, args);
    va_end(args);

    editor_show_status();
}


/**
 * @brief Puts the document's name and whether it has unsaved changes in the title and the toolbar.
 */
static void editor_update_title(void) {

    const bool modified = editor_history_modified(&editor.history);

    char title[UI_TITLE_MAX];

    snprintf(title, sizeof(title), "%s%.40s - %s", modified ? "*" : "", editor_basename(), "Text Editor");

    if (strcmp(title, editor.title) != 0) {

        memcpy(editor.title, title, sizeof(title));

        ui_window_set_title(editor.window, title);
    }


    const ui_theme_t* theme = ui_theme();

    const int width = ui_widget_rect(editor.name).width - EDITOR_LABEL_PAD * 2;

    char name[PATH_MAX];
    char shown[128];

    snprintf(name, sizeof(name), "%s%s", editor_basename(), modified ? "  \xE2\x80\xA2" : "");

    ui_draw_ellipsize(shown, sizeof(shown), name, theme->font_medium, theme->font_size, width > 0 ? width : 0);
    ui_label_set_text(editor.name, shown);
}


/**
 * @brief Counts the characters between two positions, a line break counting as one.
 * @param from The first position.
 * @param to The position past the last one.
 * @return The count.
 */
static size_t editor_count_chars(editor_pos_t from, editor_pos_t to) {

    size_t count = 0;

    for (size_t i = from.line; i <= to.line; i++) {

        const editor_line_t* line = editor_line(i);

        const size_t start = i == from.line ? from.offset : 0;
        const size_t stop  = i == to.line ? to.offset : line->length;

        for (size_t at = start; at < stop; at = editor_text_next(line->text, line->length, at)) {
            count++;
        }

        if (i < to.line) {
            count++;
        }
    }

    return count;
}


/**
 * @brief Puts the caret position, the selection size and the line count in the status line.
 */
static void editor_update_position(void) {

    editor_pos_t from;
    editor_pos_t to;

    const size_t line   = editor.caret.line + 1;
    const size_t column = editor_column(editor.caret) + 1;

    const char* ending = editor.buffer.crlf ? "CRLF" : "LF";

    char text[128];

    if (editor_selection(&from, &to)) {
        snprintf(text, sizeof(text), "%zu selected    Ln %zu, Col %zu    %s", editor_count_chars(from, to), line, column, ending);
    } else {
        snprintf(text, sizeof(text), "Ln %zu, Col %zu    %zu %s    %s", line, column, editor.buffer.count, editor.buffer.count == 1 ? "line" : "lines", ending);
    }

    ui_label_set_text(editor.position, text);
}


/**
 * @brief Records what an event is about to be handled against.
 * @return The snapshot.
 */
static editor_snapshot_t editor_snapshot(void) {

    editor_snapshot_t snapshot = {

        .caret    = editor.caret,
        .anchor   = editor.anchor,
        .count    = editor.buffer.count,
        .edits    = editor.edits,
        .modified = editor_history_modified(&editor.history),
        .focused  = editor_canvas_focused(),
    };

    return snapshot;
}


/**
 * @brief Brings the text area, the status line and the title up to date with whatever an event changed.
 * @param before The snapshot taken before the event.
 */
static void editor_refresh(const editor_snapshot_t* before) {

    const bool moved   = editor_pos_compare(before->caret, editor.caret) != 0 || editor_pos_compare(before->anchor, editor.anchor) != 0;
    const bool focused = editor_canvas_focused();

    if (moved || focused != before->focused) {

        size_t first = before->caret.line;
        size_t last  = before->caret.line;

        const size_t lines[] = {before->anchor.line, editor.caret.line, editor.anchor.line};

        for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {

            first = lines[i] < first ? lines[i] : first;
            last  = lines[i] > last ? lines[i] : last;
        }

        editor_canvas_dirty_lines(first, last);
    }

    if (moved || before->edits != editor.edits) {
        editor_canvas_wake();
    }

    if (before->count != editor.buffer.count) {
        editor_canvas_sync();
    }


    if (!editor.keep_goal) {
        editor.goal = editor_column(editor.caret);
    }

    editor.keep_goal = false;


    editor_update_position();

    if (before->modified != editor_history_modified(&editor.history)) {
        editor_update_title();
    }
}


/**
 * @brief Reports that a change could not be made for want of memory.
 */
static void editor_out_of_memory(void) {

    editor_message("Out of memory: the last change was not made");
}


/**
 * @brief Inserts text and records it, leaving the caret after it.
 * @param at Where to insert it.
 * @param text The text.
 * @param size Its size in bytes.
 * @param merge The run of keystrokes it may be folded into.
 * @return true when it was inserted.
 */
static bool editor_insert(editor_pos_t at, const char* text, size_t size, editor_merge_t merge) {

    const editor_pos_t caret  = editor.caret;
    const editor_pos_t anchor = editor.anchor;

    editor_pos_t end;

    if (editor_buffer_insert(&editor.buffer, at, text, size, &end) < 0) {
        editor_out_of_memory();
        return false;
    }

    if (editor_history_record(&editor.history, EDITOR_EDIT_INSERT, merge, at, text, size, caret, anchor) < 0) {
        editor_history_forget(&editor.history);
    }

    editor_canvas_dirty_lines(at.line, end.line == at.line ? at.line : SIZE_MAX);

    editor.caret  = end;
    editor.anchor = end;

    editor.edits++;

    return true;
}


/**
 * @brief Erases the text between two positions and records it, leaving the caret where it was.
 * @param from The first position erased.
 * @param to The position past the last one.
 * @param merge The run of keystrokes it may be folded into.
 * @return true when anything was erased.
 */
static bool editor_erase(editor_pos_t from, editor_pos_t to, editor_merge_t merge) {

    if (editor_pos_compare(from, to) >= 0) {
        return false;
    }


    const editor_pos_t caret  = editor.caret;
    const editor_pos_t anchor = editor.anchor;

    size_t size;

    char* text = editor_buffer_copy(&editor.buffer, from, to, &size);

    if (!text) {
        editor_out_of_memory();
        return false;
    }

    if (editor_buffer_erase(&editor.buffer, from, to) < 0) {
        free(text);
        editor_out_of_memory();
        return false;
    }

    if (editor_history_record(&editor.history, EDITOR_EDIT_ERASE, merge, from, text, size, caret, anchor) < 0) {
        editor_history_forget(&editor.history);
    }

    free(text);

    editor_canvas_dirty_lines(from.line, to.line == from.line ? from.line : SIZE_MAX);

    editor.caret  = from;
    editor.anchor = from;

    editor.edits++;

    return true;
}


/**
 * @brief Replaces the selection with text, or inserts it at the caret when nothing is selected.
 * @param text The text.
 * @param size Its size in bytes.
 * @param merge The run of keystrokes it may be folded into.
 */
static void editor_replace(const char* text, size_t size, editor_merge_t merge) {

    editor_pos_t from;
    editor_pos_t to;

    const bool selected = editor_selection(&from, &to);

    if (selected) {
        editor_history_begin(&editor.history);
        editor_erase(from, to, EDITOR_MERGE_NONE);
    }

    if (size > 0) {
        editor_insert(editor.caret, text, size, merge);
    }

    if (selected) {
        editor_history_end(&editor.history);
    }

    editor_canvas_reveal(false);
}


/**
 * @brief Moves the caret, dragging the selection along or dropping it.
 * @param pos Where to move it.
 * @param extend Whether the selection follows rather than collapsing.
 */
static void editor_move(editor_pos_t pos, bool extend) {

    editor.caret = pos;

    if (!extend) {
        editor.anchor = pos;
    }

    editor_history_seal(&editor.history);
    editor_canvas_reveal(false);
}


/**
 * @brief Moves the caret up or down by lines, keeping to the column it started from.
 * @param delta Lines to move, negative upwards.
 * @param extend Whether the selection follows.
 */
static void editor_move_vertical(long delta, bool extend) {

    const long line = (long)editor.caret.line + delta;

    editor_pos_t pos;

    if (line < 0) {

        pos.line   = 0;
        pos.offset = 0;

    } else if (line >= (long)editor.buffer.count) {

        pos = editor_buffer_end(&editor.buffer);

    } else {

        const editor_line_t* text = editor_line((size_t)line);

        pos.line   = (size_t)line;
        pos.offset = editor_text_offset(text->text, text->length, editor.goal);

        editor.keep_goal = true;
    }

    editor_move(pos, extend);
}


/**
 * @brief Moves the caret and the view a page up or down.
 * @param direction -1 for up, 1 for down.
 * @param extend Whether the selection follows.
 */
static void editor_page(long direction, bool extend) {

    const size_t rows = editor_canvas_rows();

    const long step = rows > 1 ? (long)rows - 1 : 1;

    editor_canvas_scroll_rows(direction * step);
    editor_move_vertical(direction * step, extend);
}


/**
 * @brief Moves the caret to the first non-blank of its line, or to the start of the line when it is already there.
 * @param extend Whether the selection follows.
 */
static void editor_home(bool extend) {

    const editor_line_t* line = editor_line(editor.caret.line);

    size_t first = 0;

    while (first < line->length && (line->text[first] == ' ' || line->text[first] == '\t')) {
        first++;
    }

    editor_pos_t pos = {editor.caret.line, editor.caret.offset == first ? 0 : first};

    editor_move(pos, extend);
}


/**
 * @brief Types what a key produced, dropping the control bytes a keymap also hands out.
 * @param text The bytes.
 * @param size How many there are.
 */
static void editor_type(const char* text, size_t size) {

    char clean[sizeof(editor.key_text)];

    size_t taken = 0;

    for (size_t i = 0; i < size && taken < sizeof(clean); i++) {

        const unsigned char byte = (unsigned char)text[i];

        if (byte >= 0x20 && byte != 0x7F) {
            clean[taken++] = text[i];
        }
    }

    if (taken > 0) {
        editor_replace(clean, taken, EDITOR_MERGE_TYPE);
    }
}


/**
 * @brief Breaks the line at the caret, carrying its indentation over to the new one.
 */
static void editor_newline(void) {

    editor_pos_t from;
    editor_pos_t to;

    editor_selection(&from, &to);

    const editor_line_t* line = editor_line(from.line);

    size_t indent = 0;

    while (indent < from.offset && (line->text[indent] == ' ' || line->text[indent] == '\t')) {
        indent++;
    }


    char* text = (char*)malloc(indent + 1);

    if (!text) {
        editor_out_of_memory();
        return;
    }

    text[0] = '\n';

    if (indent > 0) {
        memcpy(text + 1, line->text, indent);
    }

    editor_replace(text, indent + 1, EDITOR_MERGE_NONE);

    free(text);
}


/**
 * @brief Erases the selection, or the character or word before the caret.
 * @param word Whether to erase back to the start of a word.
 */
static void editor_backspace(bool word) {

    editor_pos_t from;
    editor_pos_t to;

    if (editor_selection(&from, &to)) {

        editor_erase(from, to, EDITOR_MERGE_NONE);

    } else {

        const editor_pos_t start = word ? editor_word_left(editor.caret) : editor_prev_pos(editor.caret);

        editor_erase(start, editor.caret, !word && start.line == editor.caret.line ? EDITOR_MERGE_BACKSPACE : EDITOR_MERGE_NONE);
    }

    editor_canvas_reveal(false);
}


/**
 * @brief Erases the selection, or the character or word after the caret.
 * @param word Whether to erase on to the end of a word.
 */
static void editor_delete(bool word) {

    editor_pos_t from;
    editor_pos_t to;

    if (editor_selection(&from, &to)) {

        editor_erase(from, to, EDITOR_MERGE_NONE);

    } else {

        const editor_pos_t end = word ? editor_word_right(editor.caret) : editor_next_pos(editor.caret);

        editor_erase(editor.caret, end, !word && end.line == editor.caret.line ? EDITOR_MERGE_DELETE : EDITOR_MERGE_NONE);
    }

    editor_canvas_reveal(false);
}


/**
 * @brief Shifts a position on a line by what was inserted or removed at the line's start.
 * @param pos The position.
 * @param line The line that changed.
 * @param delta Bytes inserted, or removed when negative.
 */
static void editor_shift(editor_pos_t* pos, size_t line, long delta) {

    if (pos->line != line) {
        return;
    }

    if (delta > 0 && pos->offset > 0) {
        pos->offset += (size_t)delta;
    }

    if (delta < 0) {
        pos->offset = pos->offset > (size_t)-delta ? pos->offset - (size_t)-delta : 0;
    }
}


/**
 * @brief Inserts a tab, or indents or unindents every line the selection touches.
 * @param outdent Whether to unindent.
 */
static void editor_indent(bool outdent) {

    editor_pos_t from;
    editor_pos_t to;

    const bool selected = editor_selection(&from, &to);

    if (!outdent && (!selected || from.line == to.line)) {
        editor_replace("\t", 1, EDITOR_MERGE_TYPE);
        return;
    }


    const size_t last = to.line > from.line && to.offset == 0 ? to.line - 1 : to.line;

    editor_pos_t caret  = editor.caret;
    editor_pos_t anchor = editor.anchor;

    editor_history_begin(&editor.history);

    for (size_t i = from.line; i <= last; i++) {

        const editor_line_t* line = editor_line(i);

        editor_pos_t start = {i, 0};

        if (outdent) {

            size_t removed = line->length > 0 && line->text[0] == '\t' ? 1 : 0;

            while (removed < line->length && removed < EDITOR_TAB_WIDTH && line->text[removed] == ' ') {
                removed++;
            }

            editor_pos_t end = {i, removed};

            if (removed > 0 && editor_erase(start, end, EDITOR_MERGE_NONE)) {
                editor_shift(&caret, i, -(long)removed);
                editor_shift(&anchor, i, -(long)removed);
            }

        } else if (line->length > 0 && editor_insert(start, "\t", 1, EDITOR_MERGE_NONE)) {

            editor_shift(&caret, i, 1);
            editor_shift(&anchor, i, 1);
        }
    }

    editor_history_end(&editor.history);
    editor_history_seal(&editor.history);

    editor.caret  = caret;
    editor.anchor = anchor;
}


/**
 * @brief Copies the selection, or the caret's whole line when nothing is selected, and erases it to cut.
 * @param cut Whether to erase what was copied.
 */
static void editor_copy(bool cut) {

    editor_pos_t from;
    editor_pos_t to;

    const bool selected = editor_selection(&from, &to);

    if (!selected) {

        from.line   = editor.caret.line;
        from.offset = 0;

        if (editor.caret.line + 1 < editor.buffer.count) {
            to.line   = editor.caret.line + 1;
            to.offset = 0;
        } else {
            to.line   = editor.caret.line;
            to.offset = editor_line(editor.caret.line)->length;
        }
    }


    size_t size;

    char* text = editor_buffer_copy(&editor.buffer, from, to, &size);

    if (!text) {
        editor_out_of_memory();
        return;
    }

    if (!selected && to.line == from.line) {

        char* line = (char*)realloc(text, size + 2);

        if (!line) {
            free(text);
            editor_out_of_memory();
            return;
        }

        text         = line;
        text[size++] = '\n';
        text[size]   = '\0';

        if (from.line > 0) {
            from.line--;
            from.offset = editor_line(from.line)->length;
        }
    }

    free(editor.clipboard);

    editor.clipboard       = text;
    editor.clipboard_size  = size;
    editor.clipboard_lines = !selected;


    if (cut) {

        editor_erase(from, to, EDITOR_MERGE_NONE);
        editor_history_seal(&editor.history);
        editor_canvas_reveal(false);
    }
}


/**
 * @brief Pastes over the selection, or above the caret's line when a whole line was copied and nothing is selected.
 */
static void editor_paste(void) {

    editor_pos_t from;
    editor_pos_t to;

    if (!editor.clipboard) {
        editor_message("Nothing to paste");
        return;
    }

    if (editor.clipboard_lines && !editor_selection(&from, &to)) {

        const editor_pos_t caret = editor.caret;

        editor_pos_t start = {caret.line, 0};

        if (editor_insert(start, editor.clipboard, editor.clipboard_size, EDITOR_MERGE_NONE)) {

            editor.caret.offset = caret.offset;
            editor.anchor       = editor.caret;
        }

        editor_canvas_reveal(false);

    } else {

        editor_replace(editor.clipboard, editor.clipboard_size, EDITOR_MERGE_NONE);
    }

    editor_history_seal(&editor.history);
}


/**
 * @brief Appends the first line of the clipboard to the prompt's entry, which takes no paste of its own.
 */
static void editor_paste_into_entry(void) {

    if (!editor.clipboard || ui_view_focused(editor.view) != editor.prompt_entry) {
        return;
    }


    char text[EDITOR_QUERY_MAX];

    snprintf(text, sizeof(text), "%s", ui_entry_text(editor.prompt_entry));

    const size_t used = strlen(text);

    size_t take = 0;

    while (take < editor.clipboard_size && editor.clipboard[take] != '\n' && editor.clipboard[take] != '\r') {
        take++;
    }

    if (take > sizeof(text) - 1 - used) {

        take = sizeof(text) - 1 - used;

        while (take > 0 && ((unsigned char)editor.clipboard[take] & 0xC0) == 0x80) {
            take--;
        }
    }

    memcpy(text + used, editor.clipboard, take);

    text[used + take] = '\0';

    ui_entry_set_text(editor.prompt_entry, text);

    editor_on_prompt_change(editor.prompt_entry, NULL);
}


/**
 * @brief Takes back the last change, or makes again the last one taken back.
 * @param redo Whether to redo rather than undo.
 */
static void editor_undo(bool redo) {

    editor_pos_t caret  = editor.caret;
    editor_pos_t anchor = editor.anchor;

    const bool done = redo ? editor_history_redo(&editor.history, &editor.buffer, &caret, &anchor) : editor_history_undo(&editor.history, &editor.buffer, &caret, &anchor);

    if (!done) {
        editor_message(redo ? "Nothing to redo" : "Nothing to undo");
        return;
    }

    editor.caret  = editor_clamp(caret);
    editor.anchor = editor_clamp(anchor);

    editor.edits++;

    editor_canvas_dirty_all();
    editor_canvas_reveal(true);
}


/**
 * @brief Selects the whole document.
 */
static void editor_select_all(void) {

    editor.anchor.line   = 0;
    editor.anchor.offset = 0;

    editor.caret = editor_buffer_end(&editor.buffer);

    editor_history_seal(&editor.history);
}


/**
 * @brief Counts the matches of the search query in the whole document.
 * @return The count.
 */
static size_t editor_count_matches(void) {

    const size_t length = strlen(editor.query);

    size_t count = 0;

    for (size_t i = 0; length > 0 && i < editor.buffer.count; i++) {

        const editor_line_t* line = editor_line(i);

        size_t at = 0;
        size_t found;

        while (editor_text_find(line->text, line->length, at, editor.query, length, &found)) {
            count++;
            at = found + length;
        }
    }

    return count;
}


/**
 * @brief Looks for the search query from a position, going round the end of the document.
 * @param origin Where to start.
 * @param forward Whether to look forwards from it, at or after it, or backwards, before it.
 * @param found Receives where the match starts.
 * @return true when there is a match anywhere.
 */
static bool editor_search(editor_pos_t origin, bool forward, editor_pos_t* found) {

    const size_t length = strlen(editor.query);
    const size_t count  = editor.buffer.count;

    for (size_t i = 0; length > 0 && i <= count; i++) {

        const size_t index = forward ? (origin.line + i) % count : (origin.line + count - i % count) % count;

        const editor_line_t* line = editor_line(index);

        size_t at;

        const bool hit =
            forward ? editor_text_find(line->text, line->length, i == 0 ? origin.offset : 0, editor.query, length, &at) : editor_text_rfind(line->text, line->length, i == 0 ? origin.offset : SIZE_MAX, editor.query, length, &at);

        if (hit) {

            found->line   = index;
            found->offset = at;

            return true;
        }
    }

    return false;
}


/**
 * @brief Reports how many matches the query has.
 */
static void editor_report_matches(void) {

    if (editor.matches == 0) {
        editor_message("No matches for \xE2\x80\x9C%s\xE2\x80\x9D", editor.query);
    } else {
        editor_message("%zu %s", editor.matches, editor.matches == 1 ? "match" : "matches");
    }
}


/**
 * @brief Selects the next or previous match of the search query.
 * @param forward Whether to look forwards.
 * @param inclusive Whether a match at the start of the selection counts, which is what typing into the query wants.
 */
static void editor_find(bool forward, bool inclusive) {

    editor_pos_t from;
    editor_pos_t to;
    editor_pos_t found;

    editor_selection(&from, &to);

    if (!editor_search(forward && !inclusive ? to : from, forward, &found)) {
        editor_report_matches();
        return;
    }

    editor.anchor = found;

    editor.caret.line   = found.line;
    editor.caret.offset = found.offset + strlen(editor.query);

    editor_history_seal(&editor.history);
    editor_canvas_reveal(true);
    editor_report_matches();
}


/**
 * @brief Searches again as the query is typed.
 * @param widget The entry.
 * @param user Unused.
 */
static void editor_on_prompt_change(ui_widget_t* widget, void* user) {

    (void)user;

    if (editor.prompt != EDITOR_PROMPT_FIND) {
        return;
    }

    snprintf(editor.query, sizeof(editor.query), "%s", ui_entry_text(widget));

    editor.matches = editor_count_matches();

    editor_canvas_dirty_all();

    if (editor.query[0]) {

        editor_find(true, true);

    } else {

        editor_pos_t from;
        editor_pos_t to;

        editor_selection(&from, &to);

        editor.caret  = from;
        editor.anchor = from;

        editor_message("%s", "");
    }
}


/**
 * @brief Opens the search bar, starting from the selected text when it fits on one line.
 */
static void editor_find_prompt(void) {

    editor_pos_t from;
    editor_pos_t to;

    if (editor_selection(&from, &to) && from.line == to.line && to.offset - from.offset < sizeof(editor.query)) {

        memcpy(editor.query, editor_line(from.line)->text + from.offset, to.offset - from.offset);

        editor.query[to.offset - from.offset] = '\0';
    }

    editor_prompt_show(EDITOR_PROMPT_FIND, editor.query);

    editor.matches = editor_count_matches();

    if (editor.query[0]) {
        editor_report_matches();
    }
}


/**
 * @brief Turns what was typed into a prompt into a path: blanks trimmed off, a leading ~ expanded.
 * @param text What was typed.
 * @param out Receives the path.
 * @param size The size of that buffer.
 * @return true when there is a path.
 */
static bool editor_expand_path(const char* text, char* out, size_t size) {

    while (*text == ' ' || *text == '\t') {
        text++;
    }

    size_t length = strlen(text);

    while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t')) {
        length--;
    }

    if (length == 0) {
        return false;
    }


    if (text[0] == '~' && (length == 1 || text[1] == '/')) {

        const char* home = getenv("HOME");

        const int written = snprintf(out, size, "%s%.*s", home ? home : "", (int)(length - 1), text + 1);

        return written > 0 && (size_t)written < size;
    }

    if (length >= size) {
        return false;
    }

    memcpy(out, text, length);

    out[length] = '\0';

    return true;
}


/**
 * @brief Reports the directory a prompt for a path starts in: the document's own, or the working directory.
 * @param out Receives it, ending with a slash.
 * @param size The size of that buffer.
 */
static void editor_directory(char* out, size_t size) {

    const char* slash = editor.path[0] ? strrchr(editor.path, '/') : NULL;

    if (slash) {
        snprintf(out, size, "%.*s/", (int)(slash - editor.path), editor.path);
        return;
    }

    if (!getcwd(out, size)) {
        snprintf(out, size, "/");
        return;
    }

    const size_t length = strlen(out);

    if (length + 1 < size && (length == 0 || out[length - 1] != '/')) {
        out[length]     = '/';
        out[length + 1] = '\0';
    }
}


/**
 * @brief Reports why a file could not be opened, in the words the status line uses.
 * @param error The errno editor_buffer_load() left.
 * @return The reason.
 */
static const char* editor_strerror(int error) {

    switch (error) {

        case EISDIR:
            return "it is a directory";

        case EINVAL:
            return "it is not a regular file";

        case EFBIG:
            return "it is larger than 32 MiB";

        case EILSEQ:
            return "it looks like a binary file";

        default:
            return strerror(error);
    }
}


/**
 * @brief Puts the caret and the view back at the top of the document, as a new or opened one starts.
 */
static void editor_reset_view(void) {

    editor.caret.line   = 0;
    editor.caret.offset = 0;

    editor.anchor = editor.caret;
    editor.goal   = 0;

    editor.edits++;

    editor_canvas_home();
    editor_canvas_wake();
}


/**
 * @brief Replaces the document with an empty, untitled one.
 */
static void editor_new_document(void) {

    editor_buffer_t empty;

    if (editor_buffer_init(&empty) < 0) {
        editor_out_of_memory();
        return;
    }

    editor_buffer_free(&editor.buffer);

    editor.buffer  = empty;
    editor.path[0] = '\0';

    editor_history_reset(&editor.history);
    editor_reset_view();
    editor_update_title();

    editor_message("New document");
}


/**
 * @brief Replaces the document with a file, keeping the one open when the file cannot be read.
 * @param path The file.
 * @return 0, or -1 with the reason in the status line.
 */
static int editor_open(const char* path) {

    if (editor_buffer_load(&editor.buffer, path) < 0) {
        editor_message("Cannot open %s: %s", path, editor_strerror(errno));
        return -1;
    }

    snprintf(editor.path, sizeof(editor.path), "%s", path);

    editor_history_reset(&editor.history);
    editor_reset_view();
    editor_update_title();

    editor_message("Opened %s, %zu %s", path, editor.buffer.count, editor.buffer.count == 1 ? "line" : "lines");

    return 0;
}


/**
 * @brief Writes the document to a file, which becomes the one it is saved to from then on.
 * @param path The file.
 * @return 0, or -1 with the reason in the status line.
 */
static int editor_save(const char* path) {

    if (editor_buffer_save(&editor.buffer, path) < 0) {
        editor_message("Cannot save %s: %s", path, strerror(errno));
        return -1;
    }

    if (path != editor.path) {
        snprintf(editor.path, sizeof(editor.path), "%s", path);
    }

    editor_history_mark_clean(&editor.history);
    editor_update_title();

    editor_message("Saved %s", path);

    return 0;
}


/**
 * @brief Runs whatever was waiting on the unsaved changes.
 */
static void editor_run_pending(void) {

    const editor_pending_t pending = editor.pending;

    editor.pending = EDITOR_PENDING_NONE;

    switch (pending) {

        case EDITOR_PENDING_NEW:
            editor_new_document();
            break;

        case EDITOR_PENDING_OPEN:
            editor_open(editor.pending_path);
            break;

        case EDITOR_PENDING_QUIT:
            editor.quit = true;
            break;

        default:
            break;
    }
}


/**
 * @brief Runs an action that would lose the document, asking first whether to save it when it has unsaved changes.
 * @param action The action.
 * @param path The file to open, for EDITOR_PENDING_OPEN.
 */
static void editor_guard(editor_pending_t action, const char* path) {

    editor.pending = action;

    snprintf(editor.pending_path, sizeof(editor.pending_path), "%s", path ? path : "");

    if (editor_history_modified(&editor.history)) {
        editor_prompt_show(EDITOR_PROMPT_CONFIRM, NULL);
        return;
    }

    if (editor.prompt == EDITOR_PROMPT_CONFIRM) {
        editor_prompt_close();
    }

    editor_run_pending();
}


/**
 * @brief Runs a toolbar action.
 * @param action The action.
 */
static void editor_command(editor_action_t action) {

    char path[PATH_MAX];

    switch (action) {

        case EDITOR_ACTION_NEW:
            editor_guard(EDITOR_PENDING_NEW, NULL);
            break;

        case EDITOR_ACTION_OPEN:
            editor_directory(path, sizeof(path));
            editor_prompt_show(EDITOR_PROMPT_OPEN, path);
            break;

        case EDITOR_ACTION_SAVE:

            if (editor.path[0]) {
                editor_save(editor.path);
            } else {
                editor_command(EDITOR_ACTION_SAVE_AS);
            }

            break;

        case EDITOR_ACTION_SAVE_AS:

            if (editor.path[0]) {
                snprintf(path, sizeof(path), "%s", editor.path);
            } else {
                editor_directory(path, sizeof(path));
            }

            editor_prompt_show(EDITOR_PROMPT_SAVE_AS, path);
            break;

        case EDITOR_ACTION_FIND:
            editor_find_prompt();
            break;

        default:
            break;
    }
}


/**
 * @brief Opens the file the open prompt names, once the unsaved changes are dealt with.
 */
static void editor_submit_open(void) {

    char path[PATH_MAX];

    if (!editor_expand_path(ui_entry_text(editor.prompt_entry), path, sizeof(path))) {
        editor_message("Type the path of the file to open");
        return;
    }


    struct stat st;

    if (stat(path, &st) < 0) {
        editor_message("Cannot open %s: %s", path, strerror(errno));
        return;
    }

    if (!S_ISREG(st.st_mode)) {
        editor_message("Cannot open %s: %s", path, editor_strerror(S_ISDIR(st.st_mode) ? EISDIR : EINVAL));
        return;
    }

    editor_prompt_close();
    editor_guard(EDITOR_PENDING_OPEN, path);
}


/**
 * @brief Saves to the file the save prompt names, then runs whatever was waiting on it.
 */
static void editor_submit_save_as(void) {

    char path[PATH_MAX];

    if (!editor_expand_path(ui_entry_text(editor.prompt_entry), path, sizeof(path))) {
        editor_message("Type the path to save the file to");
        return;
    }

    if (editor_save(path) < 0) {
        return;
    }

    editor_prompt_close();
    editor_run_pending();
}


/**
 * @brief Moves the caret to the line, and the column when one is given, that the prompt names.
 */
static void editor_submit_goto(void) {

    const char* text = ui_entry_text(editor.prompt_entry);

    char* end;

    const unsigned long line = strtoul(text, &end, 10);

    unsigned long column = 1;

    if (end != text && (*end == ':' || *end == ',')) {
        column = strtoul(end + 1, &end, 10);
    }

    while (*end == ' ') {
        end++;
    }

    if (end == text || line == 0 || *end != '\0') {
        editor_message("Type a line number, or line:column");
        return;
    }


    const size_t index = line - 1 < editor.buffer.count ? line - 1 : editor.buffer.count - 1;

    const editor_line_t* row = editor_line(index);

    editor_prompt_close();

    editor.caret.line   = index;
    editor.caret.offset = editor_text_offset(row->text, row->length, column > 0 ? column - 1 : 0);
    editor.anchor       = editor.caret;

    editor_history_seal(&editor.history);
    editor_canvas_reveal(true);
}


/**
 * @brief Saves before running what was waiting, asking for a path first when the document has none.
 */
static void editor_confirm_save(void) {

    if (!editor.path[0]) {

        char path[PATH_MAX];

        editor_directory(path, sizeof(path));
        editor_prompt_show(EDITOR_PROMPT_SAVE_AS, path);

        return;
    }

    if (editor_save(editor.path) < 0) {
        return;
    }

    editor_prompt_close();
    editor_run_pending();
}


/**
 * @brief Closes the prompt and forgets whatever was waiting on it.
 */
static void editor_prompt_cancel(void) {

    editor.pending = EDITOR_PENDING_NONE;

    editor_prompt_close();
}


/**
 * @brief Runs one of the prompt's buttons.
 * @param button The button, left to right.
 */
static void editor_prompt_accept(int button) {

    switch (editor.prompt) {

        case EDITOR_PROMPT_FIND:

            if (button == 2) {
                editor_prompt_close();
            } else {
                editor_find(button == 1, false);
            }

            break;

        case EDITOR_PROMPT_OPEN:

            if (button == 0) {
                editor_submit_open();
            } else {
                editor_prompt_cancel();
            }

            break;

        case EDITOR_PROMPT_SAVE_AS:

            if (button == 0) {
                editor_submit_save_as();
            } else {
                editor_prompt_cancel();
            }

            break;

        case EDITOR_PROMPT_GOTO:

            if (button == 0) {
                editor_submit_goto();
            } else {
                editor_prompt_cancel();
            }

            break;

        case EDITOR_PROMPT_CONFIRM:

            if (button == 0) {

                editor_confirm_save();

            } else if (button == 1) {

                editor_prompt_close();
                editor_run_pending();

            } else {

                editor_prompt_cancel();
            }

            break;

        default:
            break;
    }
}


/**
 * @brief Runs a prompt button that was clicked.
 * @param widget The button.
 * @param user Its index, left to right.
 */
static void editor_on_prompt_button(ui_widget_t* widget, void* user) {

    (void)widget;

    editor_prompt_accept((int)(intptr_t)user);
}


/**
 * @brief Runs what Enter in the prompt's entry stands for: the next match, or the prompt's first button.
 * @param widget The entry.
 * @param user Unused.
 */
static void editor_on_prompt_submit(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    if (editor.prompt == EDITOR_PROMPT_FIND) {
        editor_find(!(editor.modifiers & EDITOR_MOD_SHIFT), false);
    } else {
        editor_prompt_accept(0);
    }
}


/**
 * @brief Runs a toolbar button that was clicked.
 * @param widget The button.
 * @param user The action.
 */
static void editor_on_action(ui_widget_t* widget, void* user) {

    (void)widget;

    editor_command((editor_action_t)(intptr_t)user);
}


/**
 * @brief Lays the widgets out again for the window as it stands, as a prompt opening or closing needs.
 */
static void editor_relayout(void) {

    editor_layout(editor.view, ui_window_width(editor.window), ui_window_height(editor.window), NULL);
}


/**
 * @brief Shows the prompt bar asking for something.
 * @param prompt What it asks for.
 * @param text What its entry starts with, or NULL for nothing.
 */
static void editor_prompt_show(editor_prompt_t prompt, const char* text) {

    if ((editor.prompt == EDITOR_PROMPT_FIND) != (prompt == EDITOR_PROMPT_FIND)) {
        editor_canvas_dirty_all();
    }

    if (prompt != EDITOR_PROMPT_CONFIRM && !(prompt == EDITOR_PROMPT_SAVE_AS && editor.prompt == EDITOR_PROMPT_CONFIRM)) {
        editor.pending = EDITOR_PENDING_NONE;
    }

    editor.prompt = prompt;


    if (prompt == EDITOR_PROMPT_CONFIRM) {

        char label[128];

        snprintf(label, sizeof(label), "Save the changes to \xE2\x80\x9C%.60s\xE2\x80\x9D?", editor_basename());

        ui_label_set_text(editor.prompt_label, label);

    } else {

        ui_label_set_text(editor.prompt_label, editor_prompts[prompt].label);
    }


    const bool entry = editor_prompts[prompt].placeholder != NULL;

    if (entry) {
        ui_entry_set_placeholder(editor.prompt_entry, editor_prompts[prompt].placeholder);
        ui_entry_set_text(editor.prompt_entry, text ? text : "");
    }

    ui_widget_set_visible(editor.prompt_entry, entry);


    for (int i = 0; i < EDITOR_PROMPT_BUTTONS; i++) {

        const char* label = editor_prompts[prompt].buttons[i];

        if (label) {
            ui_button_set_text(editor.prompt_buttons[i], label);
            ui_button_set_style(editor.prompt_buttons[i], editor_prompts[prompt].styles[i]);
        }

        ui_widget_set_visible(editor.prompt_buttons[i], label != NULL);
    }

    ui_widget_set_visible(editor.prompt_bar, true);
    ui_widget_set_visible(editor.prompt_label, true);

    editor_relayout();

    ui_view_focus(editor.view, entry ? editor.prompt_entry : NULL);
}


/**
 * @brief Hides the prompt bar and hands the keyboard back to the text.
 */
static void editor_prompt_close(void) {

    if (editor.prompt == EDITOR_PROMPT_NONE) {
        return;
    }

    if (editor.prompt == EDITOR_PROMPT_FIND) {
        editor_canvas_dirty_all();
        editor_message("%s", "");
    }

    editor.prompt = EDITOR_PROMPT_NONE;

    ui_view_focus(editor.view, NULL);

    ui_widget_set_visible(editor.prompt_bar, false);
    ui_widget_set_visible(editor.prompt_label, false);
    ui_widget_set_visible(editor.prompt_entry, false);

    for (int i = 0; i < EDITOR_PROMPT_BUTTONS; i++) {
        ui_widget_set_visible(editor.prompt_buttons[i], false);
    }

    editor_relayout();
}


/**
 * @brief Reports what a key is to the modifier state, and folds it in.
 * @param vkey The key.
 * @param down Whether it went down.
 */
static void editor_track_modifiers(uint16_t vkey, bool down) {

    uint8_t bit = 0;

    switch (vkey) {

        case KEY_LEFTCTRL:
            bit = EDITOR_MOD_LEFTCTRL;
            break;

        case KEY_RIGHTCTRL:
            bit = EDITOR_MOD_RIGHTCTRL;
            break;

        case KEY_LEFTSHIFT:
            bit = EDITOR_MOD_LEFTSHIFT;
            break;

        case KEY_RIGHTSHIFT:
            bit = EDITOR_MOD_RIGHTSHIFT;
            break;

        default:
            return;
    }

    if (down) {
        editor.modifiers |= bit;
    } else {
        editor.modifiers &= (uint8_t)~bit;
    }
}


/**
 * @brief Runs a Ctrl shortcut, the editing ones only while the text has the keyboard.
 * @param vkey The key held with Ctrl.
 * @param shift Whether Shift is held too.
 * @return true when the key is a shortcut.
 */
static bool editor_shortcut(uint16_t vkey, bool shift) {

    const bool text = ui_view_focused(editor.view) == NULL && editor.prompt != EDITOR_PROMPT_CONFIRM;

    switch (vkey) {

        case KEY_N:
            editor_command(EDITOR_ACTION_NEW);
            return true;

        case KEY_O:
            editor_command(EDITOR_ACTION_OPEN);
            return true;

        case KEY_S:
            editor_command(shift ? EDITOR_ACTION_SAVE_AS : EDITOR_ACTION_SAVE);
            return true;

        case KEY_F:
            editor_command(EDITOR_ACTION_FIND);
            return true;

        case KEY_G:
            editor_prompt_show(EDITOR_PROMPT_GOTO, NULL);
            return true;

        case KEY_Q:
            editor_guard(EDITOR_PENDING_QUIT, NULL);
            return true;

        case KEY_A:

            if (text) {
                editor_select_all();
            }

            return true;

        case KEY_C:
        case KEY_X:

            if (text) {
                editor_copy(vkey == KEY_X);
            }

            return true;

        case KEY_V:

            if (text) {
                editor_paste();
            } else {
                editor_paste_into_entry();
            }

            return true;

        case KEY_Z:

            if (text) {
                editor_undo(shift);
            }

            return true;

        case KEY_Y:

            if (text) {
                editor_undo(true);
            }

            return true;

        default:
            return false;
    }
}


/**
 * @brief Runs a key while the text has the keyboard: movement, editing, and typing.
 * @param vkey The key.
 * @param ctrl Whether Ctrl is held.
 * @param shift Whether Shift is held.
 * @return true when the key did anything.
 */
static bool editor_edit_key(uint16_t vkey, bool ctrl, bool shift) {

    editor_pos_t from;
    editor_pos_t to;

    const bool selected = editor_selection(&from, &to);

    switch (vkey) {

        case KEY_LEFT:

            if (selected && !shift) {
                editor_move(from, false);
            } else {
                editor_move(ctrl ? editor_word_left(editor.caret) : editor_prev_pos(editor.caret), shift);
            }

            return true;

        case KEY_RIGHT:

            if (selected && !shift) {
                editor_move(to, false);
            } else {
                editor_move(ctrl ? editor_word_right(editor.caret) : editor_next_pos(editor.caret), shift);
            }

            return true;

        case KEY_UP:
        case KEY_DOWN:

            if (ctrl) {
                editor_canvas_scroll_rows(vkey == KEY_UP ? -1 : 1);
            } else {
                editor_move_vertical(vkey == KEY_UP ? -1 : 1, shift);
            }

            return true;

        case KEY_PAGEUP:
        case KEY_PAGEDOWN:
            editor_page(vkey == KEY_PAGEUP ? -1 : 1, shift);
            return true;

        case KEY_HOME:

            if (ctrl) {
                editor_pos_t start = {0, 0};
                editor_move(start, shift);
            } else {
                editor_home(shift);
            }

            return true;

        case KEY_END:

            if (ctrl) {
                editor_move(editor_buffer_end(&editor.buffer), shift);
            } else {
                editor_pos_t end = {editor.caret.line, editor_line(editor.caret.line)->length};
                editor_move(end, shift);
            }

            return true;

        case KEY_BACKSPACE:
            editor_backspace(ctrl);
            return true;

        case KEY_DELETE:
            editor_delete(ctrl);
            return true;

        case KEY_ENTER:
        case KEY_KPENTER:
            editor_newline();
            return true;

        case KEY_TAB:
            editor_indent(shift);
            return true;

        default:

            if (ctrl || editor.key_size == 0) {
                return false;
            }

            editor_type(editor.key_text, editor.key_size);
            return true;
    }
}


/**
 * @brief Handles a key the focused widget, if any, did not take.
 * @param view Unused.
 * @param vkey The key.
 * @param down Whether it went down.
 * @param user Unused.
 * @return true when the key was acted on.
 */
static bool editor_on_key(ui_view_t* view, uint16_t vkey, bool down, void* user) {

    (void)view;
    (void)user;

    if (!down) {
        return false;
    }


    const bool ctrl  = (editor.modifiers & EDITOR_MOD_CTRL) != 0;
    const bool shift = (editor.modifiers & EDITOR_MOD_SHIFT) != 0;

    if (ctrl && editor_shortcut(vkey, shift)) {
        return true;
    }


    switch (vkey) {

        case KEY_F3:

            if (editor.query[0]) {
                editor_find(!shift, false);
            } else {
                editor_find_prompt();
            }

            return true;

        case KEY_ESC:

            if (editor.prompt != EDITOR_PROMPT_NONE) {
                editor_prompt_cancel();
            } else {
                editor.anchor = editor.caret;
            }

            return true;

        default:
            break;
    }


    if (editor.prompt == EDITOR_PROMPT_CONFIRM) {

        if (vkey == KEY_ENTER || vkey == KEY_KPENTER) {
            editor_prompt_accept(0);
        }

        return true;
    }

    if (ui_view_focused(editor.view)) {

        if (editor.prompt == EDITOR_PROMPT_FIND && (vkey == KEY_UP || vkey == KEY_DOWN)) {
            editor_find(vkey == KEY_DOWN, false);
            return true;
        }

        return false;
    }

    return editor_edit_key(vkey, ctrl, shift);
}


/**
 * @brief Counts a press towards a double or triple click.
 * @param x The press position.
 * @param y The press position.
 * @return 1, 2 or 3.
 */
static int editor_click_count(int x, int y) {

    const uint64_t now = ui_anim_now();

    const bool again = now - editor.click_time <= EDITOR_DOUBLE_CLICK_MS && abs(x - editor.click_x) <= EDITOR_DOUBLE_CLICK_SLOP && abs(y - editor.click_y) <= EDITOR_DOUBLE_CLICK_SLOP;

    editor.clicks     = again ? editor.clicks % 3 + 1 : 1;
    editor.click_time = now;
    editor.click_x    = x;
    editor.click_y    = y;

    return editor.clicks;
}


/**
 * @brief Reports the position under the pointer while a selection is dragged, held to the lines in view.
 * @return The position.
 */
static editor_pos_t editor_drag_hit(void) {

    const ui_rect_t text = editor_canvas_text_rect();

    const int bottom = text.y + (int)editor_canvas_rows() * editor.canvas.line - 1;

    int x = editor.pointer_x;
    int y = editor.pointer_y;

    if (y < text.y) {
        y = text.y;
    }

    if (y > bottom) {
        y = bottom;
    }

    if (x < text.x) {
        x = text.x;
    }

    if (x > text.x + text.width) {
        x = text.x + text.width;
    }

    return editor_canvas_hit(x, y);
}


/**
 * @brief Starts what a press in the text area does: a click, a double click on a word, a triple click on a line, or a scrollbar grab.
 * @param x The press position.
 * @param y The press position.
 */
static void editor_press(int x, int y) {

    ui_rect_t track;
    ui_rect_t thumb;

    if (editor_canvas_scrollbar(&track, &thumb) && x >= track.x) {

        if (y >= thumb.y && y < thumb.y + thumb.height) {

            editor.drag      = EDITOR_DRAG_THUMB;
            editor.drag_grab = y - thumb.y;

        } else {

            const size_t rows = editor_canvas_rows();

            editor_canvas_scroll_rows((y < thumb.y ? -1 : 1) * (rows > 1 ? (long)rows - 1 : 1));
        }

        return;
    }


    ui_view_focus(editor.view, NULL);

    switch (editor_click_count(x, y)) {

        case 2:
            editor_word_at(editor_canvas_hit(x - editor.canvas.cell / 2, y), &editor.anchor, &editor.caret);
            break;

        case 3: {

            const size_t line = editor_canvas_hit(x, y).line;

            editor.anchor.line   = line;
            editor.anchor.offset = 0;

            if (line + 1 < editor.buffer.count) {
                editor.caret.line   = line + 1;
                editor.caret.offset = 0;
            } else {
                editor.caret.line   = line;
                editor.caret.offset = editor_line(line)->length;
            }

            break;
        }

        default:
            editor_move(editor_canvas_hit(x, y), (editor.modifiers & EDITOR_MOD_SHIFT) != 0);
            break;
    }

    editor_history_seal(&editor.history);

    editor.drag          = EDITOR_DRAG_TEXT;
    editor.autoscroll_at = ui_anim_now();
}


/**
 * @brief Follows the pointer: presses, drags, the scrollbar lighting up under it, and the shape it takes.
 * @param event The pointer event.
 */
static void editor_on_pointer(const ui_event_t* event) {

    const int x = event->pointer.x;
    const int y = event->pointer.y;

    const bool down    = (event->pointer.buttons & UI_BUTTON_LEFT) != 0;
    const bool pressed = down && !(editor.buttons & UI_BUTTON_LEFT);

    editor.buttons    = event->pointer.buttons;
    editor.pointer_x  = x;
    editor.pointer_y  = y;
    editor.pointer_in = true;

    if (!down) {

        editor.drag = EDITOR_DRAG_NONE;

    } else if (pressed) {

        if (editor_canvas_contains(x, y)) {
            editor_press(x, y);
        }

    } else if (editor.drag == EDITOR_DRAG_TEXT) {

        editor.caret = editor_drag_hit();

    } else if (editor.drag == EDITOR_DRAG_THUMB) {

        editor_canvas_drag_thumb(y, editor.drag_grab);
    }


    ui_rect_t track;

    const bool inside = editor_canvas_contains(x, y);
    const bool over   = inside && editor_canvas_scrollbar(&track, NULL) && x >= track.x;

    editor_canvas_set_thumb_hot(editor.drag == EDITOR_DRAG_THUMB || (over && editor.drag == EDITOR_DRAG_NONE));

    ui_view_set_cursor(editor.view, editor.drag == EDITOR_DRAG_TEXT || (editor.drag == EDITOR_DRAG_NONE && inside && !over) ? UI_CURSOR_TEXT : UI_CURSOR_DEFAULT);
}


/**
 * @brief Scrolls the text under the pointer, sideways with Shift held.
 * @param event The scroll event.
 */
static void editor_on_scroll(const ui_event_t* event) {

    if (!editor.pointer_in || !editor_canvas_contains(editor.pointer_x, editor.pointer_y)) {
        return;
    }

    if (editor.modifiers & EDITOR_MOD_SHIFT) {
        editor_canvas_scroll_columns(-(long)event->scroll.dy * EDITOR_WHEEL_COLUMNS);
    } else {
        editor_canvas_scroll_rows(-(long)event->scroll.dy * EDITOR_WHEEL_ROWS);
    }

    if (editor.drag == EDITOR_DRAG_TEXT) {
        editor.caret = editor_drag_hit();
    }
}


/**
 * @brief Drops whatever the pointer was doing once it leaves the window.
 */
static void editor_on_leave(void) {

    editor.pointer_in = false;
    editor.buttons    = 0;
    editor.drag       = EDITOR_DRAG_NONE;

    editor_canvas_set_thumb_hot(false);
}


/**
 * @brief Follows the window gaining or losing the keyboard, forgetting the modifiers whose release went elsewhere.
 * @param event The focus event.
 */
static void editor_on_focus(const ui_event_t* event) {

    editor.active    = event->focus.focused != 0;
    editor.modifiers = 0;

    ui_keymap_reset(editor.keymap);

    if (!editor.active) {
        editor.buttons = 0;
        editor.drag    = EDITOR_DRAG_NONE;
    }

    editor_canvas_dirty_all();
    editor_canvas_wake();
}


/**
 * @brief Scrolls a selection being dragged past the edge of the text, a step at a time.
 * @param now The loop's reading of the clock.
 * @return Milliseconds until the next step, or -1 when nothing is scrolling.
 */
static int editor_autoscroll(uint64_t now) {

    if (editor.drag != EDITOR_DRAG_TEXT || !editor.pointer_in) {
        return -1;
    }


    const ui_rect_t text = editor_canvas_text_rect();

    const int bottom = text.y + (int)editor_canvas_rows() * editor.canvas.line;

    long rows    = 0;
    long columns = 0;

    if (editor.pointer_y < text.y) {
        rows = -1;
    } else if (editor.pointer_y >= bottom) {
        rows = 1;
    }

    if (editor.pointer_x < text.x && editor.canvas.left > 0) {
        columns = -1;
    } else if (editor.pointer_x > text.x + text.width) {
        columns = 1;
    }

    if (rows == 0 && columns == 0) {
        return -1;
    }


    const uint64_t elapsed = now > editor.autoscroll_at ? now - editor.autoscroll_at : 0;

    if (elapsed < EDITOR_AUTOSCROLL_MS) {
        return (int)(EDITOR_AUTOSCROLL_MS - elapsed);
    }

    editor.autoscroll_at = now;


    const editor_snapshot_t before = editor_snapshot();

    editor_canvas_scroll_rows(rows);
    editor_canvas_scroll_columns(columns);

    editor.caret = editor_drag_hit();

    editor_refresh(&before);

    return EDITOR_AUTOSCROLL_MS;
}


/**
 * @brief Hands an event to the view and to the editor, and brings the window up to date with what it changed.
 * @param event The event.
 */
static void editor_dispatch(const ui_event_t* event) {

    const editor_snapshot_t before = editor_snapshot();

    switch (event->type) {

        case UI_EVENT_CLOSE:
            editor_guard(EDITOR_PENDING_QUIT, NULL);
            break;

        case UI_EVENT_KEY:

            editor_track_modifiers(event->key.vkey, event->key.down != 0);

            editor.key_size = ui_keymap_translate(editor.keymap, event->key.vkey, event->key.down != 0, editor.key_text, sizeof(editor.key_text));

            ui_view_dispatch(editor.view, event);
            break;

        case UI_EVENT_POINTER:
            ui_view_dispatch(editor.view, event);
            editor_on_pointer(event);
            break;

        case UI_EVENT_SCROLL:
            ui_view_dispatch(editor.view, event);
            editor_on_scroll(event);
            break;

        case UI_EVENT_LEAVE:
            ui_view_dispatch(editor.view, event);
            editor_on_leave();
            break;

        case UI_EVENT_FOCUS:
            ui_view_dispatch(editor.view, event);
            editor_on_focus(event);
            break;

        default:
            ui_view_dispatch(editor.view, event);
            break;
    }

    editor_refresh(&before);
}


/**
 * @brief Reports the sooner of two deadlines, either of which may be -1 for none.
 * @param a A deadline in milliseconds.
 * @param b Another.
 * @return The sooner one.
 */
static int editor_soonest(int a, int b) {

    if (a < 0) {
        return b;
    }

    if (b < 0) {
        return a;
    }

    return a < b ? a : b;
}


/**
 * @brief Reports the width a button needs for its text.
 * @param text The text.
 * @return The width in pixels.
 */
static int editor_button_width(const char* text) {

    const ui_theme_t* theme = ui_theme();

    const int width = (int)ceil(ui_draw_text_width(text, theme->font_medium, theme->font_size)) + EDITOR_BUTTON_PAD * 2;

    return width < EDITOR_BUTTON_MIN ? EDITOR_BUTTON_MIN : width;
}


/**
 * @brief Places the prompt bar's label, entry and buttons, the buttons against the right edge.
 * @param bar The bar.
 */
static void editor_layout_prompt(ui_rect_t bar) {

    ui_widget_place(editor.prompt_bar, bar);

    int right = bar.x + bar.width - EDITOR_BUTTON_GAP;

    for (int i = EDITOR_PROMPT_BUTTONS - 1; i >= 0; i--) {

        const char* label = editor_prompts[editor.prompt].buttons[i];

        if (!label) {
            continue;
        }

        const int width = editor_button_width(label);

        ui_rect_t rect = {right - width, bar.y + 4, width, bar.height - 8};

        ui_widget_place(editor.prompt_buttons[i], rect);

        right -= width + EDITOR_BUTTON_GAP;
    }


    const ui_theme_t* theme = ui_theme();

    const bool entry = editor_prompts[editor.prompt].placeholder != NULL;

    int label_width = right - bar.x;

    if (entry) {
        label_width = (int)ceil(ui_draw_text_width(ui_label_text(editor.prompt_label), theme->font_regular, theme->font_size)) + EDITOR_LABEL_PAD * 3;
    }

    if (label_width > right - bar.x) {
        label_width = right - bar.x;
    }

    if (label_width < 0) {
        label_width = 0;
    }

    ui_rect_t label = {bar.x, bar.y, label_width, bar.height};

    ui_widget_place(editor.prompt_label, label);


    if (entry) {

        ui_rect_t field = {bar.x + label_width, bar.y + 4, right - (bar.x + label_width), bar.height - 8};

        if (field.width < 0) {
            field.width = 0;
        }

        ui_widget_place(editor.prompt_entry, field);
    }
}


/**
 * @brief Places the widgets, and records the frame the text is drawn in.
 * @param view Unused.
 * @param width The content width.
 * @param height The content height.
 * @param user Unused.
 */
static void editor_layout(ui_view_t* view, int width, int height, void* user) {

    (void)view;
    (void)user;

    const int inner = width - EDITOR_MARGIN * 2 > 0 ? width - EDITOR_MARGIN * 2 : 0;

    ui_rect_t toolbar = {EDITOR_MARGIN, EDITOR_MARGIN, inner, EDITOR_TOOLBAR_HEIGHT};

    ui_widget_place(editor.toolbar, toolbar);


    int x = toolbar.x + toolbar.width - EDITOR_BUTTON_GAP;

    for (int i = (int)(sizeof(editor_actions) / sizeof(editor_actions[0])) - 1; i >= 0; i--) {

        ui_rect_t rect = {x - EDITOR_TOOL_SIZE, toolbar.y + (toolbar.height - EDITOR_TOOL_SIZE) / 2, EDITOR_TOOL_SIZE, EDITOR_TOOL_SIZE};

        ui_widget_place(editor.actions[editor_actions[i].action], rect);

        x -= EDITOR_TOOL_SIZE + EDITOR_BUTTON_GAP;
    }

    ui_rect_t name = {toolbar.x, toolbar.y, x - toolbar.x > 0 ? x - toolbar.x : 0, toolbar.height};

    ui_widget_place(editor.name, name);


    int y = toolbar.y + toolbar.height + EDITOR_GAP;

    if (editor.prompt != EDITOR_PROMPT_NONE) {

        ui_rect_t bar = {EDITOR_MARGIN, y, inner, EDITOR_PROMPT_HEIGHT};

        editor_layout_prompt(bar);

        y += EDITOR_PROMPT_HEIGHT + EDITOR_GAP;
    }


    const int status_y = height - EDITOR_MARGIN - EDITOR_STATUS_HEIGHT;

    const int position_width = inner / 2;

    ui_rect_t message  = {EDITOR_MARGIN, status_y, inner - position_width, EDITOR_STATUS_HEIGHT};
    ui_rect_t position = {EDITOR_MARGIN + inner - position_width, status_y, position_width, EDITOR_STATUS_HEIGHT};

    ui_widget_place(editor.message, message);
    ui_widget_place(editor.position, position);


    ui_rect_t frame = {EDITOR_MARGIN, y, inner, status_y - EDITOR_GAP - y > 0 ? status_y - EDITOR_GAP - y : 0};

    ui_widget_place(editor.frame, frame);

    editor_canvas_place(frame);


    editor_show_status();
    editor_update_title();
}


/**
 * @brief Creates the widgets.
 * @param view The view to create them in.
 * @return 0, or -1 when one of them could not be created.
 */
static int editor_build(ui_view_t* view) {

    const ui_theme_t* theme = ui_theme();

    if (!(editor.toolbar = ui_panel_create(view)) || !(editor.name = ui_label_create(view, EDITOR_UNTITLED))) {
        return -1;
    }

    for (size_t i = 0; i < sizeof(editor_actions) / sizeof(editor_actions[0]); i++) {

        ui_widget_t* button = ui_button_create(view, NULL, editor_on_action, (void*)(intptr_t)editor_actions[i].action);

        if (!button) {
            return -1;
        }

        ui_button_set_icon(button, editor_actions[i].icon);
        ui_button_set_icon_size(button, EDITOR_TOOL_ICON);

        editor.actions[editor_actions[i].action] = button;
    }


    if (!(editor.prompt_bar = ui_panel_create(view)) || !(editor.prompt_label = ui_label_create(view, "")) || !(editor.prompt_entry = ui_entry_create(view, ""))) {
        return -1;
    }

    for (int i = 0; i < EDITOR_PROMPT_BUTTONS; i++) {

        if (!(editor.prompt_buttons[i] = ui_button_create(view, "", editor_on_prompt_button, (void*)(intptr_t)i))) {
            return -1;
        }

        ui_widget_set_visible(editor.prompt_buttons[i], false);
    }


    if (!(editor.frame = ui_panel_create(view)) || !(editor.message = ui_label_create(view, "")) || !(editor.position = ui_label_create(view, ""))) {
        return -1;
    }


    ui_panel_set_color(editor.toolbar, theme->surface);
    ui_panel_set_radius(editor.toolbar, theme->corner_radius);

    ui_label_set_padding(editor.name, EDITOR_LABEL_PAD);
    ui_label_set_font(editor.name, UI_FONT_MEDIUM, 0.0);

    ui_panel_set_color(editor.prompt_bar, theme->surface);
    ui_panel_set_radius(editor.prompt_bar, theme->corner_radius);

    ui_label_set_padding(editor.prompt_label, EDITOR_LABEL_PAD);

    ui_entry_on_change(editor.prompt_entry, editor_on_prompt_change, NULL);
    ui_entry_on_submit(editor.prompt_entry, editor_on_prompt_submit, NULL);

    ui_widget_set_visible(editor.prompt_bar, false);
    ui_widget_set_visible(editor.prompt_label, false);
    ui_widget_set_visible(editor.prompt_entry, false);

    ui_panel_set_color(editor.frame, theme->surface_sunken);
    ui_panel_set_radius(editor.frame, theme->corner_radius);
    ui_panel_set_border(editor.frame, theme->border, 1.0);

    ui_label_set_color(editor.message, theme->text_muted);
    ui_label_set_color(editor.position, theme->text_muted);
    ui_label_set_align(editor.position, UI_ALIGN_RIGHT);

    return 0;
}


/**
 * @brief The event loop: the view paints its widgets, the text area paints over its frame, and one commit sends both.
 * @return 0 on a clean close, -1 when the connection went away.
 */
static int editor_run(void) {

    while (!editor.quit) {

        const uint64_t now = ui_anim_now();

        int timeout = ui_view_timeout(editor.view);

        timeout = editor_soonest(timeout, editor_canvas_tick(now));
        timeout = editor_soonest(timeout, editor_autoscroll(now));


        if (ui_view_present(editor.view) < 0) {
            return -1;
        }

        editor_canvas_paint();

        if (ui_window_commit(editor.window) < 0) {
            return -1;
        }


        ui_event_t event;

        const int e = ui_next_event(editor.conn, &event, timeout);

        if (e < 0) {

            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (e == 0) {
            continue;
        }

        editor_dispatch(&event);


        for (size_t drained = 0; drained < EDITOR_DRAIN_MAX && !editor.quit; drained++) {

            const int pending = ui_next_event(editor.conn, &event, 0);

            if (pending < 0) {
                return -1;
            }

            if (pending == 0) {
                break;
            }

            editor_dispatch(&event);
        }
    }

    return 0;
}


/**
 * @brief Prints how to run the editor and the keys it answers to.
 * @param out Where to print it.
 */
static void editor_usage(FILE* out) {

    fprintf(out,
            "Usage: %s [FILE]\n"
            "Edit FILE, or a new document when none is given.\n"
            "\n"
            "  -h, --help       show this help and exit\n"
            "  -v, --version    show version information and exit\n"
            "\n"
            "Keys:\n"
            "  Ctrl+N, Ctrl+O, Ctrl+S, Ctrl+Shift+S    new, open, save, save as\n"
            "  Ctrl+Z, Ctrl+Y, Ctrl+Shift+Z            undo, redo\n"
            "  Ctrl+X, Ctrl+C, Ctrl+V, Ctrl+A          cut, copy, paste, select all\n"
            "  Ctrl+F, F3, Shift+F3                    find, next match, previous match\n"
            "  Ctrl+G                                  go to line\n"
            "  Tab, Shift+Tab                          indent, unindent the selected lines\n"
            "  Ctrl+Left, Ctrl+Right                   move by words\n"
            "  Ctrl+Up, Ctrl+Down                      scroll without moving the caret\n"
            "  Ctrl+Q                                  quit\n",
            EDITOR_NAME);
}


/**
 * @brief Starts on the file named on the command line, or on a new document by that name when it does not exist yet.
 * @param path The file.
 */
static void editor_open_initial(const char* path) {

    struct stat st;

    if (stat(path, &st) == 0 || errno != ENOENT) {
        editor_open(path);
        return;
    }

    snprintf(editor.path, sizeof(editor.path), "%s", path);

    editor_update_title();
    editor_message("New file %s", path);
}


int main(int argc, char** argv) {

    const char* path = NULL;

    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            editor_usage(stdout);
            return 0;
        }

        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("%s (aplus sysutils) %s\n", EDITOR_NAME, EDITOR_VERSION);
            return 0;
        }

        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            editor_usage(stderr);
            return 1;
        }

        path = argv[i];
    }


    editor.active = true;

    editor_history_init(&editor.history);

    if (editor_buffer_init(&editor.buffer) < 0) {
        ulog_errorf("out of memory");
        return 1;
    }

    if (editor_canvas_init() < 0) {
        ulog_errorf("cannot load %s: %s", EDITOR_FONT_PATH, strerror(errno));
        return 1;
    }

    if (!(editor.keymap = ui_keymap_open(NULL))) {
        ulog_warnf("cannot load the keymap, typing will not work: %s", strerror(errno));
    }


    if (!(editor.conn = ui_connect(NULL, 5000))) {
        ulog_errorf("ui_connect() failed: %s", strerror(errno));
        return 1;
    }

    if (!(editor.window = ui_window_create(editor.conn, EDITOR_WINDOW_WIDTH, EDITOR_WINDOW_HEIGHT, "Text Editor"))) {
        ulog_errorf("ui_window_create() failed: %s", strerror(errno));
        ui_disconnect(editor.conn);
        return 1;
    }

    if (!(editor.view = ui_view_create(editor.window))) {
        ulog_errorf("ui_view_create() failed: %s", strerror(errno));
        ui_window_destroy(editor.window);
        ui_disconnect(editor.conn);
        return 1;
    }

    if (editor_build(editor.view) < 0) {
        ulog_errorf("cannot create the widgets");
        ui_view_destroy(editor.view);
        ui_window_destroy(editor.window);
        ui_disconnect(editor.conn);
        return 1;
    }

    ui_view_on_key(editor.view, editor_on_key, NULL);
    ui_view_on_layout(editor.view, editor_layout, NULL);


    if (path) {
        editor_open_initial(path);
    }

    editor_update_title();
    editor_update_position();


    const int status = editor_run();

    if (status < 0) {
        ulog_errorf("the connection went away: %s", strerror(errno));
    }

    ui_view_destroy(editor.view);
    ui_window_destroy(editor.window);
    ui_disconnect(editor.conn);

    ui_keymap_close(editor.keymap);

    editor_canvas_free();
    editor_history_free(&editor.history);
    editor_buffer_free(&editor.buffer);

    free(editor.clipboard);

    return status < 0 ? 1 : 0;
}
