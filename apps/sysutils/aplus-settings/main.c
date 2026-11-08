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
#include <aplus/input.h>
#include <aplus/ulog.h>

#include "settings.h"


#define SETTINGS_WINDOW_WIDTH  820
#define SETTINGS_WINDOW_HEIGHT 580

#define SETTINGS_SIDEBAR      224
#define SETTINGS_NAV_ICON     20
#define SETTINGS_NAV_SPACING  4
#define SETTINGS_SIDEBAR_GAP  24
#define SETTINGS_PAGE_WIDGETS 128
#define SETTINGS_DRAIN_MAX    64


/**
 * @brief The pages, in the order the sidebar lists them.
 */
static const settings_page_t* const settings_pages[] = {
    &settings_appearance, &settings_wallpaper, &settings_network, &settings_audio, &settings_software, &settings_about,
};

#define SETTINGS_PAGES (sizeof(settings_pages) / sizeof(settings_pages[0]))



settings_t settings;


/**
 * @brief The sidebar, the page on show, and the widgets each page owns.
 */
static struct {

    ui_widget_t* sidebar;
    ui_widget_t* nav[SETTINGS_PAGES];
    ui_widget_t* rules[SETTINGS_PAGES];
    ui_widget_t* thumb;

    ui_rect_t rect;
    settings_scroller_t scroller;

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

    for (size_t p = 0; p < SETTINGS_PAGES; p++) {
        ui_button_set_style(settings_state.nav[p], p == page ? UI_BUTTON_STYLE_PRIMARY : UI_BUTTON_STYLE_FLAT);
    }

    if (settings_pages[page]->enter) {
        settings_pages[page]->enter();
    }

    ui_view_invalidate(settings.view);
}


static void settings_on_page(ui_widget_t* widget, void* user) {

    (void)widget;

    const size_t page = (size_t)(intptr_t)user;

    if (page != settings_state.page) {
        settings_show(page);
    }
}


static bool settings_on_key(ui_view_t* view, uint16_t vkey, bool down, void* user) {

    (void)user;

    if (!down || (vkey != KEY_UP && vkey != KEY_DOWN)) {
        return false;
    }

    const ui_widget_t* focused = ui_view_focused(view);

    bool nav = focused == NULL;

    for (size_t p = 0; p < SETTINGS_PAGES && !nav; p++) {
        nav = focused == settings_state.nav[p];
    }

    if (!nav) {
        return false;
    }

    if (vkey == KEY_UP && settings_state.page > 0) {
        settings_show(settings_state.page - 1);
    } else if (vkey == KEY_DOWN && settings_state.page + 1 < SETTINGS_PAGES) {
        settings_show(settings_state.page + 1);
    }

    return true;
}


static int settings_nav_row(void) {
    return settings_control_height() + 2 * SETTINGS_GAP;
}


static int settings_nav_content(void) {
    return (int)SETTINGS_PAGES * settings_nav_row() + (int)(SETTINGS_PAGES - 1) * (2 * SETTINGS_NAV_SPACING + 1) + SETTINGS_GAP;
}


static void settings_arrange_sidebar(void) {

    const ui_rect_t card = settings_state.rect;
    const ui_rect_t view = settings_state.scroller.view;

    const int row   = settings_nav_row();
    const int right = settings_scroller_scrollable(&settings_state.scroller) ? SETTINGS_SCROLL_TRACK : 0;
    const int width = card.width - 2 * SETTINGS_GAP - right;

    int top = view.y + SETTINGS_GAP / 2 - settings_state.scroller.offset;

    for (size_t p = 0; p < SETTINGS_PAGES; p++) {

        if (p > 0) {

            top += SETTINGS_NAV_SPACING;

            ui_widget_set_rect(settings_state.rules[p], card.x + SETTINGS_CARD_PADDING, top, card.width - 2 * SETTINGS_CARD_PADDING - right, 1);
            ui_widget_set_clip(settings_state.rules[p], &view);

            top += 1 + SETTINGS_NAV_SPACING;
        }

        ui_widget_set_rect(settings_state.nav[p], card.x + SETTINGS_GAP, top, width > 0 ? width : 0, row);
        ui_widget_set_clip(settings_state.nav[p], &view);

        top += row;
    }
}


static void settings_place_sidebar(int x, int y, int width, int height) {

    settings_state.rect = (ui_rect_t){x, y, width, height};

    ui_widget_set_rect(settings_state.sidebar, x, y, width, height);

    const ui_rect_t view = {x, y + SETTINGS_GAP / 2, width, height - SETTINGS_GAP};

    settings_scroller_set(&settings_state.scroller, view, x + width - SETTINGS_SCROLL_TRACK - 2, settings_nav_content());
    settings_arrange_sidebar();
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

    settings_place_sidebar(SETTINGS_MARGIN, SETTINGS_MARGIN, sidebar, inner_height);

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

    ui_panel_set_color(settings_state.sidebar, ui_theme()->surface);
    ui_panel_set_border(settings_state.sidebar, ui_theme()->border, 1.0);
    ui_panel_set_radius(settings_state.sidebar, ui_theme()->corner_radius);

    for (size_t p = 1; p < SETTINGS_PAGES; p++) {
        ui_panel_set_color(settings_state.rules[p], ui_theme()->border);
        ui_panel_set_radius(settings_state.rules[p], 0.0);
    }

    settings_scroller_set(&settings_state.scroller, settings_state.scroller.view, settings_state.scroller.track_x, settings_state.scroller.content);

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

    if ((settings_state.sidebar = ui_panel_create(settings.view)) == NULL) {
        return -1;
    }

    for (size_t i = 0; i < SETTINGS_PAGES; i++) {

        if ((settings_state.nav[i] = ui_button_create(settings.view, settings_pages[i]->label, settings_on_page, (void*)(intptr_t)i)) == NULL) {
            return -1;
        }

        ui_button_set_icon(settings_state.nav[i], settings_pages[i]->icon);
        ui_button_set_icon_size(settings_state.nav[i], SETTINGS_NAV_ICON);
        ui_button_set_align(settings_state.nav[i], UI_ALIGN_LEFT);
        ui_button_set_style(settings_state.nav[i], UI_BUTTON_STYLE_FLAT);
    }

    for (size_t p = 1; p < SETTINGS_PAGES; p++) {

        if ((settings_state.rules[p] = ui_panel_create(settings.view)) == NULL) {
            return -1;
        }
    }

    if ((settings_state.thumb = ui_panel_create(settings.view)) == NULL) {
        return -1;
    }

    settings_scroller_init(&settings_state.scroller, settings_state.thumb, NULL, settings_arrange_sidebar);

    for (size_t i = 0; i < SETTINGS_PAGES; i++) {

        settings_state.building = i;

        if (settings_pages[i]->build() < 0) {
            ulog_errorf("cannot create the %s page", settings_pages[i]->name);
            return -1;
        }
    }

    settings_restyle(settings.view, NULL);

    settings_state.page = initial;

    settings_show(initial);

    ui_view_on_key(settings.view, settings_on_key, NULL);

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

    const bool handled = ui_view_dispatch(settings.view, event);

    const settings_page_t* page = settings_pages[settings_state.page];

    settings_scroller_pointer(&settings_state.scroller, event);

    if (page->pointer && (event->type == UI_EVENT_POINTER || event->type == UI_EVENT_LEAVE)) {
        page->pointer(event);
    }

    if (event->type == UI_EVENT_SCROLL && !handled && !settings_scroller_wheel(&settings_state.scroller, event) && page->scroll) {
        page->scroll(event);
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
