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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "buffer.h"


/**
 * @brief The smallest allocation a line or the line array grows from.
 */
#define EDITOR_LINE_MIN  16
#define EDITOR_LINES_MIN 64

/**
 * @brief How much a load reads at a time, and how much a save writes at a time.
 */
#define EDITOR_IO_CHUNK (64 * 1024)


/**
 * @brief A save in progress, batching small lines into large writes.
 */
typedef struct {

    int fd;

    char* data;
    size_t used;

} editor_writer_t;


/**
 * @brief Grows a line so that it holds at least a number of bytes.
 * @param line The line.
 * @param size The bytes it has to hold.
 * @return 0, or -1 with errno set.
 */
static int editor_line_reserve(editor_line_t* line, size_t size) {

    if (size <= line->capacity) {
        return 0;
    }


    size_t capacity = line->capacity ? line->capacity : EDITOR_LINE_MIN;

    while (capacity < size) {
        capacity *= 2;
    }

    char* text = (char*)realloc(line->text, capacity);

    if (!text) {
        return -1;
    }

    line->text     = text;
    line->capacity = capacity;

    return 0;
}


/**
 * @brief Grows the line array so that it holds at least a number of lines.
 * @param buffer The document.
 * @param count The lines it has to hold.
 * @return 0, or -1 with errno set.
 */
static int editor_buffer_reserve(editor_buffer_t* buffer, size_t count) {

    if (count <= buffer->capacity) {
        return 0;
    }


    size_t capacity = buffer->capacity ? buffer->capacity : EDITOR_LINES_MIN;

    while (capacity < count) {
        capacity *= 2;
    }

    editor_line_t* lines = (editor_line_t*)realloc(buffer->lines, capacity * sizeof(editor_line_t));

    if (!lines) {
        return -1;
    }

    buffer->lines    = lines;
    buffer->capacity = capacity;

    return 0;
}


int editor_buffer_init(editor_buffer_t* buffer) {

    memset(buffer, 0, sizeof(*buffer));

    if (editor_buffer_reserve(buffer, 1) < 0) {
        return -1;
    }

    memset(&buffer->lines[0], 0, sizeof(editor_line_t));

    buffer->count = 1;

    return 0;
}


void editor_buffer_free(editor_buffer_t* buffer) {

    for (size_t i = 0; i < buffer->count; i++) {
        free(buffer->lines[i].text);
    }

    free(buffer->lines);

    memset(buffer, 0, sizeof(*buffer));
}


/**
 * @brief Reads a file to its end, whatever size it claims to have.
 * @param fd The file.
 * @param size Receives how many bytes were read.
 * @return The bytes, to be freed, or NULL with errno set.
 */
static char* editor_read_all(int fd, size_t* size) {

    size_t capacity = EDITOR_IO_CHUNK;
    size_t used     = 0;

    char* data = (char*)malloc(capacity);

    if (!data) {
        return NULL;
    }


    for (;;) {

        if (used == capacity) {

            if (capacity > EDITOR_FILE_MAX) {

                free(data);

                errno = EFBIG;
                return NULL;
            }


            const size_t grown = capacity * 2 > EDITOR_FILE_MAX ? EDITOR_FILE_MAX + 1 : capacity * 2;

            char* next = (char*)realloc(data, grown);

            if (!next) {
                free(data);
                return NULL;
            }

            data     = next;
            capacity = grown;
        }


        const ssize_t got = read(fd, data + used, capacity - used);

        if (got < 0) {

            if (errno == EINTR) {
                continue;
            }

            const int saved = errno;

            free(data);

            errno = saved;
            return NULL;
        }

        if (got == 0) {
            break;
        }

        used += (size_t)got;
    }


    if (used > EDITOR_FILE_MAX) {

        free(data);

        errno = EFBIG;
        return NULL;
    }

    *size = used;

    return data;
}


/**
 * @brief Replaces a document with the lines of some text, taking CRLF terminators off when the first line has one.
 * @param buffer The document.
 * @param data The text.
 * @param size Its size in bytes.
 * @return 0, or -1 with errno set, the document being unchanged.
 */
static int editor_buffer_split(editor_buffer_t* buffer, const char* data, size_t size) {

    size_t count = 1;

    for (size_t i = 0; i < size; i++) {

        if (data[i] == '\n') {
            count++;
        }
    }


    editor_buffer_t next;

    memset(&next, 0, sizeof(next));

    if (editor_buffer_reserve(&next, count) < 0) {
        return -1;
    }


    const char* first = size ? (const char*)memchr(data, '\n', size) : NULL;

    next.crlf = first && first > data && first[-1] == '\r';


    size_t start = 0;

    for (size_t i = 0; i < count; i++) {

        const char* newline = start < size ? (const char*)memchr(data + start, '\n', size - start) : NULL;

        const size_t span = newline ? (size_t)(newline - (data + start)) : size - start;

        size_t length = span;

        if (next.crlf && length > 0 && data[start + length - 1] == '\r') {
            length--;
        }


        editor_line_t* line = &next.lines[i];

        memset(line, 0, sizeof(*line));

        next.count = i + 1;

        if (length > 0) {

            if ((line->text = (char*)malloc(length)) == NULL) {
                editor_buffer_free(&next);
                return -1;
            }

            memcpy(line->text, data + start, length);

            line->length   = length;
            line->capacity = length;
        }

        start += span + 1;
    }


    editor_buffer_free(buffer);

    *buffer = next;

    return 0;
}


int editor_buffer_load(editor_buffer_t* buffer, const char* path) {

    const int fd = open(path, O_RDONLY);

    if (fd < 0) {
        return -1;
    }


    struct stat st;

    if (fstat(fd, &st) < 0) {

        const int saved = errno;

        close(fd);

        errno = saved;
        return -1;
    }

    if (!S_ISREG(st.st_mode)) {

        close(fd);

        errno = S_ISDIR(st.st_mode) ? EISDIR : EINVAL;
        return -1;
    }


    size_t size = 0;

    char* data = editor_read_all(fd, &size);

    const int saved = errno;

    close(fd);

    if (!data) {
        errno = saved;
        return -1;
    }

    if (size > 0 && memchr(data, '\0', size)) {

        free(data);

        errno = EILSEQ;
        return -1;
    }


    const int status = editor_buffer_split(buffer, data, size);

    const int split = errno;

    free(data);

    errno = split;

    return status;
}


/**
 * @brief Writes every byte of a block, going round again on a short write.
 * @param fd The file.
 * @param data The bytes.
 * @param size How many there are.
 * @return 0, or -1 with errno set.
 */
static int editor_write_all(int fd, const char* data, size_t size) {

    while (size > 0) {

        const ssize_t put = write(fd, data, size);

        if (put < 0) {

            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        data += put;
        size -= (size_t)put;
    }

    return 0;
}


/**
 * @brief Writes out whatever a save has batched up.
 * @param writer The save.
 * @return 0, or -1 with errno set.
 */
static int editor_writer_flush(editor_writer_t* writer) {

    const size_t used = writer->used;

    writer->used = 0;

    return editor_write_all(writer->fd, writer->data, used);
}


/**
 * @brief Adds bytes to a save, writing them straight through when they would not fit the batch.
 * @param writer The save.
 * @param data The bytes.
 * @param size How many there are.
 * @return 0, or -1 with errno set.
 */
static int editor_writer_put(editor_writer_t* writer, const char* data, size_t size) {

    if (size == 0) {
        return 0;
    }

    if (writer->used + size > EDITOR_IO_CHUNK) {

        if (editor_writer_flush(writer) < 0) {
            return -1;
        }

        if (size >= EDITOR_IO_CHUNK) {
            return editor_write_all(writer->fd, data, size);
        }
    }

    memcpy(writer->data + writer->used, data, size);

    writer->used += size;

    return 0;
}


int editor_buffer_save(const editor_buffer_t* buffer, const char* path) {

    editor_writer_t writer = {

        .fd   = -1,
        .data = (char*)malloc(EDITOR_IO_CHUNK),
        .used = 0,
    };

    if (!writer.data) {
        return -1;
    }

    if ((writer.fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {

        const int saved = errno;

        free(writer.data);

        errno = saved;
        return -1;
    }


    const char* newline = buffer->crlf ? "\r\n" : "\n";

    const size_t newline_size = buffer->crlf ? 2 : 1;

    int status = 0;

    for (size_t i = 0; i < buffer->count && status == 0; i++) {

        status = editor_writer_put(&writer, buffer->lines[i].text, buffer->lines[i].length);

        if (status == 0 && i + 1 < buffer->count) {
            status = editor_writer_put(&writer, newline, newline_size);
        }
    }

    if (status == 0) {
        status = editor_writer_flush(&writer);
    }


    const int saved = errno;

    if (close(writer.fd) < 0 && status == 0) {
        status = -1;
    } else {
        errno = saved;
    }

    free(writer.data);

    return status;
}


/**
 * @brief Releases the lines a failed insert had prepared.
 * @param lines The lines.
 * @param count How many there are.
 */
static void editor_lines_free(editor_line_t* lines, size_t count) {

    for (size_t i = 0; i < count; i++) {
        free(lines[i].text);
    }

    free(lines);
}


int editor_buffer_insert(editor_buffer_t* buffer, editor_pos_t at, const char* text, size_t size, editor_pos_t* end) {

    if (end) {
        *end = at;
    }

    if (size == 0) {
        return 0;
    }


    const char* newline = (const char*)memchr(text, '\n', size);

    if (!newline) {

        editor_line_t* line = &buffer->lines[at.line];

        if (editor_line_reserve(line, line->length + size) < 0) {
            return -1;
        }

        memmove(line->text + at.offset + size, line->text + at.offset, line->length - at.offset);
        memcpy(line->text + at.offset, text, size);

        line->length += size;

        if (end) {
            end->offset = at.offset + size;
        }

        return 0;
    }


    size_t breaks = 0;

    for (size_t i = 0; i < size; i++) {

        if (text[i] == '\n') {
            breaks++;
        }
    }

    if (editor_buffer_reserve(buffer, buffer->count + breaks) < 0) {
        return -1;
    }


    editor_line_t* line = &buffer->lines[at.line];

    editor_line_t* added = (editor_line_t*)calloc(breaks, sizeof(editor_line_t));

    if (!added) {
        return -1;
    }


    const size_t tail = line->length - at.offset;

    const char* piece = newline + 1;

    size_t last = 0;

    for (size_t i = 0; i < breaks; i++) {

        const char* next = (const char*)memchr(piece, '\n', (size_t)(text + size - piece));

        const size_t length = next ? (size_t)(next - piece) : (size_t)(text + size - piece);

        const size_t total = length + (i + 1 == breaks ? tail : 0);

        if (total > 0 && editor_line_reserve(&added[i], total) < 0) {
            editor_lines_free(added, breaks);
            return -1;
        }

        if (length > 0) {
            memcpy(added[i].text, piece, length);
        }

        if (i + 1 == breaks && tail > 0) {
            memcpy(added[i].text + length, line->text + at.offset, tail);
        }

        added[i].length = total;

        last  = length;
        piece = next ? next + 1 : text + size;
    }


    const size_t head = (size_t)(newline - text);

    if (editor_line_reserve(line, at.offset + head) < 0) {
        editor_lines_free(added, breaks);
        return -1;
    }

    if (head > 0) {
        memcpy(line->text + at.offset, text, head);
    }

    line->length = at.offset + head;


    memmove(&buffer->lines[at.line + 1 + breaks], &buffer->lines[at.line + 1], (buffer->count - at.line - 1) * sizeof(editor_line_t));
    memcpy(&buffer->lines[at.line + 1], added, breaks * sizeof(editor_line_t));

    buffer->count += breaks;

    free(added);

    if (end) {
        end->line   = at.line + breaks;
        end->offset = last;
    }

    return 0;
}


int editor_buffer_erase(editor_buffer_t* buffer, editor_pos_t from, editor_pos_t to) {

    if (editor_pos_compare(from, to) >= 0) {
        return 0;
    }


    editor_line_t* first = &buffer->lines[from.line];

    if (from.line == to.line) {

        memmove(first->text + from.offset, first->text + to.offset, first->length - to.offset);

        first->length -= to.offset - from.offset;

        return 0;
    }


    const editor_line_t* last = &buffer->lines[to.line];

    const size_t tail = last->length - to.offset;

    if (editor_line_reserve(first, from.offset + tail) < 0) {
        return -1;
    }

    if (tail > 0) {
        memcpy(first->text + from.offset, last->text + to.offset, tail);
    }

    first->length = from.offset + tail;


    for (size_t i = from.line + 1; i <= to.line; i++) {
        free(buffer->lines[i].text);
    }

    memmove(&buffer->lines[from.line + 1], &buffer->lines[to.line + 1], (buffer->count - to.line - 1) * sizeof(editor_line_t));

    buffer->count -= to.line - from.line;

    return 0;
}


char* editor_buffer_copy(const editor_buffer_t* buffer, editor_pos_t from, editor_pos_t to, size_t* size) {

    size_t total = 0;

    for (size_t i = from.line; i <= to.line; i++) {

        const size_t start = i == from.line ? from.offset : 0;
        const size_t stop  = i == to.line ? to.offset : buffer->lines[i].length;

        total += stop - start + (i < to.line ? 1 : 0);
    }


    char* out = (char*)malloc(total + 1);

    if (!out) {
        return NULL;
    }


    size_t used = 0;

    for (size_t i = from.line; i <= to.line; i++) {

        const size_t start = i == from.line ? from.offset : 0;
        const size_t stop  = i == to.line ? to.offset : buffer->lines[i].length;

        if (stop > start) {
            memcpy(out + used, buffer->lines[i].text + start, stop - start);
        }

        used += stop - start;

        if (i < to.line) {
            out[used++] = '\n';
        }
    }

    out[used] = '\0';

    *size = used;

    return out;
}


editor_pos_t editor_buffer_end(const editor_buffer_t* buffer) {

    editor_pos_t end = {buffer->count - 1, buffer->lines[buffer->count - 1].length};

    return end;
}


int editor_pos_compare(editor_pos_t a, editor_pos_t b) {

    if (a.line != b.line) {
        return a.line < b.line ? -1 : 1;
    }

    if (a.offset != b.offset) {
        return a.offset < b.offset ? -1 : 1;
    }

    return 0;
}


editor_pos_t editor_pos_after(editor_pos_t at, const char* text, size_t size) {

    editor_pos_t end = at;

    size_t start = 0;

    for (size_t i = 0; i < size; i++) {

        if (text[i] == '\n') {
            end.line++;
            start = i + 1;
        }
    }

    end.offset = end.line == at.line ? at.offset + size : size - start;

    return end;
}


size_t editor_utf8_decode(const char* text, size_t size, uint32_t* codepoint) {

    const unsigned char* bytes = (const unsigned char*)text;

    const unsigned char lead = bytes[0];

    if (lead < 0x80) {

        *codepoint = lead;

        return 1;
    }


    size_t length;
    uint32_t value;
    uint32_t least;

    if (lead >= 0xC2 && lead <= 0xDF) {

        length = 2;
        value  = lead & 0x1F;
        least  = 0x80;

    } else if (lead >= 0xE0 && lead <= 0xEF) {

        length = 3;
        value  = lead & 0x0F;
        least  = 0x800;

    } else if (lead >= 0xF0 && lead <= 0xF4) {

        length = 4;
        value  = lead & 0x07;
        least  = 0x10000;

    } else {

        *codepoint = EDITOR_UTF8_INVALID;

        return 1;
    }


    if (length > size) {

        *codepoint = EDITOR_UTF8_INVALID;

        return 1;
    }

    for (size_t i = 1; i < length; i++) {

        if ((bytes[i] & 0xC0) != 0x80) {

            *codepoint = EDITOR_UTF8_INVALID;

            return 1;
        }

        value = (value << 6) | (bytes[i] & 0x3F);
    }

    if (value < least || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {

        *codepoint = EDITOR_UTF8_INVALID;

        return 1;
    }

    *codepoint = value;

    return length;
}


size_t editor_utf8_encode(uint32_t codepoint, char* out) {

    if (codepoint < 0x80) {

        out[0] = (char)codepoint;

        return 1;
    }

    if (codepoint < 0x800) {

        out[0] = (char)(0xC0 | (codepoint >> 6));
        out[1] = (char)(0x80 | (codepoint & 0x3F));

        return 2;
    }

    if (codepoint < 0x10000) {

        out[0] = (char)(0xE0 | (codepoint >> 12));
        out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = (char)(0x80 | (codepoint & 0x3F));

        return 3;
    }

    out[0] = (char)(0xF0 | (codepoint >> 18));
    out[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    out[3] = (char)(0x80 | (codepoint & 0x3F));

    return 4;
}


size_t editor_text_next(const char* text, size_t size, size_t offset) {

    if (offset >= size) {
        return size;
    }

    uint32_t codepoint;

    return offset + editor_utf8_decode(text + offset, size - offset, &codepoint);
}


size_t editor_text_prev(const char* text, size_t size, size_t offset) {

    if (offset == 0) {
        return 0;
    }


    size_t start = offset - 1;

    for (int steps = 0; start > 0 && steps < 3 && ((unsigned char)text[start] & 0xC0) == 0x80; steps++) {
        start--;
    }

    uint32_t codepoint;

    if (start + editor_utf8_decode(text + start, size - start, &codepoint) == offset) {
        return start;
    }

    return offset - 1;
}


size_t editor_text_column(const char* text, size_t size, size_t offset) {

    size_t column = 0;

    for (size_t at = 0; at < offset && at < size;) {

        uint32_t codepoint;

        at += editor_utf8_decode(text + at, size - at, &codepoint);

        column = codepoint == '\t' ? (column / EDITOR_TAB_WIDTH + 1) * EDITOR_TAB_WIDTH : column + 1;
    }

    return column;
}


size_t editor_text_offset(const char* text, size_t size, size_t column) {

    size_t current = 0;

    for (size_t at = 0; at < size;) {

        uint32_t codepoint;

        const size_t length = editor_utf8_decode(text + at, size - at, &codepoint);

        const size_t next = codepoint == '\t' ? (current / EDITOR_TAB_WIDTH + 1) * EDITOR_TAB_WIDTH : current + 1;

        if (column < next) {
            return (column - current) * 2 < next - current ? at : at + length;
        }

        at += length;
        current = next;
    }

    return size;
}


/**
 * @brief Folds an ASCII letter to lower case, leaving every other byte alone.
 * @param byte The byte.
 * @return The folded byte.
 */
static unsigned char editor_fold(unsigned char byte) {

    return byte >= 'A' && byte <= 'Z' ? (unsigned char)(byte - 'A' + 'a') : byte;
}


/**
 * @brief Reports whether a query matches at an offset, ignoring ASCII case.
 * @param text The line, with at least length bytes from the offset.
 * @param query The query.
 * @param length Its length.
 * @return true when it matches.
 */
static bool editor_text_matches(const char* text, const char* query, size_t length) {

    for (size_t i = 0; i < length; i++) {

        if (editor_fold((unsigned char)text[i]) != editor_fold((unsigned char)query[i])) {
            return false;
        }
    }

    return true;
}


bool editor_text_find(const char* text, size_t size, size_t from, const char* query, size_t length, size_t* at) {

    if (length == 0 || length > size) {
        return false;
    }

    for (size_t i = from; i + length <= size; i++) {

        if (editor_text_matches(text + i, query, length)) {

            *at = i;

            return true;
        }
    }

    return false;
}


bool editor_text_rfind(const char* text, size_t size, size_t before, const char* query, size_t length, size_t* at) {

    if (length == 0 || length > size || before == 0) {
        return false;
    }


    size_t i = size - length;

    if (i > before - 1) {
        i = before - 1;
    }

    for (;; i--) {

        if (editor_text_matches(text + i, query, length)) {

            *at = i;

            return true;
        }

        if (i == 0) {
            return false;
        }
    }
}
