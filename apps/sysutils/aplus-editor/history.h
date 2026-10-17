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

#ifndef _APLUS_EDITOR_HISTORY_H
#define _APLUS_EDITOR_HISTORY_H

#include <stdbool.h>
#include <stddef.h>

#include "buffer.h"


/**
 * @brief How many edits the history keeps before it forgets the oldest.
 */
#define EDITOR_HISTORY_MAX 4096


/**
 * @brief What an edit did to the document.
 */
typedef enum {

    EDITOR_EDIT_INSERT = 0,
    EDITOR_EDIT_ERASE,

} editor_edit_type_t;


/**
 * @brief Which run of keystrokes an edit may be folded into, so that undo takes back a word rather than a letter.
 */
typedef enum {

    EDITOR_MERGE_NONE = 0,
    EDITOR_MERGE_TYPE,
    EDITOR_MERGE_BACKSPACE,
    EDITOR_MERGE_DELETE,

} editor_merge_t;


/**
 * @brief One edit, with the selection it was made from.
 */
typedef struct {

    editor_edit_type_t type;
    editor_merge_t merge;

    unsigned int group;

    editor_pos_t at;

    char* text;
    size_t size;

    editor_pos_t caret;
    editor_pos_t anchor;

} editor_edit_t;


/**
 * @brief The undo history: the edits made, how many of them are applied, and which count matches the file on disk.
 */
typedef struct {

    editor_edit_t* edits;
    size_t count;
    size_t capacity;
    size_t applied;

    size_t clean;
    bool clean_lost;

    unsigned int groups;
    unsigned int group;
    unsigned int depth;

    bool sealed;

} editor_history_t;


/**
 * @brief Starts an empty history that matches the file on disk.
 * @param history The history.
 */
void editor_history_init(editor_history_t* history);

/**
 * @brief Releases a history.
 * @param history The history.
 */
void editor_history_free(editor_history_t* history);

/**
 * @brief Forgets every edit, the document now matching the file on disk.
 * @param history The history.
 */
void editor_history_reset(editor_history_t* history);

/**
 * @brief Forgets every edit, the document no longer matching the file on disk.
 * @param history The history.
 */
void editor_history_forget(editor_history_t* history);

/**
 * @brief Opens a group: every edit recorded until the matching end is undone as one.
 * @param history The history.
 */
void editor_history_begin(editor_history_t* history);

/**
 * @brief Closes a group opened by editor_history_begin().
 * @param history The history.
 */
void editor_history_end(editor_history_t* history);

/**
 * @brief Keeps the next edit from being folded into the last one.
 * @param history The history.
 */
void editor_history_seal(editor_history_t* history);

/**
 * @brief Records an edit already made to the document, dropping whatever could have been redone.
 * @param history The history.
 * @param type What the edit did.
 * @param merge The run it may be folded into.
 * @param at Where it starts.
 * @param text The text inserted or erased.
 * @param size Its size in bytes.
 * @param caret The caret before the edit.
 * @param anchor The selection anchor before the edit.
 * @return 0, or -1 with errno set.
 */
int editor_history_record(editor_history_t* history, editor_edit_type_t type, editor_merge_t merge, editor_pos_t at, const char* text, size_t size, editor_pos_t caret, editor_pos_t anchor);

/**
 * @brief Takes back the last group of edits.
 * @param history The history.
 * @param buffer The document to take them back from.
 * @param caret Receives the caret from before the group.
 * @param anchor Receives the selection anchor from before the group.
 * @return true when there was anything to undo.
 */
bool editor_history_undo(editor_history_t* history, editor_buffer_t* buffer, editor_pos_t* caret, editor_pos_t* anchor);

/**
 * @brief Makes again the next group of edits that was undone.
 * @param history The history.
 * @param buffer The document to make them in.
 * @param caret Receives the caret after the group.
 * @param anchor Receives the selection anchor after the group.
 * @return true when there was anything to redo.
 */
bool editor_history_redo(editor_history_t* history, editor_buffer_t* buffer, editor_pos_t* caret, editor_pos_t* anchor);

/**
 * @brief Reports whether the document differs from the file on disk.
 * @param history The history.
 * @return true when it does.
 */
bool editor_history_modified(const editor_history_t* history);

/**
 * @brief Records that the document now matches the file on disk.
 * @param history The history.
 */
void editor_history_mark_clean(editor_history_t* history);

#endif
