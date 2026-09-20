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
 * @brief The console keymap, read for the characters the key codes stand for.
 *
 * The file is the binary form kbd(1) writes: a magic, then one table per modifier
 * combination, each of NR_KEYS entries of a value and a type.
 */

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <aplus/input.h>
#include <aplus/ui-keymap.h>


/**
 * @brief What stands at the head of a keymap file.
 */
#define UI_KEYMAP_MAGIC "KMAP\x00\x00\x00\x00"

/**
 * @brief How long that magic is.
 */
#define UI_KEYMAP_MAGIC_SIZE 8

/**
 * @brief How many tables a file carries, one per combination of the modifiers.
 */
#define UI_KEYMAP_MAPS 256


/**
 * @brief One key of one table: what it produces, and what kind of thing that is.
 */

typedef struct {

    union {

        struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
            uint8_t val;
            uint8_t typ;
#else
            uint8_t typ;
            uint8_t val;
#endif
        } __attribute__((packed));

        uint16_t raw;
    };

} __attribute__((packed)) ui_keymap_key_t;


struct ui_keymap {

    struct {
        ui_keymap_key_t keys[NR_KEYS];
    } __attribute__((packed)) maps[UI_KEYMAP_MAPS];

    //? Which table the next lookup is made against. Maintained by the shift and lock
    //? entries of the map itself rather than by a list of key codes here, so a map that
    //? puts a modifier somewhere unusual still works.
    uint8_t modifiers;
};


/**
 * @brief Reads a file to the end, or reports that it could not be.
 *
 * @param fd The descriptor to read.
 * @param buffer Receives the bytes.
 * @param size How many to read.
 * @return true when every byte arrived, false otherwise.
 */
static bool ui_keymap_read_all(int fd, void* buffer, size_t size) {

    uint8_t* at = (uint8_t*)buffer;

    while (size > 0) {

        const ssize_t got = read(fd, at, size);

        if (got < 0) {

            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        if (got == 0) {

            errno = EPROTO;
            return false;
        }

        at += got;
        size -= (size_t)got;
    }

    return true;
}


ui_keymap_t* ui_keymap_open(const char* path) {

    if (!path) {
        path = getenv(UI_KEYMAP_ENV);
    }

    if (!path || !*path) {
        path = UI_KEYMAP_DEFAULT;
    }


    const int fd = open(path, O_RDONLY);

    if (fd < 0) {
        return NULL;
    }


    ui_keymap_t* keymap = (ui_keymap_t*)calloc(1, sizeof(ui_keymap_t));

    if (!keymap) {

        const int saved = errno;

        close(fd);

        errno = saved;
        return NULL;
    }


    char magic[UI_KEYMAP_MAGIC_SIZE];

    if (!ui_keymap_read_all(fd, magic, sizeof(magic)) || memcmp(magic, UI_KEYMAP_MAGIC, sizeof(magic)) != 0 || !ui_keymap_read_all(fd, keymap->maps, sizeof(keymap->maps))) {

        const int saved = errno == 0 ? EPROTO : errno;

        close(fd);
        free(keymap);

        errno = saved;
        return NULL;
    }

    close(fd);

    return keymap;
}


void ui_keymap_close(ui_keymap_t* keymap) {

    free(keymap);
}


uint8_t ui_keymap_modifiers(const ui_keymap_t* keymap) {

    return keymap ? keymap->modifiers : 0;
}


void ui_keymap_reset(ui_keymap_t* keymap) {

    if (keymap) {
        keymap->modifiers = 0;
    }
}


size_t ui_keymap_translate(ui_keymap_t* keymap, uint16_t vkey, bool down, char* out, size_t size) {

    if (!keymap || vkey >= NR_KEYS) {
        return 0;
    }


    const ui_keymap_key_t key = keymap->maps[keymap->modifiers].keys[vkey];

    switch (key.typ) {

        case KT_SHIFT:

            if (down) {
                keymap->modifiers |= (uint8_t)(1 << key.val);
            } else {
                keymap->modifiers &= (uint8_t) ~(1 << key.val);
            }

            return 0;

        case KT_LOCK:

            if (down) {
                keymap->modifiers ^= (uint8_t)(1 << key.val);
            }

            return 0;

        case KT_LATIN:
        case KT_ASCII:
        case KT_LETTER:
            break;

        default:
            return 0;
    }


    if (!down || key.val == 0 || !out || size == 0) {
        return 0;
    }


    if (key.val < 0x80) {

        out[0] = (char)key.val;

        if (size > 1) {
            out[1] = '\0';
        }

        return 1;
    }


    if (size < 2) {
        return 0;
    }

    out[0] = (char)(0xC0 | (key.val >> 6));
    out[1] = (char)(0x80 | (key.val & 0x3F));

    if (size > 2) {
        out[2] = '\0';
    }

    return 2;
}
