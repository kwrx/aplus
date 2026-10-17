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

#ifndef _APLUS_EDITOR_EDITOR_H
#define _APLUS_EDITOR_EDITOR_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cairo/cairo.h>

#include <aplus/ui-keymap.h>
#include <aplus/ui-widgets.h>
#include <aplus/ui.h>

#include "buffer.h"
#include "history.h"


#define EDITOR_NAME "aplus-editor"

/**
 * @brief The monospaced face the text is drawn with, and its size in pixels.
 */
#define EDITOR_FONT_PATH "/usr/share/fonts/ttf/UbuntuMono-R.ttf"
#define EDITOR_FONT_SIZE 16

/**
 * @brief How many codepoints keep the glyph they map to one array lookup away.
 */
#define EDITOR_GLYPH_CACHE_MAX 512

/**
 * @brief How long a search query may be, terminator included, which is what an entry holds.
 */
#define EDITOR_QUERY_MAX 256

/**
 * @brief How many buttons the prompt bar has room for.
 */
#define EDITOR_PROMPT_BUTTONS 3


/**
 * @brief The toolbar buttons, left to right.
 */
typedef enum {

    EDITOR_ACTION_NEW = 0,
    EDITOR_ACTION_OPEN,
    EDITOR_ACTION_SAVE,
    EDITOR_ACTION_SAVE_AS,
    EDITOR_ACTION_FIND,

    EDITOR_ACTION_COUNT,

} editor_action_t;


/**
 * @brief What the bar under the toolbar is asking for, if anything.
 */
typedef enum {

    EDITOR_PROMPT_NONE = 0,
    EDITOR_PROMPT_FIND,
    EDITOR_PROMPT_OPEN,
    EDITOR_PROMPT_SAVE_AS,
    EDITOR_PROMPT_GOTO,
    EDITOR_PROMPT_CONFIRM,

    EDITOR_PROMPT_COUNT,

} editor_prompt_t;


/**
 * @brief What runs once the unsaved changes have been saved or discarded.
 */
typedef enum {

    EDITOR_PENDING_NONE = 0,
    EDITOR_PENDING_NEW,
    EDITOR_PENDING_OPEN,
    EDITOR_PENDING_QUIT,

} editor_pending_t;


/**
 * @brief What a held left button is dragging.
 */
typedef enum {

    EDITOR_DRAG_NONE = 0,
    EDITOR_DRAG_TEXT,
    EDITOR_DRAG_THUMB,

} editor_drag_t;


/**
 * @brief The text area: where it is, the font it draws with, how far it is scrolled, and what it has to repaint.
 */
typedef struct {

    ui_rect_t area;

    cairo_scaled_font_t* font;
    int32_t glyphs[EDITOR_GLYPH_CACHE_MAX];
    unsigned long replacement;

    int cell;
    int line;
    int baseline;

    size_t top;
    size_t left;
    size_t digits;

    ui_damage_t dirty;

    bool caret_on;
    uint64_t caret_at;

    bool thumb_hot;

    cairo_glyph_t* run;
    size_t run_capacity;

} editor_canvas_t;


/**
 * @brief Everything the editor keeps between one event and the next.
 */
typedef struct {

    ui_connection_t* conn;
    ui_window_t* window;
    ui_view_t* view;
    ui_keymap_t* keymap;

    ui_widget_t* toolbar;
    ui_widget_t* name;
    ui_widget_t* actions[EDITOR_ACTION_COUNT];

    ui_widget_t* prompt_bar;
    ui_widget_t* prompt_label;
    ui_widget_t* prompt_entry;
    ui_widget_t* prompt_buttons[EDITOR_PROMPT_BUTTONS];

    ui_widget_t* frame;
    ui_widget_t* message;
    ui_widget_t* position;

    editor_canvas_t canvas;

    editor_buffer_t buffer;
    editor_history_t history;

    editor_pos_t caret;
    editor_pos_t anchor;

    size_t goal;
    bool keep_goal;

    unsigned long edits;

    char path[PATH_MAX];
    char title[UI_TITLE_MAX];
    char status[256];

    char* clipboard;
    size_t clipboard_size;
    bool clipboard_lines;

    editor_prompt_t prompt;
    editor_pending_t pending;
    char pending_path[PATH_MAX];

    char query[EDITOR_QUERY_MAX];
    size_t matches;

    uint8_t modifiers;
    char key_text[8];
    size_t key_size;

    uint8_t buttons;
    int pointer_x;
    int pointer_y;
    bool pointer_in;

    editor_drag_t drag;
    int drag_grab;
    uint64_t autoscroll_at;

    uint64_t click_time;
    int click_x;
    int click_y;
    int clicks;

    bool active;
    bool quit;

} editor_t;


extern editor_t editor;


/**
 * @brief Reports the selection in document order.
 * @param from Receives where it starts.
 * @param to Receives where it ends.
 * @return true when anything is selected.
 */
bool editor_selection(editor_pos_t* from, editor_pos_t* to);


/**
 * @brief Loads the font and takes the cell size from it.
 * @return 0, or -1 with errno set.
 */
int editor_canvas_init(void);

/**
 * @brief Releases what the text area holds.
 */
void editor_canvas_free(void);

/**
 * @brief Moves the text area, which repaints it whole.
 * @param area The frame it is drawn inside, in window coordinates.
 */
void editor_canvas_place(ui_rect_t area);

/**
 * @brief Scrolls back to the top left corner, as a newly opened document starts.
 */
void editor_canvas_home(void);

/**
 * @brief Reports how many whole lines fit the text area.
 * @return The count, at least one.
 */
size_t editor_canvas_rows(void);

/**
 * @brief Reports how many whole columns fit the text area.
 * @return The count, at least one.
 */
size_t editor_canvas_columns(void);

/**
 * @brief Reports whether the caret blinks, which is when the text has the keyboard and the window is active.
 * @return true when it does.
 */
bool editor_canvas_focused(void);

/**
 * @brief Marks the whole text area for repainting.
 */
void editor_canvas_dirty_all(void);

/**
 * @brief Marks a run of lines for repainting.
 * @param first The first line.
 * @param last The last line, or SIZE_MAX for everything below the first.
 */
void editor_canvas_dirty_lines(size_t first, size_t last);

/**
 * @brief Marks the scrollbar for repainting.
 */
void editor_canvas_dirty_scrollbar(void);

/**
 * @brief Follows a change in the number of lines: the gutter width, the scrollbar and the scroll range.
 */
void editor_canvas_sync(void);

/**
 * @brief Repaints whatever is marked and damages the window there, without committing.
 */
void editor_canvas_paint(void);

/**
 * @brief Reports whether a window point is inside the text area.
 * @param x The window x.
 * @param y The window y.
 * @return true when it is.
 */
bool editor_canvas_contains(int x, int y);

/**
 * @brief Reports the rectangle the lines are drawn in, gutter excluded.
 * @return The rectangle, in window coordinates.
 */
ui_rect_t editor_canvas_text_rect(void);

/**
 * @brief Reports the document position nearest a window point.
 * @param x The window x.
 * @param y The window y.
 * @return The position.
 */
editor_pos_t editor_canvas_hit(int x, int y);

/**
 * @brief Scrolls the least that brings the caret into view.
 * @param center Whether a caret out of view is brought to the middle rather than to the nearest edge.
 */
void editor_canvas_reveal(bool center);

/**
 * @brief Scrolls by a number of lines.
 * @param delta Lines to scroll, negative towards the start.
 */
void editor_canvas_scroll_rows(long delta);

/**
 * @brief Scrolls sideways by a number of columns.
 * @param delta Columns to scroll, negative towards the start.
 */
void editor_canvas_scroll_columns(long delta);

/**
 * @brief Reports the scrollbar's track and thumb.
 * @param track Receives the track, or NULL.
 * @param thumb Receives the thumb, or NULL.
 * @return Whether there is anything to scroll.
 */
bool editor_canvas_scrollbar(ui_rect_t* track, ui_rect_t* thumb);

/**
 * @brief Scrolls to wherever a dragged thumb puts the view.
 * @param y The pointer y.
 * @param grab How far below the top of the thumb it was grabbed.
 */
void editor_canvas_drag_thumb(int y, int grab);

/**
 * @brief Lights the thumb while the pointer is over the scrollbar or dragging it.
 * @param hot Whether it is lit.
 */
void editor_canvas_set_thumb_hot(bool hot);

/**
 * @brief Shows the caret and restarts its blink, as every keystroke does.
 */
void editor_canvas_wake(void);

/**
 * @brief Blinks the caret.
 * @param now The loop's reading of the clock, in milliseconds.
 * @return Milliseconds until it next has to blink, or -1 when it does not.
 */
int editor_canvas_tick(uint64_t now);

#endif
