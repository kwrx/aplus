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
 * @brief A search field over the installed applications, summoned with Super+Space.
 *
 * The window is borderless, centred and translucent, and it is only ever as tall as it has
 * rows to show. It is gone the moment it is used: whatever is picked replaces this process
 * rather than being forked from it, so aplus-wm, which spawned the launcher, ends up with
 * the application as its child and reaps it as usual.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <aplus/input.h>
#include <aplus/ui-widgets.h>
#include <aplus/ui.h>

#include "desktop.h"


/**
 * @brief What the messages call this program.
 */
#define LAUNCHER_NAME "aplus-launcher"

/**
 * @brief Where the applications describe themselves.
 */
#define LAUNCHER_APPLICATIONS "/usr/share/applications"

/**
 * @brief What turns a .desktop file into a running application.
 */
#define LAUNCHER_OPENER "aplus-xopen"

/**
 * @brief What runs a command that matched no application.
 */
#define LAUNCHER_SHELL "/bin/dash"

/**
 * @brief What a row shows when its .desktop file named no icon, and what the shell row shows.
 */
#define LAUNCHER_ICON_DEFAULT "application-x-executable"
#define LAUNCHER_ICON_SHELL   "system-run"

/**
 * @brief How long to keep trying to reach the display server.
 */
#define LAUNCHER_CONNECT_MS 5000


#define LAUNCHER_WINDOW_WIDTH 640

#define LAUNCHER_MARGIN 0
#define LAUNCHER_FIELD  48
#define LAUNCHER_ROW    32

/**
 * @brief How many rows the window grows to before the list starts scrolling instead.
 */
#define LAUNCHER_ROWS_MAX 5

/**
 * @brief How many events are dispatched between frames, so that a burst costs one repaint.
 */
#define LAUNCHER_DRAIN_MAX 64

/**
 * @brief What a row carries instead of an entry when it stands for the query itself.
 */
#define LAUNCHER_RUN_ROW ((void*)-1)


static struct {

    ui_connection_t* conn;
    ui_window_t* window;
    ui_view_t* view;

    ui_widget_t* backdrop;
    ui_widget_t* field;
    ui_widget_t* list;

    launcher_entry_t* entries;
    size_t count;

    //? Set by anything that should close the launcher, and acted on by the event loop
    //? rather than where it happened, so that nothing exits from inside a callback.
    bool dismissed;

} launcher = {0};


/**
 * @brief The dark theme with the light let through it: a backdrop that paints nothing, and recesses that only tint.
 *
 * The panel is what the window looks like, so the view is left with nothing of its own to
 * fill -- anything it painted would sit under the panel and darken it for no gain.
 */
static ui_theme_t launcher_theme;


/**
 * @brief Fills in the theme, which cannot be a file-scope initialiser since it starts from another one.
 */

static void launcher_theme_init(void) {

    launcher_theme = *ui_theme_dark();

    launcher_theme.background     = ui_rgba(0x00, 0x00, 0x00, 0.0);
    launcher_theme.surface_sunken = ui_rgba(0x0E, 0x0E, 0x0E, 0.35);
}


/**
 * @brief Reports the window height that shows a number of rows and no part of another.
 *
 * No rows is the field on its own, which is what an untouched launcher looks like. The list
 * insets its rows by a pixel at the top and at the bottom, which is the 2 below.
 *
 * @param rows How many rows to fit, clamped to at most LAUNCHER_ROWS_MAX.
 * @return The height in pixels.
 */

static int launcher_height(int rows) {

    if (rows < 1) {
        return LAUNCHER_FIELD + 2 * LAUNCHER_MARGIN;
    }

    if (rows > LAUNCHER_ROWS_MAX) {
        rows = LAUNCHER_ROWS_MAX;
    }

    return LAUNCHER_FIELD + 3 * LAUNCHER_MARGIN + rows * LAUNCHER_ROW + 2;
}


/**
 * @brief Reports whether an entry answers to a query, which nothing does until a character is typed.
 *
 * @param entry The entry to test.
 * @param query What was typed.
 * @return true when the entry should be listed.
 */

static bool launcher_matches(const launcher_entry_t* entry, const char* query) {

    if (!*query) {
        return false;
    }

    return strcasestr(entry->name, query) != NULL || strcasestr(entry->comment, query) != NULL || strcasestr(entry->exec, query) != NULL;
}


/**
 * @brief Rebuilds the list from what is in the field, and asks for the height that now fits it.
 *
 * An empty field lists nothing and leaves the window the height of the field alone. A query
 * that matches nothing is offered to the shell instead, which is the whole of what this has
 * that a menu does not.
 */

static void launcher_refilter(void) {

    const char* query = ui_entry_text(launcher.field);

    ui_list_clear(launcher.list);

    for (size_t i = 0; i < launcher.count; i++) {

        if (launcher_matches(&launcher.entries[i], query)) {

            const launcher_entry_t* entry = &launcher.entries[i];

            ui_list_add_icon(launcher.list, entry->icon[0] ? entry->icon : LAUNCHER_ICON_DEFAULT, entry->name, entry->comment, (void*)entry);
        }
    }

    if (ui_list_count(launcher.list) == 0 && *query) {

        char row[LAUNCHER_FIELD_MAX + 8];

        snprintf(row, sizeof(row), "Run \"%s\"", query);

        ui_list_add_icon(launcher.list, LAUNCHER_ICON_SHELL, row, "shell", LAUNCHER_RUN_ROW);
    }

    const size_t rows = ui_list_count(launcher.list);

    ui_list_select(launcher.list, 0);
    ui_widget_set_visible(launcher.list, rows > 0);

    ui_window_request_size(launcher.window, LAUNCHER_WINDOW_WIDTH, launcher_height((int)rows));
}


/**
 * @brief Gives back everything held, in the order the library requires, before an exec or an exit.
 */

static void launcher_teardown(void) {

    ui_view_destroy(launcher.view);
    ui_window_destroy(launcher.window);
    ui_disconnect(launcher.conn);

    launcher.view   = NULL;
    launcher.window = NULL;
    launcher.conn   = NULL;

    free(launcher.entries);

    launcher.entries = NULL;
    launcher.count   = 0;
}


/**
 * @brief Runs what the list has selected, replacing this process with it.
 *
 * Returns only when the exec failed, having already given the window back.
 */

static void launcher_run(void) {

    const int index = ui_list_selected(launcher.list);

    if (index < 0) {
        return;
    }


    const void* row = ui_list_item_user(launcher.list, index);

    char command[PATH_MAX];

    if (row == LAUNCHER_RUN_ROW) {
        strncpy(command, ui_entry_text(launcher.field), sizeof(command) - 1);
    } else {
        strncpy(command, ((const launcher_entry_t*)row)->path, sizeof(command) - 1);
    }

    command[sizeof(command) - 1] = '\0';


    const bool shell = row == LAUNCHER_RUN_ROW;

    launcher_teardown();

    for (int fd = STDERR_FILENO + 1; fd < CONFIG_OPEN_MAX; fd++) {
        close(fd);
    }

    if (shell) {
        execl(LAUNCHER_SHELL, LAUNCHER_SHELL, "-c", command, NULL);
    } else {
        execlp(LAUNCHER_OPENER, LAUNCHER_OPENER, command, NULL);
    }

    fprintf(stderr, "%s: exec() failed: %s: %s\n", LAUNCHER_NAME, command, strerror(errno));

    _exit(127);
}


/**
 * @brief Moves the selection, which the field holds the keyboard for.
 *
 * @param delta How many rows to move by.
 */

static void launcher_move(int delta) {

    const size_t count = ui_list_count(launcher.list);

    if (count == 0) {
        return;
    }


    int index = ui_list_selected(launcher.list) + delta;

    if (index < 0) {
        index = 0;
    }

    if ((size_t)index >= count) {
        index = (int)count - 1;
    }

    ui_list_select(launcher.list, index);
}


/**
 * @brief Reports how many rows fit in the list, which is what a page key moves by.
 *
 * @return The count, never less than one.
 */

static int launcher_page(void) {

    const int height = (ui_widget_rect(launcher.list).height - 2) / LAUNCHER_ROW;

    return height > 1 ? height : 1;
}


static void launcher_on_change(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    launcher_refilter();
}


static void launcher_on_submit(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    launcher_run();
}


static void launcher_on_activate(ui_widget_t* widget, int index, void* user) {

    (void)widget;
    (void)index;
    (void)user;

    launcher_run();
}


/**
 * @brief Takes the keys the field let through, which is how a focused field drives the list.
 *
 * @param view The view.
 * @param vkey The key code.
 * @param down Whether the key went down or came up.
 * @param user Unused.
 * @return true when the key was acted on.
 */

static bool launcher_on_key(ui_view_t* view, uint16_t vkey, bool down, void* user) {

    (void)view;
    (void)user;

    if (!down) {
        return false;
    }


    switch (vkey) {

        case KEY_ESC:
            launcher.dismissed = true;
            return true;

        case KEY_UP:
            launcher_move(-1);
            return true;

        case KEY_DOWN:
            launcher_move(1);
            return true;

        case KEY_PAGEUP:
            launcher_move(-launcher_page());
            return true;

        case KEY_PAGEDOWN:
            launcher_move(launcher_page());
            return true;

        default:
            return false;
    }
}


static void launcher_layout(ui_view_t* view, int width, int height, void* user) {

    (void)view;
    (void)user;

    const ui_rect_t bounds = {0, 0, width, height};

    ui_widget_place(launcher.backdrop, bounds);


    const ui_rect_t inner = ui_rect_inset(bounds, LAUNCHER_MARGIN);

    if (inner.width <= 0 || inner.height < LAUNCHER_FIELD) {
        return;
    }


    int remaining = inner.height - LAUNCHER_FIELD - LAUNCHER_MARGIN;

    if (remaining < 0) {
        remaining = 0;
    }


    const ui_rect_t field = {inner.x, inner.y, inner.width, LAUNCHER_FIELD};

    const ui_rect_t rows = {inner.x, inner.y + LAUNCHER_FIELD + LAUNCHER_MARGIN, inner.width, remaining};

    ui_widget_place(launcher.field, field);
    ui_widget_place(launcher.list, rows);
}


/**
 * @brief Builds the widgets, in the order they are painted in.
 *
 * @return 0 on success, or -1 when a widget could not be created.
 */

static int launcher_build(void) {

    const ui_theme_t* theme = ui_theme();

    launcher.backdrop = ui_panel_create(launcher.view);
    launcher.field    = ui_entry_create(launcher.view, "Search applications...");
    launcher.list     = ui_list_create(launcher.view);

    if (!launcher.backdrop || !launcher.field || !launcher.list) {
        return -1;
    }

    ui_panel_set_color(launcher.backdrop, ui_rgba(0x1E, 0x1E, 0x1E, 0.86));
    ui_panel_set_radius(launcher.backdrop, (double)UI_WINDOW_RADIUS);
    ui_panel_set_border(launcher.backdrop, ui_rgba(0xFF, 0xFF, 0xFF, 0.18), 1.0);

    ui_entry_set_font(launcher.field, UI_FONT_REGULAR, theme->font_size + 3.0);
    ui_entry_on_change(launcher.field, launcher_on_change, NULL);
    ui_entry_on_submit(launcher.field, launcher_on_submit, NULL);

    ui_list_set_row_height(launcher.list, LAUNCHER_ROW);
    ui_list_on_activate(launcher.list, launcher_on_activate, NULL);

    ui_view_focus(launcher.view, launcher.field);

    return 0;
}


/**
 * @brief Acts on one event.
 *
 * @param event The event.
 * @return true when the launcher should stay open.
 */

static bool launcher_dispatch(const ui_event_t* event) {

    if (event->type == UI_EVENT_FOCUS && !event->focus.focused) {
        return false;
    }

    ui_view_dispatch(launcher.view, event);

    return !launcher.dismissed && !ui_view_closed(launcher.view);
}


/**
 * @brief The event loop, which is the library's own plus a window that closes when it is left.
 *
 * @return 0 on a clean dismissal, -1 when the connection failed.
 */

static int launcher_loop(void) {

    bool running = true;

    while (running) {

        const int timeout = ui_view_timeout(launcher.view);

        if (ui_view_present(launcher.view) < 0) {
            return -1;
        }


        ui_event_t event;

        const int got = ui_next_event(launcher.conn, &event, timeout);

        if (got < 0) {
            return -1;
        }

        if (got == 0) {
            continue;
        }

        running = launcher_dispatch(&event);

        for (int i = 0; running && i < LAUNCHER_DRAIN_MAX; i++) {

            if (ui_next_event(launcher.conn, &event, 0) <= 0) {
                break;
            }

            running = launcher_dispatch(&event);
        }
    }

    return 0;
}


int main(int argc, char** argv) {

    (void)argc;
    (void)argv;

    setvbuf(stdout, NULL, _IONBF, 0);


    launcher.count = launcher_scan(LAUNCHER_APPLICATIONS, &launcher.entries);

    launcher.conn = ui_connect(NULL, LAUNCHER_CONNECT_MS);

    if (!launcher.conn) {

        fprintf(stderr, "%s: ui_connect() failed: %s\n", LAUNCHER_NAME, strerror(errno));

        free(launcher.entries);
        return 1;
    }


    launcher.window = ui_window_create_ex(launcher.conn, LAUNCHER_WINDOW_WIDTH, launcher_height(0), LAUNCHER_NAME, UI_WINDOW_BORDERLESS | UI_WINDOW_CENTERED | UI_WINDOW_TRANSLUCENT);

    if (!launcher.window) {

        fprintf(stderr, "%s: ui_window_create_ex() failed: %s\n", LAUNCHER_NAME, strerror(errno));

        ui_disconnect(launcher.conn);

        free(launcher.entries);
        return 1;
    }


    launcher_theme_init();
    ui_theme_set(&launcher_theme);

    launcher.view = ui_view_create(launcher.window);

    if (!launcher.view || launcher_build() < 0) {

        fprintf(stderr, "%s: cannot build the window\n", LAUNCHER_NAME);

        launcher_teardown();
        return 1;
    }

    ui_view_on_key(launcher.view, launcher_on_key, NULL);
    ui_view_on_layout(launcher.view, launcher_layout, NULL);

    launcher_refilter();


    const int status = launcher_loop();

    launcher_teardown();

    return status < 0 ? 1 : 0;
}
