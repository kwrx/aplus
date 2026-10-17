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

#ifndef _APLUS_EDITOR_BUFFER_H
#define _APLUS_EDITOR_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/**
 * @brief The largest file the editor loads, in bytes.
 */
#define EDITOR_FILE_MAX (32 * 1024 * 1024)

/**
 * @brief How many columns a tab advances to, as a multiple of.
 */
#define EDITOR_TAB_WIDTH 4

/**
 * @brief What a byte that is not valid UTF-8 decodes to.
 */
#define EDITOR_UTF8_INVALID 0xFFFD


/**
 * @brief A place in the text: a line, and a byte offset into it.
 */
typedef struct {

    size_t line;
    size_t offset;

} editor_pos_t;


/**
 * @brief One line of text, without its terminator.
 */
typedef struct {

    char* text;
    size_t length;
    size_t capacity;

} editor_line_t;


/**
 * @brief The document, as an array of lines that always holds at least one.
 */
typedef struct {

    editor_line_t* lines;
    size_t count;
    size_t capacity;

    bool crlf;

} editor_buffer_t;


/**
 * @brief Starts an empty document.
 * @param buffer The document.
 * @return 0, or -1 with errno set.
 */
int editor_buffer_init(editor_buffer_t* buffer);

/**
 * @brief Releases a document.
 * @param buffer The document.
 */
void editor_buffer_free(editor_buffer_t* buffer);

/**
 * @brief Replaces a document with the content of a file, leaving it untouched on failure.
 * @param buffer The document.
 * @param path The file to read.
 * @return 0, or -1 with errno set: EISDIR for a directory, EINVAL for anything else that is not a regular file, EFBIG past EDITOR_FILE_MAX, EILSEQ for a binary file.
 */
int editor_buffer_load(editor_buffer_t* buffer, const char* path);

/**
 * @brief Writes a document to a file, replacing what the file held.
 * @param buffer The document.
 * @param path The file to write.
 * @return 0, or -1 with errno set.
 */
int editor_buffer_save(const editor_buffer_t* buffer, const char* path);

/**
 * @brief Inserts text, which may span lines.
 * @param buffer The document.
 * @param at Where to insert it.
 * @param text The bytes to insert.
 * @param size How many there are.
 * @param end Receives the position just past the inserted text, or NULL.
 * @return 0, or -1 with errno set, the document being unchanged.
 */
int editor_buffer_insert(editor_buffer_t* buffer, editor_pos_t at, const char* text, size_t size, editor_pos_t* end);

/**
 * @brief Removes the text between two positions.
 * @param buffer The document.
 * @param from The first position removed.
 * @param to The position just past the last one removed.
 * @return 0, or -1 with errno set, the document being unchanged.
 */
int editor_buffer_erase(editor_buffer_t* buffer, editor_pos_t from, editor_pos_t to);

/**
 * @brief Copies the text between two positions, lines joined by a newline.
 * @param buffer The document.
 * @param from The first position copied.
 * @param to The position just past the last one copied.
 * @param size Receives how many bytes were copied.
 * @return The text, NUL terminated and to be freed, or NULL with errno set.
 */
char* editor_buffer_copy(const editor_buffer_t* buffer, editor_pos_t from, editor_pos_t to, size_t* size);

/**
 * @brief Reports the position past the last character of a document.
 * @param buffer The document.
 * @return The position.
 */
editor_pos_t editor_buffer_end(const editor_buffer_t* buffer);

/**
 * @brief Orders two positions.
 * @param a The first position.
 * @param b The second position.
 * @return Less than, equal to or greater than 0 as a comes before, at or after b.
 */
int editor_pos_compare(editor_pos_t a, editor_pos_t b);

/**
 * @brief Reports where text inserted at a position ends.
 * @param at Where the text starts.
 * @param text The text.
 * @param size Its size in bytes.
 * @return The position just past it.
 */
editor_pos_t editor_pos_after(editor_pos_t at, const char* text, size_t size);


/**
 * @brief Decodes one UTF-8 sequence, taking a malformed byte as EDITOR_UTF8_INVALID on its own.
 * @param text The bytes.
 * @param size How many are available, at least one.
 * @param codepoint Receives the codepoint.
 * @return How many bytes it took.
 */
size_t editor_utf8_decode(const char* text, size_t size, uint32_t* codepoint);

/**
 * @brief Encodes a codepoint as UTF-8.
 * @param codepoint The codepoint.
 * @param out A buffer of at least four bytes.
 * @return How many bytes were written.
 */
size_t editor_utf8_encode(uint32_t codepoint, char* out);

/**
 * @brief Reports the offset of the character after the one at an offset.
 * @param text The line.
 * @param size Its length.
 * @param offset Where to step from.
 * @return The next offset, or size at the end.
 */
size_t editor_text_next(const char* text, size_t size, size_t offset);

/**
 * @brief Reports the offset of the character before an offset.
 * @param text The line.
 * @param size Its length.
 * @param offset Where to step from.
 * @return The previous offset, or 0 at the start.
 */
size_t editor_text_prev(const char* text, size_t size, size_t offset);

/**
 * @brief Reports the column an offset is drawn at, tabs expanded.
 * @param text The line.
 * @param size Its length.
 * @param offset The offset.
 * @return The column.
 */
size_t editor_text_column(const char* text, size_t size, size_t offset);

/**
 * @brief Reports the character boundary nearest a column.
 * @param text The line.
 * @param size Its length.
 * @param column The column.
 * @return The offset.
 */
size_t editor_text_offset(const char* text, size_t size, size_t column);

/**
 * @brief Finds the first match of a query at or after an offset, ignoring ASCII case.
 * @param text The line.
 * @param size Its length.
 * @param from The first offset a match may start at.
 * @param query The text to look for.
 * @param length Its length, at least one.
 * @param at Receives where the match starts.
 * @return true when there is one.
 */
bool editor_text_find(const char* text, size_t size, size_t from, const char* query, size_t length, size_t* at);

/**
 * @brief Finds the last match of a query starting before an offset, ignoring ASCII case.
 * @param text The line.
 * @param size Its length.
 * @param before The offset a match has to start before.
 * @param query The text to look for.
 * @param length Its length, at least one.
 * @param at Receives where the match starts.
 * @return true when there is one.
 */
bool editor_text_rfind(const char* text, size_t size, size_t before, const char* query, size_t length, size_t* at);

#endif
