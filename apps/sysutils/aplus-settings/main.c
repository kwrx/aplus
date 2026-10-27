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
 * @brief The system settings: a sidebar of pages, and the page selected beside it.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <aplus/ui-anim.h>
#include <aplus/ui-draw.h>
#include <aplus/ulog.h>

#include "settings.h"


#define SETTINGS_WINDOW_WIDTH  780
#define SETTINGS_WINDOW_HEIGHT 520

#define SETTINGS_SIDEBAR      184
#define SETTINGS_SIDEBAR_GAP  24
#define SETTINGS_PAGE_WIDGETS 128
#define SETTINGS_DRAIN_MAX    64


/**
 * @brief The pages, in the order the sidebar lists them.
 */
static const settings_page_t* const settings_pages[] = {
    &settings_appearance, &settings_wallpaper, &settings_network, &settings_software, &settings_about,
};

#define SETTINGS_PAGES (sizeof(settings_pages) / sizeof(settings_pages[0]))


settings_t settings;


/**
 * @brief The sidebar, the page on show, and the widgets each page owns.
 */
static struct {

    ui_widget_t* sidebar;

    size_t page;
    size_t building;

    ui_widget_t* owned[SETTINGS_PAGES][SETTINGS_PAGE_WIDGETS];
    size_t owned_count[SETTINGS_PAGES];

} settings_state;


/**
 * @brief Records a widget as part of the page being built, so that it shows and hides with it.
 *
 * @param widget The widget, or NULL after a failed create.
 * @return The widget.
 */
ui_widget_t* settings_own(ui_widget_t* widget) {

    const size_t page = settings_state.building;

    if (widget && settings_state.owned_count[page] < SETTINGS_PAGE_WIDGETS) {
        settings_state.owned[page][settings_state.owned_count[page]++] = widget;
    }

    return widget;
}


/**
 * @brief Asks for the page's own drawing to be laid down again, even when no widget changed.
 */
void settings_repaint(void) {
    settings.repaint = true;
}


/**
 * @brief Reports the theme's font size, which every height on the pages is scaled from.
 *
 * @return The size in pixels.
 */
double settings_font_size(void) {
    return ui_theme()->font_size > 0.0 ? ui_theme()->font_size : 14.0;
}


/**
 * @brief Reports the font size of a page's title.
 *
 * @return The size in pixels.
 */
double settings_title_size(void) {
    return settings_font_size() * 1.55;
}


/**
 * @brief Reports the height of a line of body text.
 *
 * @return The height in pixels.
 */
int settings_line_height(void) {
    return (int)(settings_font_size() * 1.7 + 0.5);
}


/**
 * @brief Reports the height of a small button set beside a heading.
 *
 * @return The height in pixels.
 */
int settings_control_height(void) {
    return settings_line_height() + 6;
}


/**
 * @brief Reports the height of a card's row.
 *
 * @return The height in pixels.
 */
int settings_row_height(void) {

    const int height = (int)(settings_font_size() * 2.85 + 0.5);

    return height < 36 ? 36 : height;
}


/**
 * @brief Measures text in the theme's font at its size.
 *
 * @param text The text.
 * @param weight The weight it is drawn in.
 * @return The width in pixels, rounded up.
 */
int settings_text_width(const char* text, ui_font_weight_t weight) {

    const char* font = ui_theme()->font_regular;

    if (weight == UI_FONT_BOLD && ui_theme()->font_bold && ui_theme()->font_bold[0]) {
        font = ui_theme()->font_bold;
    } else if (weight == UI_FONT_MEDIUM && ui_theme()->font_medium && ui_theme()->font_medium[0]) {
        font = ui_theme()->font_medium;
    }

    return (int)(ui_draw_text_width(text, font, settings_font_size()) + 0.999);
}


/**
 * @brief Shows text in a label, cut short with an ellipsis where the label is too narrow.
 *
 * @param label The label.
 * @param text The text.
 */
void settings_fit(ui_widget_t* label, const char* text) {

    const ui_rect_t rect = ui_widget_rect(label);

    if (rect.width <= 0) {
        ui_label_set_text(label, text);
        return;
    }

    char fitted[SETTINGS_TEXT_MAX];

    ui_draw_ellipsize(fitted, sizeof(fitted), text, ui_theme()->font_regular, settings_font_size(), rect.width);
    ui_label_set_text(label, fitted);
}


/**
 * @brief Creates a section heading, bold at the theme's size.
 *
 * @param text The heading.
 * @return The label, or NULL.
 */
ui_widget_t* settings_heading(const char* text) {

    ui_widget_t* label = settings_own(ui_label_create(settings.view, text));

    if (label) {
        ui_label_set_font(label, UI_FONT_BOLD, 0.0);
    }

    return label;
}


/**
 * @brief Creates a page's title and the line under it.
 *
 * @param header The header to fill in.
 * @param title The title.
 * @param subtitle The line under it.
 * @return 0 on success, -1 otherwise.
 */
int settings_header_build(settings_header_t* header, const char* title, const char* subtitle) {

    header->title    = settings_own(ui_label_create(settings.view, title));
    header->subtitle = settings_own(ui_label_create(settings.view, subtitle));

    return header->title && header->subtitle ? 0 : -1;
}


/**
 * @brief Places a page's title and subtitle at its top.
 *
 * @param header The header.
 * @param page The page's rectangle.
 * @param reserve How much of the title's row to leave free on the right, for buttons.
 * @return Where the content below starts.
 */
int settings_header_place(settings_header_t* header, ui_rect_t page, int reserve) {

    const int title = (int)(settings_title_size() * 1.5 + 0.5);
    const int line  = settings_line_height();

    ui_widget_set_rect(header->title, page.x, page.y, page.width - reserve, title);
    ui_widget_set_rect(header->subtitle, page.x, page.y + title, page.width, line);

    return page.y + title + line + SETTINGS_SECTION_GAP;
}


/**
 * @brief Gives a header the fonts and colours of the theme in use.
 *
 * @param header The header.
 */
void settings_header_restyle(settings_header_t* header) {

    ui_label_set_font(header->title, UI_FONT_BOLD, settings_title_size());
    ui_label_set_color(header->subtitle, ui_theme()->text_muted);
}


/**
 * @brief Creates a card and its rows, every value starting out unknown.
 *
 * @param card The card to fill in.
 * @param keys The names of the rows.
 * @param count How many rows there are, at most SETTINGS_CARD_ROWS.
 * @param custom A bit per row whose value the page shows with widgets of its own.
 * @return 0 on success, -1 otherwise.
 */
int settings_card_build(settings_card_t* card, const char* const* keys, size_t count, uint32_t custom) {

    if ((card->panel = settings_own(ui_panel_create(settings.view))) == NULL) {
        return -1;
    }

    card->count = count < SETTINGS_CARD_ROWS ? count : SETTINGS_CARD_ROWS;

    for (size_t i = 0; i < card->count; i++) {

        settings_row_t* row = &card->rows[i];

        if (i > 0 && (row->rule = settings_own(ui_panel_create(settings.view))) == NULL) {
            return -1;
        }

        if ((row->key = settings_own(ui_label_create(settings.view, keys[i]))) == NULL) {
            return -1;
        }

        if (custom & (1U << i)) {
            continue;
        }

        if ((row->value = settings_own(ui_label_create(settings.view, SETTINGS_UNKNOWN))) == NULL) {
            return -1;
        }

        ui_label_set_align(row->value, UI_ALIGN_RIGHT);

        snprintf(row->text, sizeof(row->text), SETTINGS_UNKNOWN);
    }

    return 0;
}


/**
 * @brief Reports where a row's value goes, which is where a custom row puts its own widgets.
 *
 * @param card The card, already placed.
 * @param row The row.
 * @return The rectangle right of the row's name.
 */
ui_rect_t settings_card_value(const settings_card_t* card, size_t row) {

    const ui_rect_t panel = ui_widget_rect(card->panel);
    const int height      = settings_row_height();
    const int left        = SETTINGS_CARD_PADDING + SETTINGS_KEY_WIDTH + SETTINGS_GAP;
    const int width       = panel.width - left - SETTINGS_CARD_PADDING;

    return (ui_rect_t){panel.x + left, panel.y + (int)row * height, width > 0 ? width : 0, height};
}


/**
 * @brief Places a card's panel and rows at the top of a strip.
 *
 * @param card The card.
 * @param x The left edge.
 * @param y The top edge.
 * @param width The width.
 * @return The card's height.
 */
int settings_card_place(settings_card_t* card, int x, int y, int width) {

    const int row_height = settings_row_height();
    const int inner      = width - 2 * SETTINGS_CARD_PADDING;

    ui_widget_set_rect(card->panel, x, y, width, (int)card->count * row_height);

    for (size_t i = 0; i < card->count; i++) {

        settings_row_t* row = &card->rows[i];

        const int top = y + (int)i * row_height;

        if (row->rule) {
            ui_widget_set_rect(row->rule, x + SETTINGS_CARD_PADDING, top, inner > 0 ? inner : 0, 1);
        }

        ui_widget_set_rect(row->key, x + SETTINGS_CARD_PADDING, top, SETTINGS_KEY_WIDTH, row_height);

        if (row->value) {
            ui_widget_place(row->value, settings_card_value(card, i));
            settings_fit(row->value, row->text);
        }
    }

    return (int)card->count * row_height;
}


/**
 * @brief Gives a card the colours and corner of the theme in use.
 *
 * @param card The card.
 */
void settings_card_restyle(settings_card_t* card) {

    ui_panel_set_color(card->panel, ui_theme()->surface);
    ui_panel_set_border(card->panel, ui_theme()->border, 1.0);
    ui_panel_set_radius(card->panel, ui_theme()->corner_radius);

    for (size_t i = 0; i < card->count; i++) {

        if (card->rows[i].rule) {
            ui_panel_set_color(card->rows[i].rule, ui_theme()->border);
            ui_panel_set_radius(card->rows[i].rule, 0.0);
        }

        ui_label_set_color(card->rows[i].key, ui_theme()->text);

        if (card->rows[i].value) {
            ui_label_set_color(card->rows[i].value, ui_theme()->text);
        }
    }
}


/**
 * @brief Changes a row's value.
 *
 * @param row The row.
 * @param text The new value.
 */
void settings_row_set(settings_row_t* row, const char* text) {

    if (!row->value || strcmp(row->text, text) == 0) {
        return;
    }

    snprintf(row->text, sizeof(row->text), "%s", text);
    settings_fit(row->value, row->text);
}


/**
 * @brief Formats a byte count in binary units, with one decimal below ten.
 *
 * @param bytes The count.
 * @param out Receives the text.
 * @param size The size of @p out.
 */
void settings_format_size(uint64_t bytes, char* out, size_t size) {

    static const char* const units[] = {"KiB", "MiB", "GiB", "TiB"};

    if (bytes < 1024) {
        snprintf(out, size, "%llu bytes", (unsigned long long)bytes);
        return;
    }

    double value = (double)bytes / 1024.0;
    size_t unit  = 0;

    while (value >= 1024.0 && unit + 1 < sizeof(units) / sizeof(units[0])) {
        value /= 1024.0;
        unit++;
    }

    snprintf(out, size, value < 10.0 ? "%.1f %s" : "%.0f %s", value, units[unit]);
}


/**
 * @brief Describes a control request that could not be made, or that the server refused.
 *
 * @param out Receives the message.
 * @param size The size of @p out.
 * @param failed Whether the request could not be made at all, errno saying why.
 * @param reply The server's answer when it did answer.
 */
void settings_control_error(char* out, size_t size, bool failed, const ui_msg_ctl_reply_t* reply) {

    if (failed) {
        snprintf(out, size, "Cannot reach the display server: %s", strerror(errno));
    } else {
        snprintf(out, size, "%s", reply->text[0] ? reply->text : strerror(reply->status));
    }
}


/**
 * @brief Replaces a file's contents in one rename, so that a crash leaves the old file or the new one.
 *
 * @param path The file.
 * @param text The new contents.
 * @return 0, or -1 with errno set.
 */
int settings_write_file(const char* path, const char* text) {

    char staging[256];

    if ((size_t)snprintf(staging, sizeof(staging), "%s.new", path) >= sizeof(staging)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    FILE* fp = fopen(staging, "w");

    if (!fp) {
        return -1;
    }

    const bool written = fputs(text, fp) >= 0;

    if (fclose(fp) != 0 || !written || rename(staging, path) < 0) {

        const int e = errno;

        unlink(staging);

        errno = e;
        return -1;
    }

    return 0;
}


/**
 * @brief Shows one page and hides the others.
 *
 * @param page The page to show.
 */
static void settings_show(size_t page) {

    settings_state.page = page;

    for (size_t p = 0; p < SETTINGS_PAGES; p++) {

        for (size_t i = 0; i < settings_state.owned_count[p]; i++) {
            ui_widget_set_visible(settings_state.owned[p][i], p == page);
        }
    }

    if (settings_pages[page]->enter) {
        settings_pages[page]->enter();
    }

    ui_view_invalidate(settings.view);
}


/**
 * @brief Switches to the page a sidebar row names, keeping one selected whatever is clicked.
 *
 * @param widget The sidebar.
 * @param index The row, or -1 when the selection was cleared.
 * @param user Unused.
 */
static void settings_on_page(ui_widget_t* widget, int index, void* user) {

    (void)user;

    if (index < 0 || (size_t)index >= SETTINGS_PAGES) {
        ui_list_select(widget, (int)settings_state.page);
        return;
    }

    settings_show((size_t)index);
}


/**
 * @brief Places the sidebar down the left edge and every page beside it, run at startup and after every resize.
 *
 * @param view The view being laid out.
 * @param width The view width in pixels.
 * @param height The view height in pixels.
 * @param user Unused.
 */
static void settings_layout(ui_view_t* view, int width, int height, void* user) {

    (void)view;
    (void)user;

    const int inner_width  = width - 2 * SETTINGS_MARGIN;
    const int inner_height = height - 2 * SETTINGS_MARGIN;

    if (inner_width <= 0 || inner_height <= 0) {
        return;
    }

    const int sidebar = SETTINGS_SIDEBAR < inner_width / 3 ? SETTINGS_SIDEBAR : inner_width / 3;

    ui_widget_set_rect(settings_state.sidebar, SETTINGS_MARGIN, SETTINGS_MARGIN, sidebar, inner_height);

    ui_rect_t page = {
        .x      = SETTINGS_MARGIN + sidebar + SETTINGS_SIDEBAR_GAP,
        .y      = SETTINGS_MARGIN + 4,
        .width  = inner_width - sidebar - SETTINGS_SIDEBAR_GAP - SETTINGS_GAP,
        .height = inner_height - 4,
    };

    if (page.width <= 0) {
        return;
    }

    for (size_t i = 0; i < SETTINGS_PAGES; i++) {

        if (settings_pages[i]->layout) {
            settings_pages[i]->layout(page);
        }
    }

    settings_repaint();
}


/**
 * @brief Gives every page the colours and fonts of the theme in use.
 *
 * @param view The view.
 * @param user Unused.
 */
static void settings_restyle(ui_view_t* view, void* user) {

    (void)view;
    (void)user;

    for (size_t i = 0; i < SETTINGS_PAGES; i++) {

        if (settings_pages[i]->restyle) {
            settings_pages[i]->restyle();
        }
    }

    settings_repaint();
}


/**
 * @brief Creates the sidebar and every page, in the order they are painted.
 *
 * @param initial The page to open on.
 * @return 0 on success, -1 otherwise.
 */
static int settings_build(size_t initial) {

    if ((settings_state.sidebar = ui_list_create(settings.view)) == NULL) {
        return -1;
    }

    for (size_t i = 0; i < SETTINGS_PAGES; i++) {

        if (ui_list_add_icon(settings_state.sidebar, settings_pages[i]->icon, settings_pages[i]->label, NULL, NULL) < 0) {
            return -1;
        }

        settings_state.building = i;

        if (settings_pages[i]->build() < 0) {
            ulog_errorf("cannot create the %s page", settings_pages[i]->name);
            return -1;
        }
    }

    settings_restyle(settings.view, NULL);

    settings_state.page = initial;

    ui_list_on_select(settings_state.sidebar, settings_on_page, NULL);
    ui_list_select(settings_state.sidebar, (int)initial);

    settings_show(initial);

    ui_view_focus(settings.view, settings_state.sidebar);

    return 0;
}


/**
 * @brief Hands the page a cairo context over the window to lay its own drawing down with, and commits it.
 *
 * @return 0, or -1 when the commit fails.
 */
static int settings_paint(void) {

    const settings_page_t* page = settings_pages[settings_state.page];

    settings.repaint = false;

    if (!page->paint) {
        return 0;
    }

    cairo_surface_t* surface =
        cairo_image_surface_create_for_data((unsigned char*)ui_window_pixels(settings.window), CAIRO_FORMAT_RGB24, ui_window_width(settings.window), ui_window_height(settings.window), (int)ui_window_stride(settings.window));

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return 0;
    }

    cairo_t* cr = cairo_create(surface);

    page->paint(cr);

    cairo_destroy(cr);

    cairo_surface_flush(surface);
    cairo_surface_destroy(surface);

    return ui_window_commit(settings.window) < 0 ? -1 : 0;
}


/**
 * @brief Runs every page's tick and reports how long the loop may wait: until the view animates or a page wants its next tick.
 *
 * @return Milliseconds to wait, or -1 for as long as it takes.
 */
static int settings_timeout(void) {

    int timeout = ui_view_timeout(settings.view);

    const uint64_t now = ui_anim_now();

    for (size_t i = 0; i < SETTINGS_PAGES; i++) {

        if (!settings_pages[i]->tick) {
            continue;
        }

        const int wait = settings_pages[i]->tick(now, i == settings_state.page);

        if (wait >= 0 && (timeout < 0 || wait < timeout)) {
            timeout = wait;
        }
    }

    return timeout;
}


/**
 * @brief Feeds an event to the view, and a pointer event to the page as well.
 *
 * @param event The event.
 */
static void settings_dispatch(const ui_event_t* event) {

    ui_view_dispatch(settings.view, event);

    const settings_page_t* page = settings_pages[settings_state.page];

    if (page->pointer && (event->type == UI_EVENT_POINTER || event->type == UI_EVENT_LEAVE)) {
        page->pointer(event);
    }
}


/**
 * @brief The event loop: ui_view_run() with each page's own drawing and ticks folded in.
 *
 * @return 0 on a clean close, -1 otherwise.
 */
static int settings_run(void) {

    while (!ui_view_closed(settings.view)) {

        const int timeout = settings_timeout();
        const int painted = ui_view_present(settings.view);

        if (painted < 0) {
            return -1;
        }

        if ((painted > 0 || settings.repaint) && settings_paint() < 0) {
            return -1;
        }


        ui_event_t event;

        const int e = ui_next_event(settings.conn, &event, timeout);

        if (e < 0) {
            return -1;
        }

        if (e == 0) {
            continue;
        }

        settings_dispatch(&event);

        for (size_t drained = 0; drained < SETTINGS_DRAIN_MAX && !ui_view_closed(settings.view); drained++) {

            const int pending = ui_next_event(settings.conn, &event, 0);

            if (pending < 0) {
                return -1;
            }

            if (pending == 0) {
                break;
            }

            settings_dispatch(&event);
        }
    }

    return 0;
}


/**
 * @brief Prints the help and exits.
 *
 * @param status The exit status.
 */
static void show_usage(int status) {

    printf("Use: aplus-settings [page]\n"
           "Change how the system looks and works, and see what it runs on.\n\n"
           "Pages:\n");

    for (size_t i = 0; i < SETTINGS_PAGES; i++) {
        printf("   %s\n", settings_pages[i]->name);
    }

    exit(status);
}


int main(int argc, char** argv) {

    size_t initial = 0;

    if (argc > 2) {
        show_usage(1);
    }

    if (argc == 2) {

        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            show_usage(0);
        }

        while (initial < SETTINGS_PAGES && strcmp(argv[1], settings_pages[initial]->name) != 0) {
            initial++;
        }

        if (initial == SETTINGS_PAGES) {
            show_usage(1);
        }
    }


    if ((settings.conn = ui_connect(NULL, 5000)) == NULL) {
        ulog_errorf("ui_connect() failed: %s", strerror(errno));
        return 1;
    }

    if ((settings.window = ui_window_create(settings.conn, SETTINGS_WINDOW_WIDTH, SETTINGS_WINDOW_HEIGHT, "Settings")) == NULL) {
        ulog_errorf("ui_window_create() failed: %s", strerror(errno));
        ui_disconnect(settings.conn);
        return 1;
    }

    if ((settings.view = ui_view_create(settings.window)) == NULL) {
        ulog_errorf("ui_view_create() failed: %s", strerror(errno));
        ui_window_destroy(settings.window);
        ui_disconnect(settings.conn);
        return 1;
    }

    if (settings_build(initial) < 0) {
        ulog_errorf("cannot create the widgets");
        ui_view_destroy(settings.view);
        ui_window_destroy(settings.window);
        ui_disconnect(settings.conn);
        return 1;
    }

    ui_view_on_layout(settings.view, settings_layout, NULL);
    ui_view_on_theme(settings.view, settings_restyle, NULL);


    const int status = settings_run();

    if (status < 0) {
        ulog_errorf("event loop failed: %s", strerror(errno));
    }

    ui_view_destroy(settings.view);
    ui_window_destroy(settings.window);
    ui_disconnect(settings.conn);

    return status < 0 ? 1 : 0;
}
