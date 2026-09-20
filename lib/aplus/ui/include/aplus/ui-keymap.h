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

#ifndef _APLUS_UI_KEYMAP_H
#define _APLUS_UI_KEYMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Turning the raw key codes the server sends into the characters they stand for.
 *
 * The protocol carries `KEY_*` codes and nothing else, on the reasoning that the server has
 * no business deciding what a key means. This is where that decision is made for a client
 * that wants text, and it is deliberately outside ui-widgets.h: translation names no cairo
 * type, so a program drawing its own pixels can have characters without the widget layer.
 *
 * A keymap is the one loaded by the console, in the binary form kbd(1) writes.
 */


/**
 * @brief The keymap loaded when neither a path nor the environment names one.
 */
#define UI_KEYMAP_DEFAULT "/usr/share/keymaps/it.map"

/**
 * @brief The variable that names a keymap for the whole session.
 */
#define UI_KEYMAP_ENV "APLUS_KEYMAP"


typedef struct ui_keymap ui_keymap_t;


/**
 * @brief Loads a keymap.
 *
 * @param path The file to load, or NULL for UI_KEYMAP_ENV and then UI_KEYMAP_DEFAULT.
 * @return The keymap, or NULL with errno set. Nothing aborts: a caller with no keymap is a
 *         caller that reads raw key codes, which is where it started.
 */
ui_keymap_t* ui_keymap_open(const char* path);

/**
 * @brief Releases a keymap.
 *
 * @param keymap The keymap, which may be NULL.
 */
void ui_keymap_close(ui_keymap_t* keymap);

/**
 * @brief Reports what a key event stands for, and folds it into the modifier state.
 *
 * Every key event goes through here, releases and modifiers included: the shift and lock
 * entries of the map are what maintain the state the next lookup is made against, so a
 * caller that forwards only the keys it cares about gets the wrong characters.
 *
 * What comes back is what the map says, control bytes included -- Escape is 0x1B here and
 * Backspace is 0x7F, because that is what they are on a console. A caller collecting text
 * acts on those key codes itself and takes only the bytes from 0x20 up.
 *
 * The bytes are UTF-8. A map holds one byte per key in the console's own charset, so an
 * accented letter arrives here as a single byte above 0x7F and leaves as the two that
 * encode it -- which is what everything downstream of a keymap, cairo included, expects.
 *
 * @param keymap The keymap.
 * @param vkey The key code, as UI_EVENT_KEY carried it.
 * @param down Whether the key went down or came up.
 * @param out Receives the bytes, terminated when there is room.
 * @param size The size of that buffer.
 * @return How many bytes were written, which is 0 for every key that stands for no character.
 */
size_t ui_keymap_translate(ui_keymap_t* keymap, uint16_t vkey, bool down, char* out, size_t size);

/**
 * @brief Reports the modifiers currently held, as the bit per KG_* the map indexes itself by.
 *
 * @param keymap The keymap.
 * @return The mask, or 0 when there is no keymap.
 */
uint8_t ui_keymap_modifiers(const ui_keymap_t* keymap);

/**
 * @brief Forgets which modifiers are held, for a window that has just lost the keyboard.
 *
 * The release that would have cleared them goes to whoever has the focus now, so a caller
 * that does not do this on UI_EVENT_FOCUS comes back with shift stuck down.
 *
 * @param keymap The keymap.
 */
void ui_keymap_reset(ui_keymap_t* keymap);


#ifdef __cplusplus
}
#endif
#endif
