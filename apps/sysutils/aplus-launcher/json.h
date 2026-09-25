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

#ifndef _APLUS_LAUNCHER_JSON_H
#define _APLUS_LAUNCHER_JSON_H

#include <stddef.h>


/**
 * @brief Just enough JSON to talk to a chat completions endpoint.
 *
 * A value is a pointer into the document rather than a node of a tree: nothing is allocated,
 * nothing is owned, and a document that is malformed past the part being read still answers.
 */


/**
 * @brief Finds what an object says about a key.
 *
 * @param object The object to look in, which may be NULL.
 * @param key The member name, which is matched as it stands and so may not carry escapes.
 * @return The value, or NULL when the object has no such member.
 */
const char* json_member(const char* object, const char* key);

/**
 * @brief Finds one element of an array.
 *
 * @param array The array to look in, which may be NULL.
 * @param index Which element, counting from zero.
 * @return The element, or NULL when the array is shorter than that.
 */
const char* json_element(const char* array, size_t index);

/**
 * @brief Decodes a string value, turning its escapes back into the bytes they stand for.
 *
 * @param value The value to decode, which may be NULL or something other than a string.
 * @param out Receives the text, always NUL terminated and truncated to fit.
 * @param max The size of that buffer.
 * @return How long the text is, which is 0 when the value was not a string.
 */
size_t json_string(const char* value, char* out, size_t max);

/**
 * @brief Encodes text as the body of a JSON string, without the quotes around it.
 *
 * @param out Receives the encoded text, always NUL terminated and truncated to fit.
 * @param max The size of that buffer.
 * @param text What to encode, which may be NULL.
 * @return How long the encoded text is.
 */
size_t json_escape(char* out, size_t max, const char* text);

#endif
