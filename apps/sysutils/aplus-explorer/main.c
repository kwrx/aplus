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
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <aplus/input.h>
#include <aplus/ui-widgets.h>
#include <aplus/ui.h>


#define EXPLORER_WINDOW_WIDTH  720
#define EXPLORER_WINDOW_HEIGHT 460

#define EXPLORER_MARGIN 12
#define EXPLORER_GAP    8

#define EXPLORER_TOOLBAR_HEIGHT 36
#define EXPLORER_STATUS_HEIGHT  20
#define EXPLORER_UP_WIDTH       64

#define EXPLORER_SIDEBAR_WIDTH     148
#define EXPLORER_SIDEBAR_HEADER    32
#define EXPLORER_SIDEBAR_FONT_SIZE 14.0

/**
 * @brief How many entries one directory may show, which is what bounds the memory a listing takes.
 */
#define EXPLORER_ENTRIES_MAX 4096

/**
 * @brief How deep a path may be, past which the components at the end are dropped.
 */
#define EXPLORER_DEPTH_MAX 64

/**
 * @brief How many shortcuts the sidebar holds.
 */
#define EXPLORER_PLACES_MAX 8

/**
 * @brief How long a name read out of a .desktop file may be.
 */
#define EXPLORER_LABEL_MAX 64

/**
 * @brief Where the .desktop files of the installed applications live.
 */
#define EXPLORER_APPLICATIONS_PATH "/usr/share/applications"

/**
 * @brief What opens a file, whatever kind of file it turns out to be.
 */
#define EXPLORER_OPENER "aplus-xopen"

/**
 * @brief How long an icon name read out of a .desktop file may be.
 */
#define EXPLORER_ICON_MAX 128

/**
 * @brief The icons a row falls back on when nothing more specific names one.
 */
#define EXPLORER_ICON_FOLDER     "folder"
#define EXPLORER_ICON_UP         "go-up"
#define EXPLORER_ICON_FILE       "text-x-generic"
#define EXPLORER_ICON_EXECUTABLE "application-x-executable"


/**
 * @brief One directory entry, as the listing needs it rather than as the filesystem reports it.
 */

typedef struct {

    char name[NAME_MAX + 1];
    char label[EXPLORER_LABEL_MAX];

    //? The icon's name, as the theme spells it, or as the .desktop file asked for.
    char icon[EXPLORER_ICON_MAX];

    bool directory;
    off_t size;

} explorer_entry_t;


/**
 * @brief One sidebar shortcut: the name a row shows and the directory it opens.
 */

typedef struct {

    char name[32];
    char path[PATH_MAX];

    const char* icon;

} explorer_place_t;


static struct {

    ui_view_t* view;

    ui_widget_t* toolbar;
    ui_widget_t* up;
    ui_widget_t* path;
    ui_widget_t* sidebar_header;
    ui_widget_t* sidebar;
    ui_widget_t* list;
    ui_widget_t* status;

    char directory[PATH_MAX];

    explorer_entry_t* entries;
    size_t count;

    explorer_place_t places[EXPLORER_PLACES_MAX];
    size_t places_count;

    //? Set while the sidebar selection is being brought in line with the directory on
    //? screen, so that the row it moves does not read as a click and open it all over
    //? again.
    bool syncing;

} explorer = {0};


/**
 * @brief What each extension is shown as, everything else being a plain file or an executable.
 */

static const struct {

    const char* extension;
    const char* icon;

} explorer_types[] = {

    {".desktop", EXPLORER_ICON_EXECUTABLE},

    {".png",     "image-x-generic"       },
    {".jpg",     "image-x-generic"       },
    {".jpeg",    "image-x-generic"       },
    {".bmp",     "image-x-generic"       },
    {".gif",     "image-x-generic"       },
    {".tiff",    "image-x-generic"       },
    {".tif",     "image-x-generic"       },
    {".ico",     "image-x-generic"       },
    {".svg",     "image-x-generic"       },
    {".webp",    "image-x-generic"       },

    {".wav",     "audio-x-generic"       },
    {".mp3",     "audio-x-generic"       },
    {".ogg",     "audio-x-generic"       },
    {".flac",    "audio-x-generic"       },

    {".mp4",     "video-x-generic"       },
    {".avi",     "video-x-generic"       },
    {".mkv",     "video-x-generic"       },
    {".webm",    "video-x-generic"       },
    {".mov",     "video-x-generic"       },

    {".tar",     "package-x-generic"     },
    {".gz",      "package-x-generic"     },
    {".xz",      "package-x-generic"     },
    {".bz2",     "package-x-generic"     },
    {".zip",     "package-x-generic"     },
    {".iso",     "package-x-generic"     },
    {".img",     "package-x-generic"     },

    {".ttf",     "font-x-generic"        },
    {".otf",     "font-x-generic"        },

    {".sh",      "text-x-script"         },
    {".c",       "text-x-script"         },
    {".h",       "text-x-script"         },
    {".cpp",     "text-x-script"         },
    {".hpp",     "text-x-script"         },
    {".js",      "text-x-script"         },
    {".py",      "text-x-script"         },
};


/**
 * @brief Reports the extension of a name, which a name starting with a dot has none of.
 *
 * @param name The file name.
 * @return The extension, dot included, or NULL when there is none.
 */

static const char* explorer_extension(const char* name) {

    const char* dot = strrchr(name, '.');

    return (dot && dot != name) ? dot : NULL;
}


/**
 * @brief Chooses the icon an entry is shown with, out of what it is and what it is called.
 *
 * A .desktop file gets this as its fallback only: what the file itself asks for is read
 * afterwards and wins.
 *
 * @param entry The entry, whose icon is filled in.
 * @param mode The mode stat(2) reported, or 0 when it reported nothing.
 */

static void explorer_icon(explorer_entry_t* entry, mode_t mode) {

    const char* icon = NULL;

    const char* extension = explorer_extension(entry->name);

    if (entry->directory) {

        icon = EXPLORER_ICON_FOLDER;

    } else if (extension) {

        for (size_t i = 0; i < sizeof(explorer_types) / sizeof(explorer_types[0]); i++) {

            if (strcasecmp(extension, explorer_types[i].extension) == 0) {

                icon = explorer_types[i].icon;

                break;
            }
        }
    }

    if (!icon) {
        icon = (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) ? EXPLORER_ICON_EXECUTABLE : EXPLORER_ICON_FILE;
    }

    strncpy(entry->icon, icon, sizeof(entry->icon) - 1);

    entry->icon[sizeof(entry->icon) - 1] = '\0';
}


/**
 * @brief Reports the name a row shows, which is the application name for a .desktop file that carried one.
 *
 * @param entry The entry.
 * @return The name, which is the filename when there is nothing better.
 */

static const char* explorer_label(const explorer_entry_t* entry) {

    return entry->label[0] ? entry->label : entry->name;
}


/**
 * @brief Orders a listing the way a file manager does: directories first, then by the name shown.
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

    return strcmp(explorer_label(x), explorer_label(y));
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
 * @brief Reports the home directory: the environment first, then the password file, then /home.
 *
 * @param out Receives the path.
 * @param max The size of that buffer.
 */

static void explorer_home(char* out, size_t max) {

    const char* home = getenv("HOME");

    if (!home || !*home) {

        const struct passwd* pw = getpwuid(getuid());

        home = (pw && pw->pw_dir && *pw->pw_dir) ? pw->pw_dir : "/home";
    }


    struct stat st;

    if (stat(home, &st) < 0 || !S_ISDIR(st.st_mode)) {
        home = "/home";
    }

    strncpy(out, home, max - 1);

    out[max - 1] = '\0';
}


/**
 * @brief Appends a shortcut to the sidebar, keeping the rows and the shortcut array in step.
 *
 * @param name The name the row shows.
 * @param path The directory the row opens.
 * @param icon The icon the row shows.
 */

static void explorer_add_place(const char* name, const char* path, const char* icon) {

    if (explorer.places_count >= EXPLORER_PLACES_MAX) {
        return;
    }


    explorer_place_t* place = &explorer.places[explorer.places_count];

    strncpy(place->name, name, sizeof(place->name) - 1);

    place->name[sizeof(place->name) - 1] = '\0';

    explorer_normalize(path, place->path, sizeof(place->path));

    place->icon = icon;


    if (ui_list_add_icon(explorer.sidebar, place->icon, place->name, NULL, NULL) < 0) {
        return;
    }

    explorer.places_count++;
}


/**
 * @brief Fills the sidebar with the directories that are worth one click.
 */

static void explorer_build_places(void) {

    char home[PATH_MAX];

    explorer_home(home, sizeof(home));

    explorer_add_place("Root", "/", "drive-harddisk");
    explorer_add_place("Home", home, "user-home");
    explorer_add_place("Applications", EXPLORER_APPLICATIONS_PATH, "applications-system");
}


/**
 * @brief Highlights the shortcut the current directory is, and no row at all when it is none of them.
 */

static void explorer_sync_places(void) {

    int index = -1;

    for (size_t i = 0; i < explorer.places_count; i++) {

        if (strcmp(explorer.places[i].path, explorer.directory) == 0) {

            index = (int)i;

            break;
        }
    }


    explorer.syncing = true;

    ui_list_select(explorer.sidebar, index);

    explorer.syncing = false;
}


/**
 * @brief Matches an unlocalised key at the head of a .desktop line and reports where its value starts.
 *
 * @param text The line, already trimmed.
 * @param key The key to match.
 * @return The value, or NULL when the line carries another key.
 */

static const char* explorer_desktop_value(const char* text, const char* key) {

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
 * @brief Reads the application name and the icon out of a .desktop file.
 *
 * Only the [Desktop Entry] group counts, and only its unlocalised keys: Name[xx] and every other group are skipped.
 *
 * @param path The file to read.
 * @param name Receives the name, left empty when the file carries none.
 * @param name_max The size of that buffer.
 * @param icon Receives the icon, left as it was when the file names none.
 * @param icon_max The size of that buffer.
 * @return true when a name was read, false otherwise.
 */

static bool explorer_desktop_read(const char* path, char* name, size_t name_max, char* icon, size_t icon_max) {

    name[0] = '\0';

    FILE* file = fopen(path, "r");

    if (!file) {
        return false;
    }


    char line[512];

    bool group = false;

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

            continue;
        }

        if (!group) {
            continue;
        }


        const char* value;

        if ((value = explorer_desktop_value(text, "Name")) != NULL) {

            strncpy(name, value, name_max - 1);

            name[name_max - 1] = '\0';

        } else if ((value = explorer_desktop_value(text, "Icon")) != NULL && *value) {

            strncpy(icon, value, icon_max - 1);

            icon[icon_max - 1] = '\0';
        }
    }

    fclose(file);

    return name[0] != '\0';
}


/**
 * @brief Hands a path to the opener, first collecting the children that have since exited.
 *
 * The opener replaces itself with the application, so what is spawned here is the application.
 *
 * @param path The file to open.
 * @return 0 on success, -1 with errno set otherwise.
 */

static int explorer_open_with(const char* path) {

    char* argv[] = {(char*)EXPLORER_OPENER, (char*)path, NULL};

    while (waitpid(-1, NULL, WNOHANG) > 0) {
        ;
    }


    const pid_t pid = fork();

    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {

        for (int fd = STDERR_FILENO + 1; fd < CONFIG_OPEN_MAX; fd++) {
            close(fd);
        }

        execvp(argv[0], argv);

        _exit(127);
    }

    return 0;
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

        mode_t mode = 0;

        if (stat(full, &st) == 0) {

            out->directory = S_ISDIR(st.st_mode);
            out->size      = st.st_size;
            mode           = st.st_mode;

        } else if (entry->d_type == DT_UNKNOWN) {

            out->directory = false;
        }


        const char* extension = explorer_extension(out->name);

        out->label[0] = '\0';

        explorer_icon(out, mode);

        if (!out->directory && extension && strcasecmp(extension, ".desktop") == 0) {
            explorer_desktop_read(full, out->label, sizeof(out->label), out->icon, sizeof(out->icon));
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

        snprintf(text, sizeof(text), "%zu items    %s    %s", explorer.count, explorer_label(entry), size);

    } else if (entry) {

        snprintf(text, sizeof(text), "%zu items    %s", explorer.count, explorer_label(entry));

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

        explorer_sync_places();

        return;
    }


    strncpy(explorer.directory, resolved, sizeof(explorer.directory) - 1);

    explorer.directory[sizeof(explorer.directory) - 1] = '\0';

    ui_label_set_text(explorer.path, explorer.directory);
    ui_window_set_title(ui_view_window(explorer.view), explorer.directory);

    explorer_sync_places();


    ui_list_clear(explorer.list);

    ui_list_add_icon(explorer.list, EXPLORER_ICON_UP, "..", "up", NULL);

    for (size_t i = 0; i < explorer.count; i++) {

        char detail[32];

        if (explorer.entries[i].directory) {

            strncpy(detail, "folder", sizeof(detail) - 1);

            detail[sizeof(detail) - 1] = '\0';

        } else {

            explorer_format_size(explorer.entries[i].size, detail, sizeof(detail));
        }

        ui_list_add_icon(explorer.list, explorer.entries[i].icon, explorer_label(&explorer.entries[i]), detail, NULL);
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


/**
 * @brief Opens a shortcut, which one click on its row is enough to do.
 *
 * @param widget The sidebar.
 * @param index The row, or -1 when the selection was cleared.
 * @param user Unused.
 */

static void explorer_on_place(ui_widget_t* widget, int index, void* user) {

    (void)widget;
    (void)user;

    if (explorer.syncing) {
        return;
    }

    if (index < 0 || (size_t)index >= explorer.places_count) {
        return;
    }

    explorer_open(explorer.places[index].path);
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

    char path[PATH_MAX];

    explorer_join(explorer.directory, entry->name, path, sizeof(path));

    if (entry->directory) {

        explorer_open(path);

        return;
    }


    char text[PATH_MAX];

    if (explorer_open_with(path) < 0) {
        snprintf(text, sizeof(text), "cannot open %.255s: %s", explorer_label(entry), strerror(errno));
    } else {
        snprintf(text, sizeof(text), "opening %.255s", explorer_label(entry));
    }

    ui_label_set_text(explorer.status, text);
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
 * @brief Places the toolbar along the top, the status line along the bottom, and the sidebar and the listing in between.
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

    const int content_y = toolbar.y + toolbar.height + EXPLORER_GAP;

    int content_height = status_y - content_y - EXPLORER_GAP;

    if (content_height < 0) {
        content_height = 0;
    }


    int sidebar_width = EXPLORER_SIDEBAR_WIDTH;

    if (sidebar_width > inner / 2) {
        sidebar_width = inner / 2;
    }

    if (sidebar_width < 0) {
        sidebar_width = 0;
    }


    ui_rect_t header = {EXPLORER_MARGIN + EXPLORER_GAP, content_y, sidebar_width - EXPLORER_GAP * 2, EXPLORER_SIDEBAR_HEADER};

    if (header.width < 0) {
        header.width = 0;
    }

    ui_widget_place(explorer.sidebar_header, header);

    ui_rect_t sidebar = {EXPLORER_MARGIN, content_y + EXPLORER_SIDEBAR_HEADER, sidebar_width, content_height - EXPLORER_SIDEBAR_HEADER};

    if (sidebar.height < 0) {
        sidebar.height = 0;
    }

    ui_widget_place(explorer.sidebar, sidebar);


    ui_rect_t list = {EXPLORER_MARGIN + sidebar_width + EXPLORER_GAP, content_y, inner - sidebar_width - EXPLORER_GAP, content_height};

    if (list.width < 0) {
        list.width = 0;
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

    explorer.toolbar        = ui_panel_create(view);
    explorer.up             = ui_button_create(view, "Up", explorer_on_up, NULL);
    explorer.path           = ui_label_create(view, "");
    explorer.sidebar_header = ui_label_create(view, "Places");
    explorer.sidebar        = ui_list_create(view);
    explorer.list           = ui_list_create(view);
    explorer.status         = ui_label_create(view, "");

    if (!explorer.toolbar || !explorer.up || !explorer.path || !explorer.sidebar_header || !explorer.sidebar || !explorer.list || !explorer.status) {
        return -1;
    }


    ui_label_set_color(explorer.path, ui_theme()->text_muted);
    ui_label_set_padding(explorer.path, EXPLORER_GAP);

    ui_label_set_color(explorer.sidebar_header, ui_theme()->text_muted);
    ui_label_set_font(explorer.sidebar_header, UI_FONT_REGULAR, EXPLORER_SIDEBAR_FONT_SIZE);

    ui_list_on_select(explorer.sidebar, explorer_on_place, NULL);

    ui_label_set_color(explorer.status, ui_theme()->text_muted);

    ui_list_on_select(explorer.list, explorer_on_select, NULL);
    ui_list_on_activate(explorer.list, explorer_on_activate, NULL);

    explorer_build_places();

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
