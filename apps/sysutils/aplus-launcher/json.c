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

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "json.h"


/**
 * @brief Steps over whitespace.
 *
 * @param p Where to start.
 * @return The first character that is not whitespace, which may be the terminator.
 */

static const char* json_space(const char* p) {

    while (*p && isspace((unsigned char)*p)) {
        p++;
    }

    return p;
}


/**
 * @brief Steps over a string, which the caller has already found the opening quote of.
 *
 * @param p The opening quote.
 * @return What follows the closing quote, or the terminator when there is none.
 */

static const char* json_skip_string(const char* p) {

    p++;

    while (*p) {

        if (*p == '\\' && p[1]) {
            p += 2;
            continue;
        }

        if (*p == '"') {
            return p + 1;
        }

        p++;
    }

    return p;
}


/**
 * @brief Steps over one value of any kind, so that the next member of an object can be reached.
 *
 * @param p The value.
 * @return What follows it.
 */

static const char* json_skip(const char* p) {

    p = json_space(p);

    if (*p == '"') {
        return json_skip_string(p);
    }

    if (*p == '{' || *p == '[') {

        int depth = 0;

        while (*p) {

            if (*p == '"') {
                p = json_skip_string(p);
                continue;
            }

            if (*p == '{' || *p == '[') {

                depth++;

            } else if (*p == '}' || *p == ']') {

                if (--depth == 0) {
                    return p + 1;
                }
            }

            p++;
        }

        return p;
    }

    while (*p && *p != ',' && *p != '}' && *p != ']' && !isspace((unsigned char)*p)) {
        p++;
    }

    return p;
}


/**
 * @brief Reads the four hexadecimal digits of an escape.
 *
 * @param p The first digit.
 * @param out Receives the code point.
 * @return true when four digits were there.
 */

static bool json_hex(const char* p, unsigned* out) {

    unsigned value = 0;

    for (int i = 0; i < 4; i++) {

        const char c = p[i];

        if (c >= '0' && c <= '9') {
            value = value * 16 + (unsigned)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            value = value * 16 + (unsigned)(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            value = value * 16 + (unsigned)(c - 'A' + 10);
        } else {
            return false;
        }
    }

    *out = value;

    return true;
}


/**
 * @brief Writes one code point as UTF-8, which is what the widgets draw.
 *
 * @param code The code point.
 * @param out Where to write it.
 * @param max How much room there is.
 * @return How many bytes were written, which is 0 when there was not room for it.
 */

static size_t json_utf8(unsigned code, char* out, size_t max) {

    if (code < 0x80) {

        if (max < 1) {
            return 0;
        }

        out[0] = (char)code;

        return 1;
    }

    if (code < 0x800) {

        if (max < 2) {
            return 0;
        }

        out[0] = (char)(0xC0 | (code >> 6));
        out[1] = (char)(0x80 | (code & 0x3F));

        return 2;
    }

    if (code < 0x10000) {

        if (max < 3) {
            return 0;
        }

        out[0] = (char)(0xE0 | (code >> 12));
        out[1] = (char)(0x80 | ((code >> 6) & 0x3F));
        out[2] = (char)(0x80 | (code & 0x3F));

        return 3;
    }

    if (max < 4) {
        return 0;
    }

    out[0] = (char)(0xF0 | (code >> 18));
    out[1] = (char)(0x80 | ((code >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((code >> 6) & 0x3F));
    out[3] = (char)(0x80 | (code & 0x3F));

    return 4;
}


const char* json_member(const char* object, const char* key) {

    if (!object || !key) {
        return NULL;
    }


    const char* p = json_space(object);

    if (*p != '{') {
        return NULL;
    }

    p++;


    const size_t want = strlen(key);

    for (;;) {

        p = json_space(p);

        if (*p != '"') {
            return NULL;
        }


        const char* name = p + 1;
        const char* end  = json_skip_string(p) - 1;

        p = json_space(end + 1);

        if (*p != ':') {
            return NULL;
        }

        p = json_space(p + 1);


        if ((size_t)(end - name) == want && memcmp(name, key, want) == 0) {
            return p;
        }

        p = json_space(json_skip(p));

        if (*p != ',') {
            return NULL;
        }

        p++;
    }
}


const char* json_element(const char* array, size_t index) {

    if (!array) {
        return NULL;
    }


    const char* p = json_space(array);

    if (*p != '[') {
        return NULL;
    }

    p = json_space(p + 1);


    for (size_t i = 0; *p && *p != ']'; i++) {

        if (i == index) {
            return p;
        }

        p = json_space(json_skip(p));

        if (*p != ',') {
            return NULL;
        }

        p = json_space(p + 1);
    }

    return NULL;
}


size_t json_string(const char* value, char* out, size_t max) {

    if (!out || max == 0) {
        return 0;
    }

    out[0] = '\0';

    if (!value) {
        return 0;
    }


    const char* p = json_space(value);

    if (*p != '"') {
        return 0;
    }

    p++;


    size_t len = 0;

    while (*p && *p != '"' && len + 1 < max) {

        if (*p != '\\') {

            out[len++] = *p++;
            continue;
        }

        p++;

        if (!*p) {
            break;
        }

        switch (*p) {

            case 'n':
                out[len++] = '\n';
                p++;
                break;

            case 't':
                out[len++] = '\t';
                p++;
                break;

            case 'r':
                out[len++] = '\r';
                p++;
                break;

            case 'b':
                out[len++] = '\b';
                p++;
                break;

            case 'f':
                out[len++] = '\f';
                p++;
                break;

            case 'u': {

                unsigned code = 0;

                if (!json_hex(p + 1, &code)) {

                    out[len++] = *p++;
                    break;
                }

                p += 5;

                if (code >= 0xD800 && code <= 0xDBFF && p[0] == '\\' && p[1] == 'u') {

                    unsigned low = 0;

                    if (json_hex(p + 2, &low) && low >= 0xDC00 && low <= 0xDFFF) {

                        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                        p += 6;
                    }
                }

                len += json_utf8(code, out + len, max - len - 1);

            } break;

            default:
                out[len++] = *p++;
                break;
        }
    }

    out[len] = '\0';

    return len;
}


size_t json_escape(char* out, size_t max, const char* text) {

    if (!out || max == 0) {
        return 0;
    }

    out[0] = '\0';


    size_t len = 0;

    for (const char* p = text; p && *p; p++) {

        const unsigned char c = (unsigned char)*p;

        char seq[8];
        size_t n = 1;

        switch (c) {

            case '"':
                memcpy(seq, "\\\"", 2);
                n = 2;
                break;

            case '\\':
                memcpy(seq, "\\\\", 2);
                n = 2;
                break;

            case '\n':
                memcpy(seq, "\\n", 2);
                n = 2;
                break;

            case '\r':
                memcpy(seq, "\\r", 2);
                n = 2;
                break;

            case '\t':
                memcpy(seq, "\\t", 2);
                n = 2;
                break;

            default:

                if (c < 0x20) {
                    n = (size_t)snprintf(seq, sizeof(seq), "\\u%04x", c);
                } else {
                    seq[0] = (char)c;
                }

                break;
        }

        if (len + n + 1 > max) {
            break;
        }

        memcpy(out + len, seq, n);

        len += n;
    }

    out[len] = '\0';

    return len;
}
