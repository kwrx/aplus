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

#ifndef _APLUS_SETTINGS_H
#define _APLUS_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cairo/cairo.h>

#include <aplus/ui-control.h>
#include <aplus/ui-widgets.h>
#include <aplus/ui.h>


#define SETTINGS_MARGIN       12
#define SETTINGS_GAP          8
#define SETTINGS_SECTION_GAP  20
#define SETTINGS_CARD_PADDING 14
#define SETTINGS_KEY_WIDTH    150
#define SETTINGS_CARD_ROWS    8
#define SETTINGS_TEXT_MAX     128
#define SETTINGS_UNKNOWN      "Unknown"
#define SETTINGS_TIMES        "\xC3\x97"


/**
 * @brief One line of a card: a name on the left, a value against the right edge, and a rule above it.
 */
typedef struct {

    ui_widget_t* key;
    ui_widget_t* value;
    ui_widget_t* rule;

    char text[SETTINGS_TEXT_MAX];

} settings_row_t;


/**
 * @brief A raised panel holding rows of names and values.
 */
typedef struct {

    ui_widget_t* panel;

    settings_row_t rows[SETTINGS_CARD_ROWS];
    size_t count;

} settings_card_t;


/**
 * @brief A page's title and the line under it.
 */
typedef struct {

    ui_widget_t* title;
    ui_widget_t* subtitle;

} settings_header_t;


/**
 * @brief A page: its sidebar row and the hooks the window calls; any hook but build may be NULL.
 *
 * tick runs every pass for every page and returns how long it may wait, or -1 for as long as it likes.
 */
typedef struct {

    const char* name;
    const char* label;
    const char* icon;

    int (*build)(void);
    void (*layout)(ui_rect_t page);
    void (*restyle)(void);
    void (*enter)(void);
    int (*tick)(uint64_t now, bool visible);
    void (*paint)(cairo_t* cr);
    void (*pointer)(const ui_event_t* event);

} settings_page_t;


extern const settings_page_t settings_appearance;
extern const settings_page_t settings_wallpaper;
extern const settings_page_t settings_network;
extern const settings_page_t settings_software;
extern const settings_page_t settings_about;


/**
 * @brief The window every page draws into.
 */
typedef struct {

    ui_connection_t* conn;
    ui_window_t* window;
    ui_view_t* view;

    bool repaint;

} settings_t;

extern settings_t settings;


/**
 * @brief Implemented in main.c.
 */
ui_widget_t* settings_own(ui_widget_t* widget);
void settings_repaint(void);

double settings_font_size(void);
double settings_title_size(void);
int settings_line_height(void);
int settings_control_height(void);
int settings_row_height(void);
int settings_text_width(const char* text, ui_font_weight_t weight);
void settings_fit(ui_widget_t* label, const char* text);

ui_widget_t* settings_heading(const char* text);
int settings_header_build(settings_header_t* header, const char* title, const char* subtitle);
int settings_header_place(settings_header_t* header, ui_rect_t page, int reserve);
void settings_header_restyle(settings_header_t* header);

int settings_card_build(settings_card_t* card, const char* const* keys, size_t count, uint32_t custom);
int settings_card_place(settings_card_t* card, int x, int y, int width);
ui_rect_t settings_card_value(const settings_card_t* card, size_t row);
void settings_card_restyle(settings_card_t* card);
void settings_row_set(settings_row_t* row, const char* text);

void settings_format_size(uint64_t bytes, char* out, size_t size);
void settings_control_error(char* out, size_t size, bool failed, const ui_msg_ctl_reply_t* reply);
int settings_write_file(const char* path, const char* text);

#endif
