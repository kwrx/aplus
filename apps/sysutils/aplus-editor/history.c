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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "history.h"


/**
 * @brief How many edits the history makes room for the first time it grows.
 */
#define EDITOR_HISTORY_MIN 64


/**
 * @brief Releases the edits from an index to the end.
 * @param history The history.
 * @param from The first edit to release.
 */
static void editor_history_drop(editor_history_t* history, size_t from) {

    for (size_t i = from; i < history->count; i++) {
        free(history->edits[i].text);
    }

    history->count = from;
}


void editor_history_init(editor_history_t* history) {

    memset(history, 0, sizeof(*history));

    history->sealed = true;
}


void editor_history_free(editor_history_t* history) {

    editor_history_drop(history, 0);

    free(history->edits);

    editor_history_init(history);
}


void editor_history_reset(editor_history_t* history) {

    editor_history_drop(history, 0);

    history->applied    = 0;
    history->clean      = 0;
    history->clean_lost = false;
    history->sealed     = true;
}


void editor_history_forget(editor_history_t* history) {

    editor_history_reset(history);

    history->clean_lost = true;
}


void editor_history_begin(editor_history_t* history) {

    if (history->depth++ == 0) {
        history->group = ++history->groups;
    }
}


void editor_history_end(editor_history_t* history) {

    if (history->depth > 0) {
        history->depth--;
    }
}


void editor_history_seal(editor_history_t* history) {

    history->sealed = true;
}


/**
 * @brief Reports whether a byte is a blank, which is where a run of typing is cut into words.
 * @param byte The byte.
 * @return true for a space or a tab.
 */
static bool editor_history_blank(char byte) {

    return byte == ' ' || byte == '\t';
}


/**
 * @brief Adds text to either end of an edit.
 * @param edit The edit.
 * @param text The text.
 * @param size Its size in bytes.
 * @param front Whether it goes before the edit's text rather than after it.
 * @return 1, or -1 with errno set.
 */
static int editor_history_join(editor_edit_t* edit, const char* text, size_t size, bool front) {

    char* joined = (char*)realloc(edit->text, edit->size + size);

    if (!joined) {
        return -1;
    }

    if (front) {

        memmove(joined + size, joined, edit->size);
        memcpy(joined, text, size);

    } else {

        memcpy(joined + edit->size, text, size);
    }

    edit->text = joined;
    edit->size += size;

    return 1;
}


/**
 * @brief Folds an edit into the last one when both belong to the same run of keystrokes.
 * @param top The last edit.
 * @param type What the new edit did.
 * @param merge The run it belongs to.
 * @param at Where it starts.
 * @param text The text inserted or erased.
 * @param size Its size in bytes.
 * @return 1 when it was folded in, 0 when it has to be recorded apart, -1 with errno set.
 */
static int editor_history_merge(editor_edit_t* top, editor_edit_type_t type, editor_merge_t merge, editor_pos_t at, const char* text, size_t size) {

    if (top->type != type || top->merge != merge || at.line != top->at.line || (merge == EDITOR_MERGE_TYPE) != (type == EDITOR_EDIT_INSERT)) {
        return 0;
    }


    switch (merge) {

        case EDITOR_MERGE_TYPE:

            if (at.offset != top->at.offset + top->size) {
                return 0;
            }

            if (editor_history_blank(text[0]) && top->size > 0 && !editor_history_blank(top->text[top->size - 1])) {
                return 0;
            }

            return editor_history_join(top, text, size, false);

        case EDITOR_MERGE_DELETE:

            if (at.offset != top->at.offset) {
                return 0;
            }

            return editor_history_join(top, text, size, false);

        case EDITOR_MERGE_BACKSPACE:

            if (at.offset + size != top->at.offset) {
                return 0;
            }

            if (editor_history_join(top, text, size, true) < 0) {
                return -1;
            }

            top->at = at;

            return 1;

        default:
            return 0;
    }
}


/**
 * @brief Forgets the oldest group of edits, unless it is the only one.
 * @param history The history.
 */
static void editor_history_trim(editor_history_t* history) {

    size_t oldest = 0;

    while (oldest < history->count && history->edits[oldest].group == history->edits[0].group) {
        oldest++;
    }

    if (oldest == history->count) {
        return;
    }


    for (size_t i = 0; i < oldest; i++) {
        free(history->edits[i].text);
    }

    memmove(&history->edits[0], &history->edits[oldest], (history->count - oldest) * sizeof(editor_edit_t));

    history->count -= oldest;
    history->applied -= oldest;

    if (!history->clean_lost) {

        if (history->clean < oldest) {
            history->clean_lost = true;
        } else {
            history->clean -= oldest;
        }
    }
}


int editor_history_record(editor_history_t* history, editor_edit_type_t type, editor_merge_t merge, editor_pos_t at, const char* text, size_t size, editor_pos_t caret, editor_pos_t anchor) {

    editor_history_drop(history, history->applied);

    if (!history->clean_lost && history->clean > history->applied) {
        history->clean_lost = true;
    }

    if (size == 0 || memchr(text, '\n', size)) {
        merge = EDITOR_MERGE_NONE;
    }


    if (merge != EDITOR_MERGE_NONE && history->depth == 0 && !history->sealed && history->applied > 0 && (history->clean_lost || history->clean != history->applied)) {

        const int merged = editor_history_merge(&history->edits[history->applied - 1], type, merge, at, text, size);

        if (merged < 0) {
            return -1;
        }

        if (merged > 0) {
            return 0;
        }
    }


    if (history->count >= EDITOR_HISTORY_MAX) {
        editor_history_trim(history);
    }

    if (history->count == history->capacity) {

        const size_t capacity = history->capacity ? history->capacity * 2 : EDITOR_HISTORY_MIN;

        editor_edit_t* edits = (editor_edit_t*)realloc(history->edits, capacity * sizeof(editor_edit_t));

        if (!edits) {
            return -1;
        }

        history->edits    = edits;
        history->capacity = capacity;
    }


    char* copy = (char*)malloc(size ? size : 1);

    if (!copy) {
        return -1;
    }

    if (size > 0) {
        memcpy(copy, text, size);
    }


    editor_edit_t* edit = &history->edits[history->count];

    edit->type   = type;
    edit->merge  = merge;
    edit->group  = history->depth ? history->group : ++history->groups;
    edit->at     = at;
    edit->text   = copy;
    edit->size   = size;
    edit->caret  = caret;
    edit->anchor = anchor;

    history->count++;

    history->applied = history->count;
    history->sealed  = false;

    return 0;
}


/**
 * @brief Makes an edit, or takes it back.
 * @param buffer The document.
 * @param edit The edit.
 * @param forward Whether to make it rather than take it back.
 * @param caret Receives where the change ends.
 */
static void editor_history_apply(editor_buffer_t* buffer, const editor_edit_t* edit, bool forward, editor_pos_t* caret) {

    if ((edit->type == EDITOR_EDIT_INSERT) == forward) {

        editor_buffer_insert(buffer, edit->at, edit->text, edit->size, caret);

    } else {

        editor_buffer_erase(buffer, edit->at, editor_pos_after(edit->at, edit->text, edit->size));

        *caret = edit->at;
    }
}


bool editor_history_undo(editor_history_t* history, editor_buffer_t* buffer, editor_pos_t* caret, editor_pos_t* anchor) {

    if (history->applied == 0) {
        return false;
    }


    const unsigned int group = history->edits[history->applied - 1].group;

    while (history->applied > 0 && history->edits[history->applied - 1].group == group) {

        const editor_edit_t* edit = &history->edits[--history->applied];

        editor_history_apply(buffer, edit, false, caret);

        *caret  = edit->caret;
        *anchor = edit->anchor;
    }

    history->sealed = true;

    return true;
}


bool editor_history_redo(editor_history_t* history, editor_buffer_t* buffer, editor_pos_t* caret, editor_pos_t* anchor) {

    if (history->applied == history->count) {
        return false;
    }


    const unsigned int group = history->edits[history->applied].group;

    while (history->applied < history->count && history->edits[history->applied].group == group) {
        editor_history_apply(buffer, &history->edits[history->applied++], true, caret);
    }

    *anchor = *caret;

    history->sealed = true;

    return true;
}


bool editor_history_modified(const editor_history_t* history) {

    return history->clean_lost || history->applied != history->clean;
}


void editor_history_mark_clean(editor_history_t* history) {

    history->clean      = history->applied;
    history->clean_lost = false;
    history->sealed     = true;
}
