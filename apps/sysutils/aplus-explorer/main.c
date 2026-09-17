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
 * @brief A file browser, built out of libui's widgets.
 *
 * It only reads: nothing here creates, renames or removes anything on the filesystem.
 */

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <aplus/input.h>
#include <aplus/ui-widgets.h>
#include <aplus/ui.h>


#define EXPLORER_WINDOW_WIDTH  560
#define EXPLORER_WINDOW_HEIGHT 460

#define EXPLORER_MARGIN 12
#define EXPLORER_GAP    8

#define EXPLORER_TOOLBAR_HEIGHT 36
#define EXPLORER_STATUS_HEIGHT  20
#define EXPLORER_UP_WIDTH       64

/**
 * @brief How many entries one directory may show, which is what bounds the memory a listing takes.
 */
#define EXPLORER_ENTRIES_MAX 4096

/**
 * @brief How deep a path may be, past which the components at the end are dropped.
 */
#define EXPLORER_DEPTH_MAX 64


/**
 * @brief One directory entry, as the listing needs it rather than as the filesystem reports it.
 */

typedef struct {

    char name[NAME_MAX + 1];

    bool directory;
    off_t size;

} explorer_entry_t;


static struct {

    ui_view_t* view;

    ui_widget_t* toolbar;
    ui_widget_t* up;
    ui_widget_t* path;
    ui_widget_t* list;
    ui_widget_t* status;

    char directory[PATH_MAX];

    explorer_entry_t* entries;
    size_t count;

} explorer = {0};


/**
 * @brief Orders a listing the way a file manager does: directories first, then by name.
 *
 * @param a The first entry.
 * @param b The second entry.
 * @return Less than, equal to or greater than zero.
 */

static int explorer_compare(const void* a, const void* b) {

    const explorer_entry_t* x = (const explorer_entry_t*)a;
    const explorer_entry_t* y = (const explorer_entry_t*)b;

    if (x->directory != y->directory) {
        return x->directory ? -1 : 1;
    }

    return strcmp(x->name, y->name);
}


/**
 * @brief Formats a size the way a listing shows it, in the largest unit that keeps it under 1000.
 *
 * @param size The size in bytes.
 * @param out Receives the text.
 * @param max The size of that buffer.
 */

static void explorer_format_size(off_t size, char* out, size_t max) {

    static const char* const units[] = {"B", "K", "M", "G", "T"};

    double value = (double)size;

    size_t unit = 0;

    while (value >= 1024.0 && unit + 1 < sizeof(units) / sizeof(units[0])) {

        value /= 1024.0;
        unit++;
    }

    if (unit == 0) {
        snprintf(out, max, "%lld B", (long long)size);
    } else {
        snprintf(out, max, "%.1f %s", value, units[unit]);
    }
}


/**
 * @brief Joins a directory and a name into a path, collapsing the separator the root already carries.
 *
 * @param directory The directory.
 * @param name The entry in it.
 * @param out Receives the path.
 * @param max The size of that buffer.
 */

static void explorer_join(const char* directory, const char* name, char* out, size_t max) {

    const size_t length = strlen(directory);

    if (length > 0 && directory[length - 1] == '/') {
        snprintf(out, max, "%s%s", directory, name);
    } else {
        snprintf(out, max, "%s/%s", directory, name);
    }
}


/**
 * @brief Resolves "." and ".." in a path, textually.
 *
 * realpath(3) cannot be used here: musl resolves through /proc/self/fd, which this procfs does not carry.
 *
 * @param path The path to resolve, absolute or relative to the working directory.
 * @param out Receives the result, always absolute.
 * @param max The size of that buffer.
 */

static void explorer_normalize(const char* path, char* out, size_t max) {

    char work[PATH_MAX];

    if (path[0] == '/') {

        strncpy(work, path, sizeof(work) - 1);

        work[sizeof(work) - 1] = '\0';

    } else {

        char cwd[PATH_MAX];

        if (!getcwd(cwd, sizeof(cwd))) {

            cwd[0] = '/';
            cwd[1] = '\0';
        }

        snprintf(work, sizeof(work), "%.*s/%s", (int)(sizeof(work) / 2), cwd, path);
    }


    char* parts[EXPLORER_DEPTH_MAX];

    size_t count = 0;

    for (char* token = strtok(work, "/"); token; token = strtok(NULL, "/")) {

        if (strcmp(token, ".") == 0) {
            continue;
        }

        if (strcmp(token, "..") == 0) {

            if (count > 0) {
                count--;
            }

            continue;
        }

        if (count < EXPLORER_DEPTH_MAX) {
            parts[count++] = token;
        }
    }


    size_t used = 0;

    for (size_t i = 0; i < count; i++) {

        const int written = snprintf(out + used, max - used, "/%s", parts[i]);

        if (written < 0 || (size_t)written >= max - used) {
            break;
        }

        used += (size_t)written;
    }


    if (used == 0 && max > 1) {

        out[0] = '/';
        out[1] = '\0';
    }
}


/**
 * @brief Reads a directory into the entry array, sorted.
 *
 * @param path The directory to read.
 * @return 0 on success, -1 with errno set otherwise.
 */

static int explorer_scan(const char* path) {

    DIR* dir = opendir(path);

    if (!dir) {
        return -1;
    }


    explorer.count = 0;

    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL && explorer.count < EXPLORER_ENTRIES_MAX) {

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }


        explorer_entry_t* out = &explorer.entries[explorer.count];

        strncpy(out->name, entry->d_name, sizeof(out->name) - 1);

        out->name[sizeof(out->name) - 1] = '\0';

        out->directory = entry->d_type == DT_DIR;
        out->size      = 0;


        char full[PATH_MAX];
        struct stat st;

        explorer_join(path, out->name, full, sizeof(full));

        if (stat(full, &st) == 0) {

            out->directory = S_ISDIR(st.st_mode);
            out->size      = st.st_size;

        } else if (entry->d_type == DT_UNKNOWN) {

            out->directory = false;
        }


        explorer.count++;
    }

    closedir(dir);


    qsort(explorer.entries, explorer.count, sizeof(explorer_entry_t), explorer_compare);

    return 0;
}


/**
 * @brief Refreshes the status line from the listing and whatever is selected in it.
 */

static void explorer_update_status(void) {

    char text[PATH_MAX];

    const int selected = ui_list_selected(explorer.list);

    const explorer_entry_t* entry = NULL;

    if (selected > 0 && (size_t)selected <= explorer.count) {
        entry = &explorer.entries[selected - 1];
    }


    if (entry && !entry->directory) {

        char size[32];

        explorer_format_size(entry->size, size, sizeof(size));

        snprintf(text, sizeof(text), "%zu items    %s    %s", explorer.count, entry->name, size);

    } else if (entry) {

        snprintf(text, sizeof(text), "%zu items    %s", explorer.count, entry->name);

    } else {

        snprintf(text, sizeof(text), "%zu items", explorer.count);
    }

    ui_label_set_text(explorer.status, text);
}


/**
 * @brief Shows a directory, keeping the old listing when the new one cannot be read.
 *
 * @param path The directory to show.
 */

static void explorer_open(const char* path) {

    char resolved[PATH_MAX];

    explorer_normalize(path, resolved, sizeof(resolved));


    if (explorer_scan(resolved) < 0) {

        char text[PATH_MAX];

        snprintf(text, sizeof(text), "cannot open %.255s: %s", resolved, strerror(errno));

        ui_label_set_text(explorer.status, text);

        return;
    }


    strncpy(explorer.directory, resolved, sizeof(explorer.directory) - 1);

    explorer.directory[sizeof(explorer.directory) - 1] = '\0';

    ui_label_set_text(explorer.path, explorer.directory);
    ui_window_set_title(ui_view_window(explorer.view), explorer.directory);


    ui_list_clear(explorer.list);

    ui_list_add(explorer.list, "..", "up", NULL);

    for (size_t i = 0; i < explorer.count; i++) {

        char detail[32];

        if (explorer.entries[i].directory) {

            strncpy(detail, "folder", sizeof(detail) - 1);

            detail[sizeof(detail) - 1] = '\0';

        } else {

            explorer_format_size(explorer.entries[i].size, detail, sizeof(detail));
        }

        ui_list_add(explorer.list, explorer.entries[i].name, detail, NULL);
    }


    ui_list_select(explorer.list, explorer.count > 0 ? 1 : 0);

    explorer_update_status();
}


/**
 * @brief Shows the parent of the current directory, which the root has none of.
 */

static void explorer_up(void) {

    char parent[PATH_MAX];

    explorer_join(explorer.directory, "..", parent, sizeof(parent));

    explorer_open(parent);
}


static void explorer_on_up(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    explorer_up();
}


static void explorer_on_select(ui_widget_t* widget, int index, void* user) {

    (void)widget;
    (void)index;
    (void)user;

    explorer_update_status();
}


static void explorer_on_activate(ui_widget_t* widget, int index, void* user) {

    (void)widget;
    (void)user;

    if (index == 0) {

        explorer_up();

        return;
    }


    if (index < 0 || (size_t)index > explorer.count) {
        return;
    }


    const explorer_entry_t* entry = &explorer.entries[index - 1];

    if (!entry->directory) {
        return;
    }


    char path[PATH_MAX];

    explorer_join(explorer.directory, entry->name, path, sizeof(path));

    explorer_open(path);
}


/**
 * @brief The keys the list itself does not take: Backspace goes up, F5 reads the directory again.
 *
 * @param view The view the key arrived at.
 * @param vkey The key code.
 * @param down Whether it went down.
 * @param user Unused.
 * @return Whether the key was handled.
 */
static bool explorer_on_key(ui_view_t* view, uint16_t vkey, bool down, void* user) {

    (void)view;
    (void)user;

    if (!down) {
        return false;
    }

    switch (vkey) {

        case KEY_BACKSPACE:

            explorer_up();

            return true;

        case KEY_F5:

            explorer_open(explorer.directory);

            return true;

        default:
            return false;
    }
}


/**
 * @brief Places the toolbar along the top, the status line along the bottom and the list in between.
 *
 * @param view The view being laid out.
 * @param width The view width in pixels.
 * @param height The view height in pixels.
 * @param user Unused.
 */
static void explorer_layout(ui_view_t* view, int width, int height, void* user) {

    (void)view;
    (void)user;

    const int inner = width - EXPLORER_MARGIN * 2;

    ui_rect_t toolbar = {EXPLORER_MARGIN, EXPLORER_MARGIN, inner, EXPLORER_TOOLBAR_HEIGHT};

    ui_widget_place(explorer.toolbar, toolbar);
    ui_rect_t up = {toolbar.x, toolbar.y, EXPLORER_UP_WIDTH, toolbar.height};

    ui_widget_place(explorer.up, ui_rect_inset(up, 4));

    ui_rect_t path = {toolbar.x + EXPLORER_UP_WIDTH + EXPLORER_GAP, toolbar.y, toolbar.width - EXPLORER_UP_WIDTH - EXPLORER_GAP, toolbar.height};

    ui_widget_place(explorer.path, path);


    const int status_y = height - EXPLORER_MARGIN - EXPLORER_STATUS_HEIGHT;

    ui_rect_t list = {EXPLORER_MARGIN, toolbar.y + toolbar.height + EXPLORER_GAP, inner, status_y - (toolbar.y + toolbar.height + EXPLORER_GAP) - EXPLORER_GAP};

    if (list.height < 0) {
        list.height = 0;
    }

    ui_widget_place(explorer.list, list);

    ui_rect_t status = {EXPLORER_MARGIN, status_y, inner, EXPLORER_STATUS_HEIGHT};

    ui_widget_place(explorer.status, status);
}


/**
 * @brief Creates the widgets, in the order they are painted.
 *
 * @param view The view to create them in.
 * @return 0 on success, -1 otherwise.
 */

static int explorer_build(ui_view_t* view) {

    explorer.toolbar = ui_panel_create(view);
    explorer.up      = ui_button_create(view, "Up", explorer_on_up, NULL);
    explorer.path    = ui_label_create(view, "");
    explorer.list    = ui_list_create(view);
    explorer.status  = ui_label_create(view, "");

    if (!explorer.toolbar || !explorer.up || !explorer.path || !explorer.list || !explorer.status) {
        return -1;
    }


    ui_label_set_color(explorer.path, ui_theme()->text_muted);
    ui_label_set_padding(explorer.path, EXPLORER_GAP);

    ui_label_set_color(explorer.status, ui_theme()->text_muted);

    ui_list_on_select(explorer.list, explorer_on_select, NULL);
    ui_list_on_activate(explorer.list, explorer_on_activate, NULL);

    ui_view_focus(view, explorer.list);

    return 0;
}


int main(int argc, char** argv) {

    explorer.entries = (explorer_entry_t*)calloc(EXPLORER_ENTRIES_MAX, sizeof(explorer_entry_t));

    if (!explorer.entries) {
        fprintf(stderr, "aplus-explorer: calloc() failed: %s\n", strerror(errno));
        return 1;
    }


    ui_connection_t* conn = ui_connect(NULL, 5000);

    if (!conn) {
        fprintf(stderr, "aplus-explorer: ui_connect() failed: %s\n", strerror(errno));
        return 1;
    }


    ui_window_t* window = ui_window_create(conn, EXPLORER_WINDOW_WIDTH, EXPLORER_WINDOW_HEIGHT, "aplus-explorer");

    if (!window) {
        fprintf(stderr, "aplus-explorer: ui_window_create() failed: %s\n", strerror(errno));
        ui_disconnect(conn);
        return 1;
    }


    ui_view_t* view = ui_view_create(window);

    if (!view) {
        fprintf(stderr, "aplus-explorer: ui_view_create() failed: %s\n", strerror(errno));
        ui_window_destroy(window);
        ui_disconnect(conn);
        return 1;
    }

    explorer.view = view;


    if (explorer_build(view) < 0) {
        fprintf(stderr, "aplus-explorer: cannot create the widgets\n");
        ui_view_destroy(view);
        ui_window_destroy(window);
        ui_disconnect(conn);
        return 1;
    }

    ui_view_on_key(view, explorer_on_key, NULL);
    ui_view_on_layout(view, explorer_layout, NULL);

    explorer_open(argc > 1 ? argv[1] : "/");


    const int status = ui_view_run(view);

    if (status < 0) {
        fprintf(stderr, "aplus-explorer: ui_view_run() failed: %s\n", strerror(errno));
    }

    ui_view_destroy(view);
    ui_window_destroy(window);
    ui_disconnect(conn);

    free(explorer.entries);

    return status < 0 ? 1 : 0;
}
