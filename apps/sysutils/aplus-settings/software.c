/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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
 * @brief The Software page: the packages pkg knows of, installed or not, and the jobs it runs on them.
 *
 * pkg runs as a child whose output is read a little every pass, so the window stays live while it works.
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

#include "settings.h"


#define SOFTWARE_PKG        "/usr/bin/pkg"
#define SOFTWARE_MAX        512
#define SOFTWARE_NAME_MAX   64
#define SOFTWARE_VERSION    32
#define SOFTWARE_POLL_MS    100
#define SOFTWARE_OUTPUT_MAX (256 * 1024)
#define SOFTWARE_ICON       "package-x-generic"
#define SOFTWARE_ARROW      "\xE2\x86\x92"


/**
 * @brief One package, as pkg list and pkg search describe it.
 */
typedef struct {

    char name[SOFTWARE_NAME_MAX];
    char version[SOFTWARE_VERSION];
    char latest[SOFTWARE_VERSION];

    bool installed;
    bool dependency;

} software_package_t;


/**
 * @brief What a finished job hands its output to.
 */
typedef void (*software_done_fn)(int status, const char* output);


/**
 * @brief The page's widgets, the packages, the rows the filter lets through, and the job running.
 */
static struct {

    settings_header_t header;

    ui_widget_t* check;
    ui_widget_t* upgrade;
    ui_widget_t* search;
    ui_widget_t* list;
    ui_widget_t* status;
    ui_widget_t* remove;
    ui_widget_t* install;

    software_package_t packages[SOFTWARE_MAX];
    size_t count;

    int rows[SOFTWARE_MAX];
    size_t row_count;

    char selected[SOFTWARE_NAME_MAX];

    bool loaded;
    bool indexed;

    struct {

        pid_t pid;
        int fd;

        char* output;
        size_t length;

        software_done_fn done;
        bool verbose;

    } job;

} software;


/**
 * @brief Shows a line at the foot of the page.
 *
 * @param text The line.
 */
static void software_report(const char* text) {
    settings_fit(software.status, text);
}


/**
 * @brief Reports the last line a job printed, which is what pkg leaves its reason in.
 *
 * @param output The job's output.
 * @param out Receives the line.
 * @param size The size of @p out.
 */
static void software_last_line(const char* output, char* out, size_t size) {

    size_t end = strlen(output);

    while (end > 0 && isspace((unsigned char)output[end - 1])) {
        end--;
    }

    size_t start = end;

    while (start > 0 && output[start - 1] != '\n' && output[start - 1] != '\r') {
        start--;
    }

    snprintf(out, size, "%.*s", (int)(end - start), output + start);
}


/**
 * @brief Finds a package by name.
 *
 * @param name The name.
 * @return The package, or NULL.
 */
static software_package_t* software_find(const char* name) {

    for (size_t i = 0; i < software.count; i++) {

        if (strcmp(software.packages[i].name, name) == 0) {
            return &software.packages[i];
        }
    }

    return NULL;
}


/**
 * @brief Finds a package by name, adding it when it is new.
 *
 * @param name The name.
 * @return The package, or NULL when the table is full.
 */
static software_package_t* software_add(const char* name) {

    software_package_t* package = software_find(name);

    if (package || software.count == SOFTWARE_MAX) {
        return package;
    }

    package = &software.packages[software.count++];

    memset(package, 0, sizeof(*package));
    snprintf(package->name, sizeof(package->name), "%s", name);

    return package;
}


/**
 * @brief Reports whether a package has a newer version in the index than the one installed.
 *
 * @param package The package.
 * @return true when it can be updated.
 */
static bool software_upgradable(const software_package_t* package) {
    return package->installed && package->latest[0] && strcmp(package->latest, package->version) != 0;
}


/**
 * @brief Orders two packages by name regardless of case, for qsort().
 *
 * @param a The first.
 * @param b The second.
 * @return Less than, equal to or greater than 0, as strcasecmp().
 */
static int software_compare(const void* a, const void* b) {
    return strcasecmp(((const software_package_t*)a)->name, ((const software_package_t*)b)->name);
}


/**
 * @brief Matches a package against the search field, ignoring case.
 *
 * @param package The package.
 * @param filter The field's text.
 * @return true when the name contains it.
 */
static bool software_matches(const software_package_t* package, const char* filter) {

    const size_t length = strlen(filter);

    for (const char* p = package->name; *p; p++) {

        if (strncasecmp(p, filter, length) == 0) {
            return true;
        }
    }

    return length == 0;
}


/**
 * @brief Enables the buttons for what can be done to the selected package, and to every package.
 */
static void software_update_buttons(void) {

    const bool idle                   = software.job.pid == 0;
    const software_package_t* package = software.selected[0] ? software_find(software.selected) : NULL;

    bool any = false;

    for (size_t i = 0; i < software.count; i++) {
        any |= software_upgradable(&software.packages[i]);
    }

    ui_widget_set_enabled(software.check, idle);
    ui_widget_set_enabled(software.upgrade, idle && any);
    ui_widget_set_enabled(software.remove, idle && package && package->installed);

    if (package && software_upgradable(package)) {
        ui_button_set_text(software.install, "Update");
        ui_widget_set_enabled(software.install, idle);
    } else if (package && package->installed) {
        ui_button_set_text(software.install, "Installed");
        ui_widget_set_enabled(software.install, false);
    } else {
        ui_button_set_text(software.install, "Install");
        ui_widget_set_enabled(software.install, idle && package);
    }
}


/**
 * @brief Fills the list with the packages the search field lets through, keeping the selection.
 */
static void software_fill(void) {

    const char* filter = ui_entry_text(software.search);

    ui_list_clear(software.list);

    software.row_count = 0;

    int selected = -1;

    for (size_t i = 0; i < software.count; i++) {

        const software_package_t* package = &software.packages[i];

        if (!software_matches(package, filter)) {
            continue;
        }

        char detail[SETTINGS_TEXT_MAX];

        if (software_upgradable(package)) {
            snprintf(detail, sizeof(detail), "%s " SOFTWARE_ARROW " %s", package->version, package->latest);
        } else if (package->installed) {
            snprintf(detail, sizeof(detail), "%s, %s", package->version, package->dependency ? "dependency" : "installed");
        } else {
            snprintf(detail, sizeof(detail), "%s", package->version);
        }

        if (ui_list_add_icon(software.list, SOFTWARE_ICON, package->name, detail, NULL) < 0) {
            break;
        }

        if (strcmp(package->name, software.selected) == 0) {
            selected = (int)software.row_count;
        }

        software.rows[software.row_count++] = (int)i;
    }

    ui_list_select(software.list, selected);

    software_update_buttons();
}


/**
 * @brief Starts pkg with some arguments, its output read a little every pass until it exits.
 *
 * @param argv The arguments, argv[0] included, NULL terminated.
 * @param done What the output is handed to once pkg exits.
 * @param verbose Whether to show each line it prints at the foot of the page.
 * @return 0, or -1 when pkg cannot be started.
 */
static int software_spawn(char* const argv[], software_done_fn done, bool verbose) {

    int fds[2];

    if (software.job.pid != 0 || pipe(fds) < 0) {
        return -1;
    }

    const pid_t pid = fork();

    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }

    if (pid == 0) {

        const int null = open("/dev/null", O_RDONLY);

        if (null >= 0) {
            dup2(null, STDIN_FILENO);
        }

        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);

        for (int fd = 3; fd < 256; fd++) {
            close(fd);
        }

        execv(SOFTWARE_PKG, argv);
        _exit(127);
    }

    close(fds[1]);

    fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | O_NONBLOCK);

    software.job.pid     = pid;
    software.job.fd      = fds[0];
    software.job.length  = 0;
    software.job.done    = done;
    software.job.verbose = verbose;

    if (software.job.output) {
        software.job.output[0] = '\0';
    }

    software_update_buttons();

    return 0;
}


/**
 * @brief Reads whatever the job printed since the last pass, showing its last line.
 *
 * @return true when the job closed its output.
 */
static bool software_drain(void) {

    if (!software.job.output && (software.job.output = malloc(SOFTWARE_OUTPUT_MAX)) == NULL) {
        return false;
    }

    bool closed = false;
    bool got    = false;

    for (;;) {

        char chunk[1024];

        const ssize_t n = read(software.job.fd, chunk, sizeof(chunk));

        if (n == 0) {
            closed = true;
            break;
        }

        if (n < 0) {
            break;
        }

        const size_t room = SOFTWARE_OUTPUT_MAX - 1 - software.job.length;
        const size_t take = (size_t)n < room ? (size_t)n : room;

        memcpy(software.job.output + software.job.length, chunk, take);

        software.job.length += take;
        got = true;
    }

    software.job.output[software.job.length] = '\0';

    if (got && software.job.verbose) {

        char line[SETTINGS_TEXT_MAX];

        software_last_line(software.job.output, line, sizeof(line));

        if (line[0]) {
            software_report(line);
        }
    }

    return closed;
}


/**
 * @brief Reads pkg search, adding every package in the index to the table, then shows the table.
 *
 * @param status pkg's exit status.
 * @param output What it printed.
 */
static void software_on_search(int status, const char* output) {

    software.indexed = status == 0;

    if (software.indexed) {

        for (const char* line = output; *line;) {

            char name[SOFTWARE_NAME_MAX * 2];
            char version[SOFTWARE_VERSION];

            if (sscanf(line, "%127s %31s", name, version) == 2) {

                const char* slash = strchr(name, '/');

                software_package_t* package = software_add(slash ? slash + 1 : name);

                if (package) {

                    snprintf(package->latest, sizeof(package->latest), "%s", version);

                    if (!package->installed) {
                        snprintf(package->version, sizeof(package->version), "%s", version);
                    }
                }
            }

            const char* next = strchr(line, '\n');
            line             = next ? next + 1 : line + strlen(line);
        }
    }

    qsort(software.packages, software.count, sizeof(software.packages[0]), software_compare);

    software.loaded = true;

    size_t installed = 0;

    for (size_t i = 0; i < software.count; i++) {
        installed += software.packages[i].installed;
    }

    char text[SETTINGS_TEXT_MAX];

    if (software.indexed) {
        snprintf(text, sizeof(text), "%zu packages, %zu installed.", software.count, installed);
    } else {
        snprintf(text, sizeof(text), "%zu installed. Check for updates to see what else there is.", installed);
    }

    software_report(text);
    software_fill();
}


/**
 * @brief Reads pkg list into a fresh table of installed packages, then asks for the index.
 *
 * @param status pkg's exit status.
 * @param output What it printed.
 */
static void software_on_list(int status, const char* output) {

    software.count = 0;

    if (status == 127) {
        software_report("The package manager is not installed.");
        software_fill();
        return;
    }

    for (const char* line = output; status == 0 && *line;) {

        char name[SOFTWARE_NAME_MAX];
        char version[SOFTWARE_VERSION];

        const char* next = strchr(line, '\n');
        const size_t end = next ? (size_t)(next - line) : strlen(line);

        if (sscanf(line, "%63s %31s", name, version) == 2) {

            software_package_t* package = software_add(name);

            if (package) {

                char rest[256];

                snprintf(rest, sizeof(rest), "%.*s", (int)end, line);

                snprintf(package->version, sizeof(package->version), "%s", version);

                package->installed  = true;
                package->dependency = strstr(rest, "[dependency]") != NULL;
            }
        }

        line = next ? next + 1 : line + end;
    }

    static char* const argv[] = {"pkg", "search", NULL};

    if (software_spawn(argv, software_on_search, false) < 0) {
        software_on_search(1, "");
    }
}


/**
 * @brief Reads the table afresh: what is installed, then what the index offers.
 */
static void software_refresh(void) {

    static char* const argv[] = {"pkg", "list", NULL};

    if (software_spawn(argv, software_on_list, false) < 0) {
        software_report("Cannot start the package manager.");
    }
}


/**
 * @brief Reports how a job that changes the system went, and reads the table again.
 *
 * @param status pkg's exit status.
 * @param output What it printed.
 */
static void software_on_action(int status, const char* output) {

    if (status != 0) {

        char line[SETTINGS_TEXT_MAX];

        software_last_line(output, line, sizeof(line));
        software_report(line[0] ? line : "The package manager failed.");

        software_update_buttons();
        return;
    }

    software_refresh();
}


/**
 * @brief Runs pkg on the selected package, or on every package when there is no name.
 *
 * @param command The pkg command.
 * @param name The package, or NULL.
 */
static void software_run(const char* command, const char* name) {

    char* argv[] = {"pkg", "-y", (char*)command, (char*)name, NULL};

    char text[SETTINGS_TEXT_MAX];

    if (software_spawn(argv, software_on_action, true) < 0) {
        software_report("Cannot start the package manager.");
        return;
    }

    snprintf(text, sizeof(text),
             "Running pkg %s%s%s"
             "\xE2\x80\xA6",
             command, name ? " " : "", name ? name : "");
    software_report(text);
}


/**
 * @brief Installs or updates the selected package.
 *
 * @param widget The button.
 * @param user Unused.
 */
static void software_on_install(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    const software_package_t* package = software_find(software.selected);

    if (package) {
        software_run(software_upgradable(package) ? "upgrade" : "install", package->name);
    }
}


/**
 * @brief Removes the selected package.
 *
 * @param widget The button.
 * @param user Unused.
 */
static void software_on_remove(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    if (software.selected[0]) {
        software_run("remove", software.selected);
    }
}


/**
 * @brief Updates every installed package that has a newer version.
 *
 * @param widget The button.
 * @param user Unused.
 */
static void software_on_upgrade(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    software_run("upgrade", NULL);
}


/**
 * @brief Fetches the index again, then reads the table.
 *
 * @param widget The button.
 * @param user Unused.
 */
static void software_on_check(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    static char* const argv[] = {"pkg", "update", NULL};

    if (software_spawn(argv, software_on_action, true) < 0) {
        software_report("Cannot start the package manager.");
        return;
    }

    software_report("Fetching the package index\xE2\x80\xA6");
}


/**
 * @brief Remembers the package a row names, and what can be done to it.
 *
 * @param widget The list.
 * @param index The row, or -1.
 * @param user Unused.
 */
static void software_on_select(ui_widget_t* widget, int index, void* user) {

    (void)widget;
    (void)user;

    if (index >= 0 && (size_t)index < software.row_count) {
        snprintf(software.selected, sizeof(software.selected), "%s", software.packages[software.rows[index]].name);
    } else {
        software.selected[0] = '\0';
    }

    software_update_buttons();
}


/**
 * @brief Filters the list as the search field changes.
 *
 * @param widget The field.
 * @param user Unused.
 */
static void software_on_search_change(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    software_fill();
}


/**
 * @brief Creates the page's widgets.
 *
 * @return 0 on success, -1 otherwise.
 */
static int software_build(void) {

    if (settings_header_build(&software.header, "Software", "Packages from the aplus repository, installed or ready to be.") < 0) {
        return -1;
    }

    software.check   = settings_own(ui_button_create(settings.view, "Check for updates", software_on_check, NULL));
    software.upgrade = settings_own(ui_button_create(settings.view, "Update all", software_on_upgrade, NULL));
    software.search  = settings_own(ui_entry_create(settings.view, "Search packages"));
    software.list    = settings_own(ui_list_create(settings.view));
    software.status  = settings_own(ui_label_create(settings.view, ""));
    software.remove  = settings_own(ui_button_create(settings.view, "Remove", software_on_remove, NULL));
    software.install = settings_own(ui_button_create(settings.view, "Install", software_on_install, NULL));

    if (!software.check || !software.upgrade || !software.search || !software.list || !software.status || !software.remove || !software.install) {
        return -1;
    }

    ui_button_set_style(software.remove, UI_BUTTON_STYLE_DANGER);
    ui_button_set_style(software.install, UI_BUTTON_STYLE_PRIMARY);

    ui_entry_on_change(software.search, software_on_search_change, NULL);
    ui_list_on_select(software.list, software_on_select, NULL);

    software_update_buttons();

    return 0;
}


/**
 * @brief Lays out the buttons beside the title, the search field, the list, and the foot.
 *
 * @param page The page's rectangle.
 */
static void software_layout(ui_rect_t page) {

    const int control = settings_control_height();
    const int right   = page.x + page.width;

    const int check   = settings_text_width("Check for updates", UI_FONT_REGULAR) + 2 * SETTINGS_CARD_PADDING;
    const int upgrade = settings_text_width("Update all", UI_FONT_REGULAR) + 2 * SETTINGS_CARD_PADDING;

    const int top = settings_header_place(&software.header, page, check + upgrade + 2 * SETTINGS_GAP);

    ui_widget_set_rect(software.check, right - check, page.y + 4, check, control);
    ui_widget_set_rect(software.upgrade, right - check - SETTINGS_GAP - upgrade, page.y + 4, upgrade, control);

    ui_widget_set_rect(software.search, page.x, top, page.width, control + 4);


    const int foot   = page.y + page.height - control;
    const int remove = settings_text_width("Remove", UI_FONT_REGULAR) + 2 * SETTINGS_CARD_PADDING;
    const int action = settings_text_width("Installed", UI_FONT_REGULAR) + 2 * SETTINGS_CARD_PADDING;

    ui_widget_set_rect(software.install, right - action, foot, action, control);
    ui_widget_set_rect(software.remove, right - action - SETTINGS_GAP - remove, foot, remove, control);
    ui_widget_set_rect(software.status, page.x, foot, page.width - action - remove - 3 * SETTINGS_GAP, control);

    const int list = top + control + 4 + SETTINGS_GAP;

    ui_widget_set_rect(software.list, page.x, list, page.width, foot - SETTINGS_GAP - list > 0 ? foot - SETTINGS_GAP - list : 0);
}


/**
 * @brief Gives the page the colours of the theme in use.
 */
static void software_restyle(void) {

    settings_header_restyle(&software.header);

    ui_label_set_color(software.status, ui_theme()->text_muted);
}


/**
 * @brief Reads the table the first time the page is shown.
 */
static void software_enter(void) {

    if (!software.loaded && software.job.pid == 0) {
        software_report("Reading the installed packages\xE2\x80\xA6");
        software_refresh();
    }
}


/**
 * @brief Reads the running job's output, and hands it on once pkg has exited, whether or not the page is on show.
 *
 * @param now Unused.
 * @param visible Unused.
 * @return SOFTWARE_POLL_MS while a job runs, -1 otherwise.
 */
static int software_tick(uint64_t now, bool visible) {

    (void)now;
    (void)visible;

    if (software.job.pid == 0) {
        return -1;
    }

    software_drain();

    int status;

    const pid_t e = waitpid(software.job.pid, &status, WNOHANG);

    if (e == 0) {
        return SOFTWARE_POLL_MS;
    }

    software_drain();

    close(software.job.fd);

    const software_done_fn done = software.job.done;

    software.job.pid = 0;
    software.job.fd  = -1;

    const int code = e < 0 ? 1 : WIFEXITED(status) ? WEXITSTATUS(status) : 1;

    done(code, software.job.output ? software.job.output : "");

    software_update_buttons();

    return software.job.pid ? 0 : -1;
}


/**
 * @brief The Software page.
 */
const settings_page_t settings_software = {
    .name    = "software",
    .label   = "Software",
    .icon    = SOFTWARE_ICON,
    .build   = software_build,
    .layout  = software_layout,
    .restyle = software_restyle,
    .enter   = software_enter,
    .tick    = software_tick,
};
