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
 * @brief The Appearance page: the theme and its variant, and the fonts and sizes laid over whichever theme is in use.
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <aplus/ulog.h>

#include "settings.h"


#define APPEARANCE_THEMES_MAX   12
#define APPEARANCE_FAMILIES_MAX 8
#define APPEARANCE_COLUMNS      4
#define APPEARANCE_FAMILY_MAX   64
#define APPEARANCE_FONT_DIR     "/usr/share/fonts/ttf"
#define APPEARANCE_THEME_SUFFIX ".conf"
#define APPEARANCE_SIZE_MIN     9
#define APPEARANCE_SIZE_MAX     28
#define APPEARANCE_MONO_DEFAULT 14
#define APPEARANCE_MINUS        "\xE2\x88\x92"


/**
 * @brief Which of the two font groups.
 */
typedef enum {

    APPEARANCE_INTERFACE = 0,
    APPEARANCE_MONOSPACE,

    APPEARANCE_GROUPS,

} appearance_group_t;


/**
 * @brief An installed font family, by the files of the three weights the toolkit draws with.
 */
typedef struct {

    char name[APPEARANCE_FAMILY_MAX];

    char regular[UI_THEME_PATH_MAX];
    char medium[UI_THEME_PATH_MAX];
    char bold[UI_THEME_PATH_MAX];

    ui_widget_t* tile;

} appearance_family_t;


/**
 * @brief The families of one group, and the heading and size stepper above their tiles.
 */
typedef struct {

    ui_widget_t* heading;
    ui_widget_t* smaller;
    ui_widget_t* size;
    ui_widget_t* larger;

    appearance_family_t families[APPEARANCE_FAMILIES_MAX];
    size_t count;

} appearance_fonts_t;


/**
 * @brief The variants a theme comes in, as the server spells them and as their tiles say.
 */
static const char* const appearance_variants[][2] = {
    {UI_THEME_VARIANT_DARK, "Dark"},
    {UI_THEME_VARIANT_LIGHT, "Light"},
};

#define APPEARANCE_VARIANTS (sizeof(appearance_variants) / sizeof(appearance_variants[0]))


/**
 * @brief The headings the font groups are shown under.
 */
static const char* const appearance_group_names[APPEARANCE_GROUPS] = {
    "Interface font",
    "Monospace font",
};


/**
 * @brief The page's widgets, the installed themes and the installed font families.
 */
static struct {

    settings_header_t header;

    ui_widget_t* theme_heading;
    ui_widget_t* status;

    ui_widget_t* variants[APPEARANCE_VARIANTS];

    char themes[APPEARANCE_THEMES_MAX][UI_THEME_NAME_MAX];
    ui_widget_t* tiles[APPEARANCE_THEMES_MAX];
    size_t theme_count;

    appearance_fonts_t fonts[APPEARANCE_GROUPS];

} appearance;


/**
 * @brief Shows a line at the foot of the page, or clears it.
 *
 * @param text The line, or "" for none.
 */
static void appearance_report(const char* text) {
    settings_fit(appearance.status, text);
}


/**
 * @brief Fills in one key of a control request.
 *
 * @param option The key to fill in.
 * @param section The section.
 * @param key The key.
 * @param value The value.
 */
static void appearance_option(ui_msg_ctl_option_t* option, const char* section, const char* key, const char* value) {

    memset(option, 0, sizeof(*option));

    snprintf(option->section, sizeof(option->section), "%s", section);
    snprintf(option->key, sizeof(option->key), "%s", key);
    snprintf(option->value, sizeof(option->value), "%s", value);
}


/**
 * @brief Sends keys to the server, reporting a refusal at the foot of the page.
 *
 * @param options The keys.
 * @param count How many there are.
 */
static void appearance_apply(const ui_msg_ctl_option_t* options, size_t count) {

    ui_msg_ctl_reply_t reply;

    memset(&reply, 0, sizeof(reply));

    if (ui_control_set_options(options, count, &reply) < 0) {

        char text[SETTINGS_TEXT_MAX];

        settings_control_error(text, sizeof(text), reply.status == 0, &reply);
        appearance_report(text);

        return;
    }

    appearance_report("");
}


/**
 * @brief Switches the system to the theme a tile names.
 *
 * @param widget The tile.
 * @param user The theme's index.
 */
static void appearance_on_theme(ui_widget_t* widget, void* user) {

    (void)widget;

    const char* name = appearance.themes[(size_t)(intptr_t)user];

    if (strcmp(name, ui_theme_name()) == 0) {
        return;
    }

    ui_msg_ctl_reply_t reply;

    memset(&reply, 0, sizeof(reply));

    if (ui_control_set_theme(name, &reply) < 0) {

        char text[SETTINGS_TEXT_MAX];

        settings_control_error(text, sizeof(text), reply.status == 0, &reply);
        appearance_report(text);

        return;
    }

    appearance_report("");
}


/**
 * @brief Switches every theme to the variant a tile names.
 *
 * @param widget The tile.
 * @param user The variant's index.
 */
static void appearance_on_variant(ui_widget_t* widget, void* user) {

    (void)widget;

    const char* variant = appearance_variants[(size_t)(intptr_t)user][0];

    if (strcmp(variant, ui_theme_variant()) == 0) {
        return;
    }

    ui_msg_ctl_option_t option;

    appearance_option(&option, "wm", "variant", variant);
    appearance_apply(&option, 1);
}


/**
 * @brief Lays a family a tile names over the theme, for the interface or the monospace text.
 *
 * @param widget The tile.
 * @param user The group in the high bits and the family's index in the low byte.
 */
static void appearance_on_family(ui_widget_t* widget, void* user) {

    (void)widget;

    const size_t group                = (size_t)(intptr_t)user >> 8;
    const appearance_family_t* family = &appearance.fonts[group].families[(size_t)(intptr_t)user & 0xFF];

    ui_msg_ctl_option_t options[4];

    if (group == APPEARANCE_MONOSPACE) {

        appearance_option(&options[0], "ui", "font_mono", family->regular);
        appearance_option(&options[1], "ui", "font_mono_bold", family->bold);

        appearance_apply(options, 2);

    } else {

        appearance_option(&options[0], "ui", "font_regular", family->regular);
        appearance_option(&options[1], "ui", "font_medium", family->medium);
        appearance_option(&options[2], "ui", "font_bold", family->bold);
        appearance_option(&options[3], "wm", "font", family->regular);

        appearance_apply(options, 4);
    }
}


/**
 * @brief Reports a group's text size, the toolkit's default standing in for a monospace size the theme leaves out.
 *
 * @param group The group.
 * @return The size in pixels.
 */
static int appearance_size(size_t group) {

    if (group == APPEARANCE_MONOSPACE) {
        return ui_theme()->font_mono_size > 0.0 ? (int)(ui_theme()->font_mono_size + 0.5) : APPEARANCE_MONO_DEFAULT;
    }

    return (int)(settings_font_size() + 0.5);
}


/**
 * @brief Makes a group's text a pixel smaller or larger.
 *
 * @param widget The button.
 * @param user The group in the high bits, and 1 for larger or 0 for smaller in the low bit.
 */
static void appearance_on_size(ui_widget_t* widget, void* user) {

    (void)widget;

    const size_t group = (size_t)(intptr_t)user >> 8;
    const int step     = ((size_t)(intptr_t)user & 1) ? 1 : -1;

    int size = appearance_size(group) + step;

    if (size < APPEARANCE_SIZE_MIN || size > APPEARANCE_SIZE_MAX) {
        return;
    }

    char value[16];

    snprintf(value, sizeof(value), "%d", size);

    ui_msg_ctl_option_t option;

    appearance_option(&option, "ui", group == APPEARANCE_MONOSPACE ? "font_mono_size" : "font_size", value);
    appearance_apply(&option, 1);
}


/**
 * @brief Orders two names alphabetically, for qsort().
 *
 * @param a The first.
 * @param b The second.
 * @return Less than, equal to or greater than 0, as strcmp().
 */
static int appearance_compare(const void* a, const void* b) {
    return strcmp((const char*)a, (const char*)b);
}


/**
 * @brief Orders two families by name, for qsort().
 *
 * @param a The first.
 * @param b The second.
 * @return Less than, equal to or greater than 0, as strcmp().
 */
static int appearance_compare_families(const void* a, const void* b) {
    return strcmp(((const appearance_family_t*)a)->name, ((const appearance_family_t*)b)->name);
}


/**
 * @brief Collects the names of the themes installed in UI_THEME_DIR, sorted.
 */
static void appearance_scan_themes(void) {

    DIR* dir = opendir(UI_THEME_DIR);

    if (!dir) {
        ulog_warnf("cannot open %s: %s", UI_THEME_DIR, strerror(errno));
        return;
    }

    const size_t suffix = strlen(APPEARANCE_THEME_SUFFIX);

    struct dirent* ent;

    while ((ent = readdir(dir)) && appearance.theme_count < APPEARANCE_THEMES_MAX) {

        const size_t length = strlen(ent->d_name);

        if (ent->d_name[0] == '.' || length <= suffix || length - suffix >= UI_THEME_NAME_MAX) {
            continue;
        }

        if (strcmp(ent->d_name + length - suffix, APPEARANCE_THEME_SUFFIX) != 0) {
            continue;
        }

        snprintf(appearance.themes[appearance.theme_count++], UI_THEME_NAME_MAX, "%.*s", (int)(length - suffix), ent->d_name);
    }

    closedir(dir);

    qsort(appearance.themes, appearance.theme_count, sizeof(appearance.themes[0]), appearance_compare);
}


/**
 * @brief Files one font into the family it belongs to, by the weight its style names.
 *
 * @param group The group the font belongs to.
 * @param family The family's name.
 * @param style The style's name.
 * @param path The font file.
 */
static void appearance_add_font(appearance_fonts_t* group, const char* family, const char* style, const char* path) {

    char* slot = NULL;

    size_t i = 0;

    while (i < group->count && strcmp(group->families[i].name, family) != 0) {
        i++;
    }

    if (i == group->count) {

        if (group->count == APPEARANCE_FAMILIES_MAX) {
            return;
        }

        memset(&group->families[i], 0, sizeof(group->families[i]));
        snprintf(group->families[i].name, sizeof(group->families[i].name), "%s", family);

        group->count++;
    }

    if (strcmp(style, "Regular") == 0) {
        slot = group->families[i].regular;
    } else if (strcmp(style, "Medium") == 0) {
        slot = group->families[i].medium;
    } else if (strcmp(style, "Bold") == 0) {
        slot = group->families[i].bold;
    }

    if (slot && strlen(path) < UI_THEME_PATH_MAX) {
        snprintf(slot, UI_THEME_PATH_MAX, "%s", path);
    }
}


/**
 * @brief Finds the font families installed in APPEARANCE_FONT_DIR, keeping those with a regular weight.
 */
static void appearance_scan_fonts(void) {

    FT_Library library;

    if (FT_Init_FreeType(&library) != 0) {
        return;
    }

    DIR* dir = opendir(APPEARANCE_FONT_DIR);

    if (!dir) {
        FT_Done_FreeType(library);
        return;
    }

    struct dirent* ent;

    while ((ent = readdir(dir))) {

        const char* dot = strrchr(ent->d_name, '.');

        if (ent->d_name[0] == '.' || !dot || (strcasecmp(dot, ".ttf") != 0 && strcasecmp(dot, ".otf") != 0)) {
            continue;
        }

        char path[UI_THEME_PATH_MAX];

        if ((size_t)snprintf(path, sizeof(path), "%s/%s", APPEARANCE_FONT_DIR, ent->d_name) >= sizeof(path)) {
            continue;
        }

        FT_Face face;

        if (FT_New_Face(library, path, 0, &face) != 0) {
            continue;
        }

        if (face->family_name && face->style_name) {

            const bool mono = FT_IS_FIXED_WIDTH(face) || strstr(face->family_name, "Mono");

            appearance_add_font(&appearance.fonts[mono ? APPEARANCE_MONOSPACE : APPEARANCE_INTERFACE], face->family_name, face->style_name, path);
        }

        FT_Done_Face(face);
    }

    closedir(dir);

    FT_Done_FreeType(library);


    for (size_t g = 0; g < APPEARANCE_GROUPS; g++) {

        appearance_fonts_t* group = &appearance.fonts[g];

        size_t kept = 0;

        for (size_t i = 0; i < group->count; i++) {

            appearance_family_t* family = &group->families[i];

            if (!family->regular[0]) {
                continue;
            }

            if (!family->medium[0]) {
                memcpy(family->medium, family->bold[0] ? family->bold : family->regular, sizeof(family->medium));
            }

            if (!family->bold[0]) {
                memcpy(family->bold, family->regular, sizeof(family->bold));
            }

            group->families[kept++] = *family;
        }

        group->count = kept;

        qsort(group->families, group->count, sizeof(group->families[0]), appearance_compare_families);
    }
}


/**
 * @brief Creates the page's widgets: a tile per installed theme and per font family, and a size stepper per group.
 *
 * @return 0 on success, -1 otherwise.
 */
static int appearance_build(void) {

    if (settings_header_build(&appearance.header, "Appearance", "The theme every window is drawn with, and the fonts laid over it.") < 0) {
        return -1;
    }

    appearance.theme_heading = settings_heading("Theme");
    if (!appearance.theme_heading) {
        return -1;
    }


    appearance_scan_themes();

    for (size_t i = 0; i < appearance.theme_count; i++) {

        char label[UI_THEME_NAME_MAX];

        snprintf(label, sizeof(label), "%s", appearance.themes[i]);
        label[0] = (char)toupper((unsigned char)label[0]);

        if ((appearance.tiles[i] = settings_own(ui_button_create(settings.view, label, appearance_on_theme, (void*)(intptr_t)i))) == NULL) {
            return -1;
        }
    }


    for (size_t i = 0; i < APPEARANCE_VARIANTS; i++) {

        if ((appearance.variants[i] = settings_own(ui_button_create(settings.view, appearance_variants[i][1], appearance_on_variant, (void*)(intptr_t)i))) == NULL) {
            return -1;
        }
    }


    appearance_scan_fonts();

    for (size_t g = 0; g < APPEARANCE_GROUPS; g++) {

        appearance_fonts_t* group = &appearance.fonts[g];

        group->heading = settings_heading(appearance_group_names[g]);
        group->smaller = settings_own(ui_button_create(settings.view, APPEARANCE_MINUS, appearance_on_size, (void*)(intptr_t)(g << 8)));
        group->size    = settings_own(ui_label_create(settings.view, ""));
        group->larger  = settings_own(ui_button_create(settings.view, "+", appearance_on_size, (void*)(intptr_t)((g << 8) | 1)));

        if (!group->heading || !group->smaller || !group->size || !group->larger) {
            return -1;
        }

        ui_label_set_align(group->size, UI_ALIGN_CENTER);

        for (size_t i = 0; i < group->count; i++) {

            group->families[i].tile = settings_own(ui_button_create(settings.view, group->families[i].name, appearance_on_family, (void*)(intptr_t)((g << 8) | i)));

            if (!group->families[i].tile) {
                return -1;
            }
        }
    }


    if ((appearance.status = settings_own(ui_label_create(settings.view, ""))) == NULL) {
        return -1;
    }

    if (appearance.theme_count == 0) {
        appearance_report("No themes are installed in " UI_THEME_DIR ".");
    }

    return 0;
}


/**
 * @brief Places tiles in rows of up to APPEARANCE_COLUMNS, fewer when the longest name would not fit, sharing the width between them.
 *
 * @param tiles The tiles.
 * @param names What each tile says.
 * @param count How many there are.
 * @param x The left edge.
 * @param y The top edge.
 * @param width The width.
 * @return The height the tiles take, 0 for none.
 */
static int appearance_place_tiles(ui_widget_t* const* tiles, const char* const* names, size_t count, int x, int y, int width) {

    if (count == 0) {
        return 0;
    }

    int widest = 0;

    for (size_t i = 0; i < count; i++) {

        const int w = settings_text_width(names[i], UI_FONT_REGULAR) + 2 * SETTINGS_CARD_PADDING;

        if (w > widest) {
            widest = w;
        }
    }

    int columns = count < APPEARANCE_COLUMNS ? (int)count : APPEARANCE_COLUMNS;

    while (columns > 1 && (width - (columns - 1) * SETTINGS_GAP) / columns < widest) {
        columns--;
    }

    const int tile = (int)(settings_font_size() * 3.0 + 0.5);
    const int rows = ((int)count + columns - 1) / columns;

    const ui_rect_t area = {x, y, width, rows * tile + (rows - 1) * SETTINGS_GAP};

    ui_grid_t grid = ui_grid(area, columns, rows, SETTINGS_GAP);

    for (size_t i = 0; i < count; i++) {
        ui_widget_place(tiles[i], ui_grid_cell(&grid, (int)i % columns, (int)i / columns, 1, 1));
    }

    return area.height;
}


/**
 * @brief Lays out the theme tiles under their heading and variant switch, each font group under its heading and stepper, and the foot.
 *
 * @param page The page's rectangle.
 */
static void appearance_layout(ui_rect_t page) {

    const int control = settings_control_height();
    const int right   = page.x + page.width;

    int y = settings_header_place(&appearance.header, page, 0);


    int toggle = 0;

    for (size_t i = 0; i < APPEARANCE_VARIANTS; i++) {

        const int w = settings_text_width(appearance_variants[i][1], UI_FONT_MEDIUM) + 2 * SETTINGS_CARD_PADDING;

        if (w > toggle) {
            toggle = w;
        }
    }

    for (size_t i = 0; i < APPEARANCE_VARIANTS; i++) {
        ui_widget_set_rect(appearance.variants[i], right - (int)(APPEARANCE_VARIANTS - i) * toggle - (int)(APPEARANCE_VARIANTS - 1 - i) * SETTINGS_GAP, y, toggle, control);
    }

    ui_widget_set_rect(appearance.theme_heading, page.x, y, page.width - (int)APPEARANCE_VARIANTS * (toggle + SETTINGS_GAP), control);

    y += control + SETTINGS_GAP;
    const char* themes[APPEARANCE_THEMES_MAX];

    for (size_t i = 0; i < appearance.theme_count; i++) {
        themes[i] = appearance.themes[i];
    }

    y += appearance_place_tiles(appearance.tiles, themes, appearance.theme_count, page.x, y, page.width) + SETTINGS_SECTION_GAP;


    const int size = settings_text_width("28 px", UI_FONT_REGULAR) + 2 * SETTINGS_GAP;

    for (size_t g = 0; g < APPEARANCE_GROUPS; g++) {

        appearance_fonts_t* group = &appearance.fonts[g];

        ui_widget_set_rect(group->larger, right - control, y, control, control);
        ui_widget_set_rect(group->size, right - control - size, y, size, control);
        ui_widget_set_rect(group->smaller, right - 2 * control - size, y, control, control);
        ui_widget_set_rect(group->heading, page.x, y, page.width - 2 * control - size - SETTINGS_GAP, control);

        ui_widget_t* tiles[APPEARANCE_FAMILIES_MAX];
        const char* names[APPEARANCE_FAMILIES_MAX];

        for (size_t i = 0; i < group->count; i++) {
            tiles[i] = group->families[i].tile;
            names[i] = group->families[i].name;
        }

        y += control + SETTINGS_GAP;
        y += appearance_place_tiles(tiles, names, group->count, page.x, y, page.width) + SETTINGS_SECTION_GAP;
    }


    ui_widget_set_rect(appearance.status, page.x, y, page.width, control);
}


/**
 * @brief Highlights the theme, the variant and the families in use, and shows the sizes in use.
 */
static void appearance_restyle(void) {

    settings_header_restyle(&appearance.header);

    ui_label_set_color(appearance.status, ui_theme()->text_muted);

    for (size_t i = 0; i < appearance.theme_count; i++) {
        ui_button_set_style(appearance.tiles[i], strcmp(appearance.themes[i], ui_theme_name()) == 0 ? UI_BUTTON_STYLE_PRIMARY : UI_BUTTON_STYLE_DEFAULT);
    }

    for (size_t i = 0; i < APPEARANCE_VARIANTS; i++) {
        ui_button_set_style(appearance.variants[i], strcmp(appearance_variants[i][0], ui_theme_variant()) == 0 ? UI_BUTTON_STYLE_PRIMARY : UI_BUTTON_STYLE_DEFAULT);
    }

    for (size_t g = 0; g < APPEARANCE_GROUPS; g++) {

        appearance_fonts_t* group = &appearance.fonts[g];

        const char* used = g == APPEARANCE_MONOSPACE ? ui_theme()->font_mono : ui_theme()->font_regular;

        for (size_t i = 0; i < group->count; i++) {
            ui_button_set_style(group->families[i].tile, used && strcmp(used, group->families[i].regular) == 0 ? UI_BUTTON_STYLE_PRIMARY : UI_BUTTON_STYLE_DEFAULT);
        }

        char text[32];

        if (g == APPEARANCE_MONOSPACE && ui_theme()->font_mono_size <= 0.0) {
            snprintf(text, sizeof(text), "Default");
        } else {
            snprintf(text, sizeof(text), "%d px", appearance_size(g));
        }

        ui_label_set_text(group->size, text);
    }
}


/**
 * @brief Clears whatever the foot said the last time the page was shown.
 */
static void appearance_enter(void) {

    if (appearance.theme_count > 0) {
        appearance_report("");
    }
}


/**
 * @brief The Appearance page.
 */
const settings_page_t settings_appearance = {
    .name    = "appearance",
    .label   = "Appearance",
    .icon    = "preferences-desktop-theme",
    .build   = appearance_build,
    .layout  = appearance_layout,
    .restyle = appearance_restyle,
    .enter   = appearance_enter,
};
