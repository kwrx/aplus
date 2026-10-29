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
 * @brief The applications pinned to the taskbar: found by the name their windows give, opened through their .desktop file,
 * and kept in WM_PINS_PATH between boots.
 */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <aplus/ulog.h>

#include <wm.h>


/**
 * @brief How closely a .desktop file matches the name a window gives: by its file name, by the program it runs, or by its icon.
 */
typedef enum {

    WM_APP_MATCH_NONE = 0,
    WM_APP_MATCH_ICON,
    WM_APP_MATCH_EXEC,
    WM_APP_MATCH_FILE,

} wm_app_match_t;


/**
 * @brief Matches an unlocalised key at the head of a trimmed line.
 *
 * @param text The line.
 * @param key The key.
 * @return Where its value starts, or NULL when the line carries another key.
 */
static const char* wm_app_value(const char* text, const char* key) {

    const size_t length = strlen(key);

    if (strncmp(text, key, length) != 0) {
        return NULL;
    }

    const char* value = text + length;

    while (*value == ' ' || *value == '\t') {
        value++;
    }

    if (*value++ != '=') {
        return NULL;
    }

    while (*value == ' ' || *value == '\t') {
        value++;
    }

    return value;
}


/**
 * @brief Reads the [Desktop Entry] group of a .desktop file.
 *
 * @param path The file.
 * @param out Receives its name and icon, and its path.
 * @param exec Receives the name of the program its Exec line runs, without the directory.
 * @param size The size of exec.
 * @return true when it describes an application.
 */
static bool wm_app_read(const char* path, wm_app_t* out, char* exec, size_t size) {

    FILE* f = fopen(path, "r");

    if (!f) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    snprintf(out->path, sizeof(out->path), "%s", path);

    exec[0] = '\0';

    char line[512];

    bool group   = false;
    bool program = false;

    while (fgets(line, sizeof(line), f)) {

        char* text = line + strspn(line, " \t");

        text[strcspn(text, "\r\n")] = '\0';

        if (text[0] == '[') {

            if (group) {
                break;
            }

            group = strcmp(text, "[Desktop Entry]") == 0;

            continue;
        }

        if (!group) {
            continue;
        }

        const char* value;

        if ((value = wm_app_value(text, "Name")) != NULL) {

            snprintf(out->name, sizeof(out->name), "%s", value);

        } else if ((value = wm_app_value(text, "Icon")) != NULL) {

            snprintf(out->icon, sizeof(out->icon), "%s", value);

        } else if ((value = wm_app_value(text, "Type")) != NULL) {

            program = strcmp(value, "Application") == 0;

        } else if ((value = wm_app_value(text, "Exec")) != NULL) {

            const char* end  = value + strcspn(value, " \t");
            const char* base = value;

            for (const char* c = value; c < end; c++) {

                if (*c == '/') {
                    base = c + 1;
                }
            }

            snprintf(exec, size, "%.*s", (int)(end - base), base);
        }
    }

    fclose(f);

    return program;
}


/**
 * @brief Names an application after its windows, and falls back on that name for whatever its .desktop file left out.
 *
 * @param app The application, as its .desktop file was read.
 * @param id The name its windows give, shorter than UI_ICON_NAME_MAX.
 */
static void wm_app_name(wm_app_t* app, const char* id) {

    snprintf(app->id, sizeof(app->id), "%s", id);

    if (!app->name[0]) {
        snprintf(app->name, sizeof(app->name), "%s", id);
    }

    if (!app->icon[0]) {
        snprintf(app->icon, sizeof(app->icon), "%s", id);
    }
}


/**
 * @brief Finds the application a window belongs to: the .desktop file named after it, or else one that runs it or shares its icon.
 *
 * @param id The name the window gives, which is its program's unless it chose another.
 * @param out Receives the application, its id set to the one given.
 * @return false when no installed application matches.
 */
bool wm_app_resolve(const char* id, wm_app_t* out) {

    char path[WM_APP_PATH_MAX];
    char exec[UI_ICON_NAME_MAX];

    if (!id || !id[0] || strlen(id) >= sizeof(out->id)) {
        return false;
    }

    snprintf(path, sizeof(path), "%s/%s.desktop", WM_APPLICATIONS_PATH, id);

    wm_app_match_t best = wm_app_read(path, out, exec, sizeof(exec)) ? WM_APP_MATCH_FILE : WM_APP_MATCH_NONE;

    DIR* dir = best ? NULL : opendir(WM_APPLICATIONS_PATH);

    for (struct dirent* ent; dir && best < WM_APP_MATCH_EXEC && (ent = readdir(dir));) {

        const size_t length = strlen(ent->d_name);

        if (length <= 8 || strcmp(ent->d_name + length - 8, ".desktop") != 0) {
            continue;
        }

        wm_app_t app;

        if (snprintf(path, sizeof(path), "%s/%s", WM_APPLICATIONS_PATH, ent->d_name) >= (int)sizeof(path) || !wm_app_read(path, &app, exec, sizeof(exec))) {
            continue;
        }

        const wm_app_match_t match = strcmp(exec, id) == 0 ? WM_APP_MATCH_EXEC : strcmp(app.icon, id) == 0 ? WM_APP_MATCH_ICON : WM_APP_MATCH_NONE;

        if (match > best) {
            *out = app;
            best = match;
        }
    }

    if (dir) {
        closedir(dir);
    }

    if (!best) {
        return false;
    }

    wm_app_name(out, id);

    return true;
}


/**
 * @brief Opens a new instance of an application, through the program that runs .desktop files.
 *
 * @param app The application.
 */
void wm_app_launch(const wm_app_t* app) {

    const char* const argv[] = {WM_OPENER_PATH, app->path, NULL};

    wm_spawn(argv);
}


/**
 * @brief Writes the pinned applications to WM_PINS_PATH, one id and .desktop file a line, replacing the file in one rename.
 */
static void wm_pins_save(void) {

    FILE* f = fopen(WM_PINS_PATH ".new", "w");

    if (!f) {
        ulog_warnf("cannot write %s, the taskbar's pins last until the next boot: %s", WM_PINS_PATH, strerror(errno));
        return;
    }

    for (size_t i = 0; i < wm.taskbar.pins_count; i++) {
        fprintf(f, "%s %s\n", wm.taskbar.pins[i].app.id, wm.taskbar.pins[i].app.path);
    }

    if (fclose(f) != 0 || rename(WM_PINS_PATH ".new", WM_PINS_PATH) < 0) {

        ulog_warnf("cannot write %s, the taskbar's pins last until the next boot: %s", WM_PINS_PATH, strerror(errno));

        unlink(WM_PINS_PATH ".new");
    }
}


/**
 * @brief Appends an application to the pinned ones, loading the icon its entry shows while it has no window.
 *
 * @param app The application.
 * @return Its index, or -1 when the list is full.
 */
static int wm_pins_append(const wm_app_t* app) {

    if (wm.taskbar.pins_count >= WM_PINS_MAX) {
        return -1;
    }

    wm_pin_t* pin = &wm.taskbar.pins[wm.taskbar.pins_count];

    memset(pin, 0, sizeof(*pin));

    pin->app  = *app;
    pin->icon = wm_icon_create(app->icon, WM_ICON_SIZE);

    return (int)wm.taskbar.pins_count++;
}


/**
 * @brief Reads the pinned applications from WM_PINS_PATH, dropping any whose .desktop file has gone.
 */
void wm_pins_load(void) {

    FILE* f = fopen(WM_PINS_PATH, "r");

    if (!f) {
        return;
    }

    char line[UI_ICON_NAME_MAX + WM_APP_PATH_MAX + 2];

    while (fgets(line, sizeof(line), f)) {

        line[strcspn(line, "\r\n")] = '\0';

        char* path = strchr(line, ' ');

        if (line[0] == '#' || !path) {
            continue;
        }

        *path++ = '\0';

        wm_app_t app;
        char exec[UI_ICON_NAME_MAX];

        if (wm_pins_find(line) >= 0 || strlen(line) >= sizeof(app.id) || !wm_app_read(path, &app, exec, sizeof(exec))) {
            continue;
        }

        wm_app_name(&app, line);
        wm_pins_append(&app);
    }

    fclose(f);
}


/**
 * @brief Finds a pinned application by the name its windows give.
 *
 * @param id The name.
 * @return Its index, or -1 when it is not pinned.
 */
int wm_pins_find(const char* id) {

    for (size_t i = 0; id && id[0] && i < wm.taskbar.pins_count; i++) {

        if (strcmp(wm.taskbar.pins[i].app.id, id) == 0) {
            return (int)i;
        }
    }

    return -1;
}


/**
 * @brief Pins an application after the ones already pinned, and keeps the list for the next boot.
 *
 * @param app The application.
 * @return Its index, or -1 when the list is full.
 */
int wm_pins_add(const wm_app_t* app) {

    const int found = wm_pins_find(app->id);

    if (found >= 0) {
        return found;
    }

    const int index = wm_pins_append(app);

    if (index < 0) {
        ulog_warnf("cannot pin %s, the taskbar already holds %d pins", app->id, WM_PINS_MAX);
        return -1;
    }

    wm_pins_save();
    wm_taskbar_pins_changed();

    return index;
}


/**
 * @brief Unpins an application, and keeps the list for the next boot.
 *
 * @param index Its index.
 */
void wm_pins_remove(int index) {

    if (index < 0 || (size_t)index >= wm.taskbar.pins_count) {
        return;
    }

    if (wm.taskbar.pins[index].icon) {
        cairo_surface_destroy(wm.taskbar.pins[index].icon);
    }

    memmove(&wm.taskbar.pins[index], &wm.taskbar.pins[index + 1], (wm.taskbar.pins_count - index - 1) * sizeof(wm.taskbar.pins[0]));

    wm.taskbar.pins_count--;

    wm_pins_save();
    wm_taskbar_pins_changed();
}


void wm_pins_fini(void) {

    for (size_t i = 0; i < wm.taskbar.pins_count; i++) {

        if (wm.taskbar.pins[i].icon) {
            cairo_surface_destroy(wm.taskbar.pins[i].icon);
        }
    }

    wm.taskbar.pins_count = 0;
}
