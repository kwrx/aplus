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
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <aplus/input.h>
#include <aplus/ui-widgets.h>
#include <aplus/ui.h>
#include <aplus/ulog.h>

#include "desktop.h"

#if defined(CONFIG_HAVE_AI)

    #include <pthread.h>
    #include <stdatomic.h>

    #include <aplus/ui-draw.h>

    #include "ai.h"

#endif


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
 * @brief What gives that command a window to run in, and what is left running in it afterwards.
 */
#define LAUNCHER_TERMINAL "aplus-terminal"
#define LAUNCHER_SESSION  "%s; exec " LAUNCHER_SHELL

/**
 * @brief What a row shows when its .desktop file named no icon, and what the shell row shows.
 */
#define LAUNCHER_ICON_DEFAULT "application-x-executable"
#define LAUNCHER_ICON_SHELL   "system-run"

#if defined(CONFIG_HAVE_AI)
    /**
     * @brief What the row that asks the model, and the row that waits for it, show.
     */
    #define LAUNCHER_ICON_AI "system-ai"
#endif

/**
 * @brief How long to keep trying to reach the display server.
 */
#define LAUNCHER_CONNECT_MS 5000


#define LAUNCHER_WINDOW_WIDTH 680

#define LAUNCHER_FIELD     56
#define LAUNCHER_ROW       40
#define LAUNCHER_ICON      24
#define LAUNCHER_SEPARATOR 1

/**
 * @brief How far the field and the list sit in from the sides, and the space left under the last row.
 */
#define LAUNCHER_FIELD_INSET 5
#define LAUNCHER_LIST_INSET  6
#define LAUNCHER_BOTTOM      4

/**
 * @brief What the list takes up besides its rows: a pixel of border and its own inset, top and bottom.
 */
#define LAUNCHER_LIST_CHROME (2 + 2 * UI_LIST_INSET)

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

#if defined(CONFIG_HAVE_AI)

    /**
     * @brief The same, for the rows an answer is made of: one to ask, one to run, and the rest to read.
     */
    #define LAUNCHER_ASK_ROW     ((void*)-2)
    #define LAUNCHER_COMMAND_ROW ((void*)-3)
    #define LAUNCHER_TEXT_ROW    ((void*)-4)

    /**
     * @brief How many rows an answer grows the window to, which is worth more of the screen than a list of matches.
     */
    #define LAUNCHER_ANSWER_ROWS_MAX 10

    /**
     * @brief How long a wrapped line may be, and how many of them an answer is cut down to.
     */
    #define LAUNCHER_LINE_MAX  160
    #define LAUNCHER_LINES_MAX 32

    /**
     * @brief How much narrower than the window a wrapped line is.
     *
     * The list's own padding, the icon gutter it reserves for every row once one row has an
     * icon, and room for the scrollbar. A line wider than this is ellipsised rather than wrapped.
     */
    #define LAUNCHER_LINE_INSET 72

    /**
     * @brief How often the event loop checks for an answer while one is awaited, in milliseconds.
     */
    #define LAUNCHER_AI_TICK_MS 100

    /**
     * @brief The stack of the thread that asks the model, sized for a TLS handshake.
     */
    #define LAUNCHER_AI_STACK (512 * 1024)


/**
 * @brief One question to the model, owned jointly by the main thread and the thread asking it.
 */

typedef struct {

    atomic_int refs;
    atomic_bool done;

    char prompt[LAUNCHER_FIELD_MAX];

    launcher_ai_reply_t reply;

} launcher_job_t;

#endif


static struct {

    ui_connection_t* conn;
    ui_window_t* window;
    ui_view_t* view;

    ui_widget_t* backdrop;
    ui_widget_t* field;
    ui_widget_t* separator;
    ui_widget_t* list;

    launcher_entry_t* entries;
    size_t count;

    bool all;
    bool drawer;
    bool dismissed;

#if defined(CONFIG_HAVE_AI)

    struct {

        bool answered;

        launcher_job_t* job;

        launcher_ai_reply_t reply;

    } ai;

#endif

} launcher = {0};


/**
 * @brief The system theme with the light let through it: nothing but the panel paints a background, and the selection takes the accent.
 *
 * The panel is what the window looks like, so the view is left with nothing of its own to
 * fill -- anything it painted would sit under the panel and darken it for no gain.
 */
static ui_theme_t launcher_theme;


/**
 * @brief Fills in the theme, which cannot be a file-scope initialiser since it starts from another one.
 */

static void launcher_theme_init(void) {

    launcher_theme = *ui_theme_system();

    launcher_theme.background     = ui_rgba(0x00, 0x00, 0x00, 0.0);
    launcher_theme.surface_sunken = ui_rgba(0x00, 0x00, 0x00, 0.0);
    launcher_theme.border         = ui_rgba(0x00, 0x00, 0x00, 0.0);
    launcher_theme.focus_ring     = ui_rgba(0x00, 0x00, 0x00, 0.0);

    launcher_theme.secondary    = launcher_theme.primary;
    launcher_theme.on_secondary = launcher_theme.on_primary;
}


/**
 * @brief Reports how many rows the window grows to before the list scrolls instead.
 *
 * @return The count.
 */

static int launcher_rows_max(void) {

#if defined(CONFIG_HAVE_AI)

    if (launcher.ai.answered) {
        return LAUNCHER_ANSWER_ROWS_MAX;
    }

#endif

    return LAUNCHER_ROWS_MAX;
}


/**
 * @brief Reports the window height that shows a number of rows and no part of another.
 *
 * No rows is the field on its own, which is what an untouched launcher looks like.
 *
 * @param rows How many rows to fit, clamped to at most what launcher_rows_max() allows.
 * @return The height in pixels.
 */

static int launcher_height(int rows) {

    if (rows < 1) {
        return LAUNCHER_FIELD;
    }

    if (rows > launcher_rows_max()) {
        rows = launcher_rows_max();
    }

    return LAUNCHER_FIELD + LAUNCHER_SEPARATOR + rows * LAUNCHER_ROW + LAUNCHER_LIST_CHROME + LAUNCHER_BOTTOM;
}


/**
 * @brief Selects the first row and asks for the height the list now needs.
 */

static void launcher_resize(void) {

    const size_t rows = ui_list_count(launcher.list);

    ui_list_select(launcher.list, 0);
    ui_widget_set_visible(launcher.list, rows > 0);
    ui_widget_set_visible(launcher.separator, rows > 0);

    ui_window_request_size(launcher.window, LAUNCHER_WINDOW_WIDTH, launcher_height((int)rows));
}


/**
 * @brief Reports the command a query spells out with a leading '>', which is run rather than searched for or asked.
 *
 * @param query What was typed.
 * @return The command past the '>' and any blanks, or NULL when the query does not start with one.
 */

static const char* launcher_command(const char* query) {

    if (*query != '>') {
        return NULL;
    }

    query++;

    while (*query == ' ' || *query == '\t') {
        query++;
    }

    return query;
}


/**
 * @brief Reports whether an entry answers to a query; an empty one matches everything with --all, and nothing otherwise.
 *
 * @param entry The entry to test.
 * @param query What was typed.
 * @return true when the entry should be listed.
 */

static bool launcher_matches(const launcher_entry_t* entry, const char* query) {

    if (!*query) {
        return launcher.all;
    }

    return strcasestr(entry->name, query) != NULL || strcasestr(entry->comment, query) != NULL || strcasestr(entry->exec, query) != NULL;
}


#if defined(CONFIG_HAVE_AI)


/**
 * @brief Drops one hold on a question, freeing it with the last.
 *
 * @param job The question.
 */

static void launcher_job_put(launcher_job_t* job) {

    if (atomic_fetch_sub_explicit(&job->refs, 1, memory_order_acq_rel) == 1) {
        free(job);
    }
}


/**
 * @brief Asks the model a question on a thread of its own.
 *
 * @param arg The question.
 * @return NULL.
 */

static void* launcher_job_run(void* arg) {

    launcher_job_t* job = arg;

    launcher_ai_ask(job->prompt, &job->reply);

    atomic_store_explicit(&job->done, true, memory_order_release);

    launcher_job_put(job);

    return NULL;
}


/**
 * @brief Forgets the question in flight, so that its answer is dropped when it arrives.
 */

static void launcher_abandon(void) {

    if (launcher.ai.job) {

        launcher_job_put(launcher.ai.job);

        launcher.ai.job = NULL;
    }
}


#endif


/**
 * @brief Rebuilds the list from what is in the field, and asks for the height that now fits it.
 *
 * An empty field lists nothing and leaves the window the height of the field alone, or lists
 * every application with --all. A query that matches nothing is offered to the shell instead,
 * which is the whole of what this has that a menu does not. A query starting with '>' is
 * always offered to the shell, without searching or asking.
 */

static void launcher_refilter(void) {

    const char* query = ui_entry_text(launcher.field);

    ui_list_clear(launcher.list);

#if defined(CONFIG_HAVE_AI)

    launcher.ai.answered = false;

    launcher_abandon();

#endif

    const char* command = launcher_command(query);

    if (command) {

        if (*command) {

            char row[LAUNCHER_FIELD_MAX + 8];

            snprintf(row, sizeof(row), "Run \"%s\"", command);

            ui_list_add_icon(launcher.list, LAUNCHER_ICON_SHELL, row, "shell", LAUNCHER_RUN_ROW);
        }

        launcher_resize();
        return;
    }

    for (size_t i = 0; i < launcher.count; i++) {

        if (launcher_matches(&launcher.entries[i], query)) {

            const launcher_entry_t* entry = &launcher.entries[i];

            ui_list_add_icon(launcher.list, entry->icon[0] ? entry->icon : LAUNCHER_ICON_DEFAULT, entry->name, entry->comment, (void*)entry);
        }
    }

    if (ui_list_count(launcher.list) == 0 && *query) {

        char row[LAUNCHER_FIELD_MAX + 8];

#if defined(CONFIG_HAVE_AI)

        snprintf(row, sizeof(row), "Ask \"%s\"", query);

        ui_list_add_icon(launcher.list, LAUNCHER_ICON_AI, row, "ask", LAUNCHER_ASK_ROW);

#else

        snprintf(row, sizeof(row), "Run \"%s\"", query);

        ui_list_add_icon(launcher.list, LAUNCHER_ICON_SHELL, row, "shell", LAUNCHER_RUN_ROW);

#endif
    }

    launcher_resize();
}


#if defined(CONFIG_HAVE_AI)


/**
 * @brief Measures a line in the font the rows are drawn in.
 *
 * @param text The line to measure.
 * @return Its width in pixels, or 0 when the font cannot be loaded.
 */

static double launcher_measure(const char* text) {

    const ui_theme_t* theme = ui_theme();

    cairo_font_face_t* face = ui_font_face(theme->font_regular);

    if (!face) {
        return 0.0;
    }


    cairo_scaled_font_t* scaled = ui_font_scaled(face, theme->font_size);

    if (!scaled) {
        return 0.0;
    }


    cairo_text_extents_t extents;

    cairo_scaled_font_text_extents(scaled, text, &extents);

    return extents.x_advance;
}


/**
 * @brief Reports how much of a word fits a width, which is where a word longer than a line is cut.
 *
 * @param word The word.
 * @param len How long it is, which must leave room for a terminator in a line.
 * @param width How wide a line may be, in pixels.
 * @return How many bytes fit, never fewer than one and never half of a character.
 */

static size_t launcher_fit(const char* word, size_t len, double width) {

    char text[LAUNCHER_LINE_MAX];

    size_t take = len;

    while (take > 1) {

        memcpy(text, word, take);

        text[take] = '\0';

        if (launcher_measure(text) <= width) {
            break;
        }

        take--;

        while (take > 1 && ((unsigned char)word[take] & 0xC0) == 0x80) {
            take--;
        }
    }

    return take;
}


/**
 * @brief Breaks text into lines that fit a width, at the spaces where it can and mid-word where it cannot.
 *
 * Blank lines are dropped rather than kept: the window has too few rows to spend one on nothing.
 *
 * @param text What to break up.
 * @param width How wide a line may be, in pixels.
 * @param out Receives the lines.
 * @param max How many lines there is room for.
 * @return How many lines were written.
 */

static size_t launcher_wrap(const char* text, double width, char out[][LAUNCHER_LINE_MAX], size_t max) {

    char line[LAUNCHER_LINE_MAX];

    size_t count = 0;
    size_t len   = 0;

    line[0] = '\0';

    for (const char* p = text; *p && count < max;) {

        if (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
            continue;
        }

        if (*p == '\n') {

            if (len > 0) {
                memcpy(out[count++], line, len + 1);
            }

            line[0] = '\0';
            len     = 0;

            p++;

            continue;
        }


        const char* end = p;

        while (*end && *end != ' ' && *end != '\t' && *end != '\r' && *end != '\n') {
            end++;
        }


        char candidate[LAUNCHER_LINE_MAX];

        size_t at   = len;
        size_t take = (size_t)(end - p);

        memcpy(candidate, line, len);

        if (at > 0 && at + 2 < sizeof(candidate)) {
            candidate[at++] = ' ';
        }

        if (at + take + 1 > sizeof(candidate)) {
            take = sizeof(candidate) - at - 1;
        }

        if (take == 0) {

            memcpy(out[count++], line, len + 1);

            line[0] = '\0';
            len     = 0;

            continue;
        }

        memcpy(candidate + at, p, take);

        candidate[at + take] = '\0';


        if (launcher_measure(candidate) > width) {

            if (len > 0) {

                memcpy(out[count++], line, len + 1);

                line[0] = '\0';
                len     = 0;

                continue;
            }

            take = launcher_fit(candidate, take, width);

            candidate[take] = '\0';
        }

        memcpy(line, candidate, at + take + 1);

        len = at + take;

        p += take;
    }

    if (len > 0 && count < max) {
        memcpy(out[count++], line, len + 1);
    }

    return count;
}


/**
 * @brief Says that the model is being waited on, in the one row the query was asked from.
 */

static void launcher_thinking(void) {

    ui_list_clear(launcher.list);
    ui_list_add_icon(launcher.list, LAUNCHER_ICON_AI, "Thinking...", NULL, LAUNCHER_TEXT_ROW);

    launcher_resize();
}


/**
 * @brief Shows what came back: the command it suggested, where there is one, and then the answer itself.
 *
 * The command is the first row and so the selected one, which is what makes Enter run it.
 */

static void launcher_answer(void) {

    const launcher_ai_reply_t* reply = &launcher.ai.reply;

    ui_list_clear(launcher.list);

    launcher.ai.answered = true;

    if (reply->command[0]) {
        ui_list_add_icon(launcher.list, LAUNCHER_ICON_SHELL, reply->command, "run", LAUNCHER_COMMAND_ROW);
    }


    char lines[LAUNCHER_LINES_MAX][LAUNCHER_LINE_MAX];

    const char* text = reply->error[0] ? reply->error : reply->answer;

    const size_t count = launcher_wrap(text, LAUNCHER_WINDOW_WIDTH - 2 * LAUNCHER_LIST_INSET - LAUNCHER_LINE_INSET, lines, LAUNCHER_LINES_MAX);

    for (size_t i = 0; i < count; i++) {
        ui_list_add(launcher.list, lines[i], NULL, LAUNCHER_TEXT_ROW);
    }

    launcher_resize();
}


/**
 * @brief Shows an answer that could not even be asked for.
 *
 * @param reason What went wrong.
 */

static void launcher_refuse(const char* reason) {

    memset(&launcher.ai.reply, 0, sizeof(launcher.ai.reply));

    snprintf(launcher.ai.reply.error, sizeof(launcher.ai.reply.error), "%s", reason);

    launcher_answer();
}


/**
 * @brief Puts the query to the model on a thread of its own, leaving the window live until launcher_collect() shows the answer.
 */

static void launcher_ask(void) {

    const char* query = ui_entry_text(launcher.field);

    if (!*query) {
        return;
    }

    launcher_abandon();

    if (launcher_ai_init() < 0) {

        launcher_refuse("Cannot initialise the HTTPS library.");
        return;
    }


    launcher_job_t* job = calloc(1, sizeof(*job));

    if (!job) {

        launcher_refuse("Out of memory.");
        return;
    }

    strncpy(job->prompt, query, sizeof(job->prompt) - 1);

    atomic_init(&job->refs, 2);
    atomic_init(&job->done, false);


    pthread_attr_t attr;
    pthread_t thread;

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, LAUNCHER_AI_STACK);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    const int e = pthread_create(&thread, &attr, launcher_job_run, job);

    pthread_attr_destroy(&attr);

    if (e != 0) {

        free(job);

        launcher_refuse(strerror(e));
        return;
    }

    launcher.ai.job = job;

    launcher_thinking();
}


/**
 * @brief Shows the answer to the question in flight, once it has one.
 */

static void launcher_collect(void) {

    launcher_job_t* job = launcher.ai.job;

    if (!job || !atomic_load_explicit(&job->done, memory_order_acquire)) {
        return;
    }

    launcher.ai.reply = job->reply;

    launcher_abandon();
    launcher_answer();
}


#endif


/**
 * @brief Gives back everything held, in the order the library requires, before an exec or an exit.
 */

static void launcher_teardown(void) {

#if defined(CONFIG_HAVE_AI)

    launcher_abandon();

#endif

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
 * A command gets a terminal of its own, which stays at a shell prompt once the command is
 * through. Returns only when the exec failed, having already given the window back.
 */

static void launcher_run(void) {

    const int index = ui_list_selected(launcher.list);

    if (index < 0) {
        return;
    }


    const void* row = ui_list_item_user(launcher.list, index);

#if defined(CONFIG_HAVE_AI)

    if (row == LAUNCHER_TEXT_ROW) {
        return;
    }

    if (row == LAUNCHER_ASK_ROW) {

        launcher_ask();
        return;
    }

#endif

    const char* source = NULL;

    if (row == LAUNCHER_RUN_ROW) {

        source = ui_entry_text(launcher.field);

        if (launcher_command(source)) {
            source = launcher_command(source);
        }
    }

#if defined(CONFIG_HAVE_AI)

    if (row == LAUNCHER_COMMAND_ROW) {
        source = launcher.ai.reply.command;
    }

#endif

    const bool shell = source != NULL;

    if (!shell) {
        source = ((const launcher_entry_t*)row)->path;
    }


    char command[PATH_MAX];

    strncpy(command, source, sizeof(command) - 1);

    command[sizeof(command) - 1] = '\0';

    launcher_teardown();

    for (int fd = STDERR_FILENO + 1; fd < CONFIG_OPEN_MAX; fd++) {
        close(fd);
    }

    if (shell) {

        char session[sizeof(command) + sizeof(LAUNCHER_SESSION)];

        snprintf(session, sizeof(session), LAUNCHER_SESSION, command);

        execlp(LAUNCHER_TERMINAL, LAUNCHER_TERMINAL, "-c", session, NULL);
        execl(LAUNCHER_SHELL, LAUNCHER_SHELL, "-c", command, NULL);

    } else {
        execlp(LAUNCHER_OPENER, LAUNCHER_OPENER, command, NULL);
    }

    ulog_errorf("exec() failed: %s: %s", command, strerror(errno));

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

    const int height = (ui_widget_rect(launcher.list).height - LAUNCHER_LIST_CHROME) / LAUNCHER_ROW;

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

    if (launcher.backdrop) {
        ui_widget_place(launcher.backdrop, bounds);
    }

    if (width <= 2 * LAUNCHER_LIST_INSET || height < LAUNCHER_FIELD) {
        return;
    }


    int remaining = height - LAUNCHER_FIELD - LAUNCHER_SEPARATOR - LAUNCHER_BOTTOM;

    if (remaining < 0) {
        remaining = 0;
    }


    const ui_rect_t field     = {LAUNCHER_FIELD_INSET, 0, width - 2 * LAUNCHER_FIELD_INSET, LAUNCHER_FIELD};
    const ui_rect_t separator = {1, LAUNCHER_FIELD, width - 2, LAUNCHER_SEPARATOR};
    const ui_rect_t rows      = {LAUNCHER_LIST_INSET, LAUNCHER_FIELD + LAUNCHER_SEPARATOR, width - 2 * LAUNCHER_LIST_INSET, remaining};

    ui_widget_place(launcher.field, field);
    ui_widget_place(launcher.separator, separator);
    ui_widget_place(launcher.list, rows);
}


/**
 * @brief Follows the system theme: the launcher's own theme, the panels' colours and the field's font.
 *
 * @param view The view.
 * @param user Unused.
 */
static void launcher_restyle(ui_view_t* view, void* user) {

    (void)view;
    (void)user;

    launcher_theme_init();

    const ui_theme_t* system = ui_theme_system();

    if (launcher.backdrop) {

        ui_color_t well = system->surface_sunken;
        ui_color_t edge = system->border;

        well.a = 0.5;

        ui_color_t panel = ui_color_blend(system->background, well);

        panel.a = 0.90;
        edge.a *= 2.0;

        ui_panel_set_color(launcher.backdrop, panel);
        ui_panel_set_border(launcher.backdrop, edge, 1.0);
    }

    ui_panel_set_color(launcher.separator, system->border);

    ui_entry_set_font(launcher.field, UI_FONT_REGULAR, launcher_theme.font_size + 4.0);
}


/**
 * @brief Builds the widgets, in the order they are painted in.
 *
 * @return 0 on success, or -1 when a widget could not be created.
 */

static int launcher_build(void) {

    launcher.backdrop  = launcher.drawer ? NULL : ui_panel_create(launcher.view);
#if defined(CONFIG_HAVE_AI)
    launcher.field     = ui_entry_create(launcher.view, "Search apps, ask or type > command\xE2\x80\xA6");
#else
    launcher.field     = ui_entry_create(launcher.view, "Search apps or type > command\xE2\x80\xA6");
#endif
    launcher.separator = ui_panel_create(launcher.view);
    launcher.list      = ui_list_create(launcher.view);

    if ((!launcher.drawer && !launcher.backdrop) || !launcher.field || !launcher.separator || !launcher.list) {
        return -1;
    }

    if (launcher.backdrop) {
        ui_panel_set_radius(launcher.backdrop, (double)UI_WINDOW_RADIUS);
    }

    ui_panel_set_radius(launcher.separator, 0.0);
    ui_widget_set_visible(launcher.separator, false);

    launcher_restyle(launcher.view, NULL);

    ui_entry_on_change(launcher.field, launcher_on_change, NULL);
    ui_entry_on_submit(launcher.field, launcher_on_submit, NULL);

    ui_list_set_row_height(launcher.list, LAUNCHER_ROW);
    ui_list_set_icon_size(launcher.list, LAUNCHER_ICON);
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
 * @brief Reports how long the event loop may sleep: as long as the view allows, and no longer than a tick while an answer is awaited.
 *
 * @return The timeout in milliseconds, or -1 to block.
 */

static int launcher_timeout(void) {

    int timeout = ui_view_timeout(launcher.view);

#if defined(CONFIG_HAVE_AI)

    if (launcher.ai.job && (timeout < 0 || timeout > LAUNCHER_AI_TICK_MS)) {
        timeout = LAUNCHER_AI_TICK_MS;
    }

#endif

    return timeout;
}


/**
 * @brief The event loop, which is the library's own plus a window that closes when it is left.
 *
 * @return 0 on a clean dismissal, -1 when the connection failed.
 */

static int launcher_loop(void) {

    bool running = true;

    while (running) {

#if defined(CONFIG_HAVE_AI)

        launcher_collect();

#endif

        const int timeout = launcher_timeout();

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


/**
 * @brief Prints how to run the launcher and exits.
 */

static void launcher_usage(void) {

    printf("Use: " LAUNCHER_NAME " [options]...\n"
           "Search the installed applications and run one.\n\n"
           "   -a, --all                   list every application before anything is typed\n"
           "   -d, --drawer                slide out of the taskbar's rail instead of opening in the middle\n"
           "       --help                  show this help\n");

    exit(0);
}


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);


    static struct option long_options[] = {
        {"all",    no_argument, NULL, 'a'},
        {"drawer", no_argument, NULL, 'd'},
        {"help",   no_argument, NULL, 'h'},
        {NULL,     0,           NULL, 0  }
    };


    int c, idx;

    while ((c = getopt_long(argc, argv, "adh", long_options, &idx)) != -1) {

        switch (c) {

            case 'a':
                launcher.all = true;
                break;
            case 'd':
                launcher.drawer = true;
                break;
            case 'h':
            case '?':
                launcher_usage();
                break;
            default:
                abort();
        }
    }


    launcher.count = launcher_scan(LAUNCHER_APPLICATIONS, &launcher.entries);

    launcher.conn = ui_connect(NULL, LAUNCHER_CONNECT_MS);

    if (!launcher.conn) {

        ulog_errorf("ui_connect() failed: %s", strerror(errno));

        free(launcher.entries);
        return 1;
    }


    const uint32_t flags = launcher.drawer ? UI_WINDOW_DRAWER | UI_WINDOW_TRANSLUCENT : UI_WINDOW_BORDERLESS | UI_WINDOW_CENTERED | UI_WINDOW_TRANSLUCENT;

    launcher.window = ui_window_create_ex(launcher.conn, LAUNCHER_WINDOW_WIDTH, launcher_height(launcher.all ? (int)launcher.count : 0), LAUNCHER_NAME, flags);

    if (!launcher.window) {

        ulog_errorf("ui_window_create_ex() failed: %s", strerror(errno));

        ui_disconnect(launcher.conn);

        free(launcher.entries);
        return 1;
    }


    launcher_theme_init();
    ui_theme_set(&launcher_theme);

    launcher.view = ui_view_create(launcher.window);

    if (!launcher.view || launcher_build() < 0) {

        ulog_errorf("cannot build the window");

        launcher_teardown();
        return 1;
    }

    ui_view_on_key(launcher.view, launcher_on_key, NULL);
    ui_view_on_layout(launcher.view, launcher_layout, NULL);
    ui_view_on_theme(launcher.view, launcher_restyle, NULL);

    launcher_refilter();


    const int status = launcher_loop();

    launcher_teardown();

    return status < 0 ? 1 : 0;
}
