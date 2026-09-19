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
 * @brief Opens a path with whatever application is meant to handle it.
 *
 * The handler replaces this process rather than being forked from it, so whoever spawned
 * aplus-xopen ends up with the application itself as its child and reaps it as usual.
 */

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>


/**
 * @brief What the messages call this program.
 */
#define XOPEN_NAME "aplus-xopen"

/**
 * @brief How long the Exec line of a .desktop file may be.
 */
#define XOPEN_EXEC_MAX 256

/**
 * @brief How many arguments an Exec line may carry, the terminator included.
 */
#define XOPEN_ARGV_MAX 16

/**
 * @brief What shows a directory.
 */
#define XOPEN_FILE_MANAGER "aplus-explorer"

/**
 * @brief What runs a .desktop file that asks for a terminal.
 */
#define XOPEN_TERMINAL "aplus-terminal"

/**
 * @brief What shows a picture.
 */
#define XOPEN_IMAGE_VIEWER "aplus-image-viewer"


/**
 * @brief The exit codes, which are the ones xdg-open(1) is specified to use.
 */

typedef enum {

    XOPEN_OK         = 0,
    XOPEN_USAGE      = 1,
    XOPEN_NOT_FOUND  = 2,
    XOPEN_NO_HANDLER = 3,
    XOPEN_FAILED     = 4,

} xopen_status_t;


/**
 * @brief What a path is, as far as choosing a handler for it goes.
 */

typedef enum {

    XOPEN_KIND_UNKNOWN = 0,
    XOPEN_KIND_DIRECTORY,
    XOPEN_KIND_DESKTOP,
    XOPEN_KIND_IMAGE,

} xopen_kind_t;


/**
 * @brief What a .desktop file says about the application it describes.
 */

typedef struct {

    char exec[XOPEN_EXEC_MAX];

    bool terminal;

} xopen_desktop_t;


/**
 * @brief The extensions that are known, and what each one opens as.
 */

static const struct {

    const char* extension;
    xopen_kind_t kind;

} xopen_types[] = {

    {".desktop", XOPEN_KIND_DESKTOP},
    {".png",     XOPEN_KIND_IMAGE  },
    {".jpg",     XOPEN_KIND_IMAGE  },
    {".jpeg",    XOPEN_KIND_IMAGE  },
    {".bmp",     XOPEN_KIND_IMAGE  },
    {".gif",     XOPEN_KIND_IMAGE  },
    {".tiff",    XOPEN_KIND_IMAGE  },
    {".tif",     XOPEN_KIND_IMAGE  },
    {".ico",     XOPEN_KIND_IMAGE  },
    {".svg",     XOPEN_KIND_IMAGE  },
    {".webp",    XOPEN_KIND_IMAGE  },
};


/**
 * @brief Reports what a path is, which is its extension unless the filesystem already answers it.
 *
 * @param path The path.
 * @param st What stat(2) said about it.
 * @return The kind, XOPEN_KIND_UNKNOWN when no handler covers it.
 */

static xopen_kind_t xopen_kind(const char* path, const struct stat* st) {

    if (S_ISDIR(st->st_mode)) {
        return XOPEN_KIND_DIRECTORY;
    }


    const char* name = strrchr(path, '/');

    name = name ? name + 1 : path;

    const char* extension = strrchr(name, '.');

    if (!extension) {
        return XOPEN_KIND_UNKNOWN;
    }


    for (size_t i = 0; i < sizeof(xopen_types) / sizeof(xopen_types[0]); i++) {

        if (strcasecmp(extension, xopen_types[i].extension) == 0) {
            return xopen_types[i].kind;
        }
    }

    return XOPEN_KIND_UNKNOWN;
}


/**
 * @brief Matches an unlocalised key at the head of a .desktop line and reports where its value starts.
 *
 * @param text The line, already trimmed.
 * @param key The key to match.
 * @return The value, or NULL when the line carries another key.
 */

static const char* xopen_desktop_value(const char* text, const char* key) {

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
 * @brief Reads what a .desktop file says about running its application.
 *
 * Only the [Desktop Entry] group counts, and only its unlocalised keys: every other group is skipped.
 *
 * @param path The file to read.
 * @param out Receives the description, zeroed for whatever the file leaves out.
 * @return true when the file carried a [Desktop Entry] group, false otherwise.
 */

static bool xopen_desktop_read(const char* path, xopen_desktop_t* out) {

    memset(out, 0, sizeof(*out));

    FILE* file = fopen(path, "r");

    if (!file) {
        return false;
    }


    char line[512];

    bool group = false;
    bool found = false;

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

        if ((value = xopen_desktop_value(text, "Exec")) != NULL) {

            strncpy(out->exec, value, sizeof(out->exec) - 1);

        } else if ((value = xopen_desktop_value(text, "Terminal")) != NULL) {

            out->terminal = strcmp(value, "true") == 0;
        }
    }

    fclose(file);

    return found;
}


/**
 * @brief Reports whether an Exec argument is a field code, which stands for the documents a launch from here has none of.
 *
 * @param token The argument.
 * @return true when it is a field code, false otherwise.
 */

static bool xopen_desktop_field_code(const char* token) {

    return token[0] == '%' && token[1] != '\0' && token[2] == '\0' && strchr("fFuUdDnNickvm", token[1]) != NULL;
}


/**
 * @brief Splits an Exec line into an argument vector, dropping the field codes.
 *
 * @param exec The Exec value, cut up in place.
 * @param argv Receives the arguments, NULL terminated.
 * @param max How many slots argv has, the terminator included.
 * @return How many arguments were written.
 */

static size_t xopen_desktop_argv(char* exec, char** argv, size_t max) {

    size_t count = 0;

    char* cursor = exec;

    while (count + 1 < max) {

        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }

        if (*cursor == '\0') {
            break;
        }


        char* token;

        if (*cursor == '"') {

            token = ++cursor;

            while (*cursor && *cursor != '"') {
                cursor++;
            }

        } else {

            token = cursor;

            while (*cursor && *cursor != ' ' && *cursor != '\t') {
                cursor++;
            }
        }

        if (*cursor) {
            *cursor++ = '\0';
        }


        if (*token && !xopen_desktop_field_code(token)) {
            argv[count++] = token;
        }
    }

    argv[count] = NULL;

    return count;
}


/**
 * @brief Becomes the application.
 *
 * @param argv The command, NULL terminated.
 * @return XOPEN_NO_HANDLER, since this returns only when the command could not be run at all.
 */

static int xopen_run(char** argv) {

    execvp(argv[0], argv);

    fprintf(stderr, "%s: %s: %s\n", XOPEN_NAME, argv[0], strerror(errno));

    return XOPEN_NO_HANDLER;
}


/**
 * @brief Runs the application a .desktop file describes, through a terminal when it asks for one.
 *
 * @param path The .desktop file.
 * @return An exit code, this returning at all meaning the application did not start.
 */

static int xopen_desktop(const char* path) {

    xopen_desktop_t desktop;

    char* argv[XOPEN_ARGV_MAX];

    if (!xopen_desktop_read(path, &desktop)) {

        fprintf(stderr, "%s: %s: not a desktop entry\n", XOPEN_NAME, path);

        return XOPEN_FAILED;
    }

    if (xopen_desktop_argv(desktop.exec, argv, XOPEN_ARGV_MAX) == 0) {

        fprintf(stderr, "%s: %s: no command to run\n", XOPEN_NAME, path);

        return XOPEN_FAILED;
    }

    if (!desktop.terminal) {
        return xopen_run(argv);
    }


    char command[XOPEN_EXEC_MAX];

    size_t used = 0;

    for (size_t i = 0; argv[i]; i++) {

        const int written = snprintf(command + used, sizeof(command) - used, i ? " %s" : "%s", argv[i]);

        if (written < 0 || (size_t)written >= sizeof(command) - used) {
            break;
        }

        used += (size_t)written;
    }


    char* terminal[] = {(char*)XOPEN_TERMINAL, (char*)"-c", command, NULL};

    return xopen_run(terminal);
}


int main(int argc, char** argv) {

    if (argc != 2 || argv[1][0] == '\0') {

        fprintf(stderr, "usage: %s <path>\n", XOPEN_NAME);

        return XOPEN_USAGE;
    }


    const char* path = argv[1];

    struct stat st;

    if (stat(path, &st) < 0) {

        fprintf(stderr, "%s: %s: %s\n", XOPEN_NAME, path, strerror(errno));

        return XOPEN_NOT_FOUND;
    }


    char* manager[] = {(char*)XOPEN_FILE_MANAGER, (char*)path, NULL};
    char* viewer[]  = {(char*)XOPEN_IMAGE_VIEWER, (char*)path, NULL};

    switch (xopen_kind(path, &st)) {

        case XOPEN_KIND_DIRECTORY:
            return xopen_run(manager);

        case XOPEN_KIND_DESKTOP:
            return xopen_desktop(path);

        case XOPEN_KIND_IMAGE:
            return xopen_run(viewer);

        default:
            break;
    }

    fprintf(stderr, "%s: %s: no application handles this file\n", XOPEN_NAME, path);

    return XOPEN_NO_HANDLER;
}
