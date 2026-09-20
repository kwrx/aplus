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
 * @brief Reading what the applications directory has to offer.
 *
 * The half of a .desktop file this cares about is the half aplus-xopen does not: the name,
 * the comment and the icon a person picks an application by.
 */

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "desktop.h"


/**
 * @brief How many entries the array holds to begin with, doubling from there.
 */
#define LAUNCHER_SCAN_CAPACITY 16

/**
 * @brief The suffix a file must carry to be read at all.
 */
#define LAUNCHER_SUFFIX ".desktop"


/**
 * @brief Matches an unlocalised key at the head of a line and reports where its value starts.
 *
 * @param text The line, already trimmed.
 * @param key The key to match.
 * @return The value, or NULL when the line carries another key.
 */

static const char* launcher_desktop_value(const char* text, const char* key) {

    const size_t length = strlen(key);

    if (strncmp(text, key, length) != 0) {
        return NULL;
    }


    const char* value = text + length;

    while (*value == ' ' || *value == '\t') {
        value++;
    }

    if (*value != '=') {
        return NULL;
    }

    value++;

    while (*value == ' ' || *value == '\t') {
        value++;
    }

    return value;
}


/**
 * @brief Copies a value into a fixed field, truncating what does not fit.
 *
 * @param field The field to fill.
 * @param size The size of that field.
 * @param value The value to copy.
 */

static void launcher_desktop_store(char* field, size_t size, const char* value) {

    strncpy(field, value, size - 1);

    field[size - 1] = '\0';
}


/**
 * @brief Reads one .desktop file into an entry.
 *
 * Only the [Desktop Entry] group counts, and only its unlocalised keys: every other group
 * is skipped, the way aplus-xopen reads the same files.
 *
 * @param path The file to read.
 * @param out Receives the entry, zeroed for whatever the file leaves out.
 * @return true when the file describes an application that should be shown.
 */

static bool launcher_desktop_read(const char* path, launcher_entry_t* out) {

    memset(out, 0, sizeof(*out));

    FILE* file = fopen(path, "r");

    if (!file) {
        return false;
    }


    char line[512];

    bool group   = false;
    bool found   = false;
    bool hidden  = false;
    bool program = true;

    while (fgets(line, sizeof(line), file)) {

        char* text = line;

        while (*text == ' ' || *text == '\t') {
            text++;
        }

        size_t length = strlen(text);

        while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r' || text[length - 1] == ' ' || text[length - 1] == '\t')) {
            text[--length] = '\0';
        }

        if (length == 0 || text[0] == '#') {
            continue;
        }


        if (text[0] == '[') {

            if (group) {
                break;
            }

            group = strcmp(text, "[Desktop Entry]") == 0;
            found = found || group;

            continue;
        }

        if (!group) {
            continue;
        }


        const char* value;

        if ((value = launcher_desktop_value(text, "Name")) != NULL) {

            launcher_desktop_store(out->name, sizeof(out->name), value);

        } else if ((value = launcher_desktop_value(text, "Comment")) != NULL) {

            launcher_desktop_store(out->comment, sizeof(out->comment), value);

        } else if ((value = launcher_desktop_value(text, "Exec")) != NULL) {

            launcher_desktop_store(out->exec, sizeof(out->exec), value);

        } else if ((value = launcher_desktop_value(text, "Icon")) != NULL) {

            launcher_desktop_store(out->icon, sizeof(out->icon), value);

        } else if ((value = launcher_desktop_value(text, "NoDisplay")) != NULL) {

            hidden = strcmp(value, "true") == 0;

        } else if ((value = launcher_desktop_value(text, "Type")) != NULL) {

            program = strcmp(value, "Application") == 0;
        }
    }

    fclose(file);

    if (!found || hidden || !program) {
        return false;
    }

    launcher_desktop_store(out->path, sizeof(out->path), path);

    return true;
}


/**
 * @brief Reports whether a name ends in the suffix a desktop entry carries.
 *
 * @param name The file name.
 * @return true when it does.
 */

static bool launcher_desktop_named(const char* name) {

    const size_t length = strlen(name);
    const size_t suffix = strlen(LAUNCHER_SUFFIX);

    return length > suffix && strcmp(name + length - suffix, LAUNCHER_SUFFIX) == 0;
}


/**
 * @brief Orders two entries by the name they are shown under.
 *
 * @param a The first entry.
 * @param b The second entry.
 * @return Less than, equal to or greater than zero, as strcasecmp(3) reports it.
 */

static int launcher_desktop_order(const void* a, const void* b) {

    return strcasecmp(((const launcher_entry_t*)a)->name, ((const launcher_entry_t*)b)->name);
}


size_t launcher_scan(const char* path, launcher_entry_t** out) {

    *out = NULL;

    DIR* dir = opendir(path);

    if (!dir) {
        return 0;
    }


    launcher_entry_t* entries = NULL;

    size_t count    = 0;
    size_t capacity = 0;

    struct dirent* it;

    while ((it = readdir(dir)) != NULL) {

        if (!launcher_desktop_named(it->d_name)) {
            continue;
        }


        if (count == capacity) {

            const size_t next = capacity ? capacity * 2 : LAUNCHER_SCAN_CAPACITY;

            launcher_entry_t* grown = (launcher_entry_t*)realloc(entries, next * sizeof(launcher_entry_t));

            if (!grown) {
                break;
            }

            entries  = grown;
            capacity = next;
        }


        char file[PATH_MAX];

        if (snprintf(file, sizeof(file), "%s/%s", path, it->d_name) >= (int)sizeof(file)) {
            continue;
        }

        if (!launcher_desktop_read(file, &entries[count])) {
            continue;
        }

        if (entries[count].name[0] == '\0') {
            launcher_desktop_store(entries[count].name, sizeof(entries[count].name), it->d_name);
        }

        count++;
    }

    closedir(dir);

    if (count == 0) {

        free(entries);
        return 0;
    }

    qsort(entries, count, sizeof(launcher_entry_t), launcher_desktop_order);

    *out = entries;

    return count;
}
