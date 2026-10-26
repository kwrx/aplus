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
 * @brief The theme: read from UI_THEME_LINK at startup, switched at runtime, and broadcast to every client.
 *
 * A theme file is `key = value` lines under `[ui]` and `[wm]` sections; whatever it leaves out
 * keeps the built-in value, and a missing file means the built-in theme as a whole.
 */

#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <aplus/ulog.h>

#include <wm.h>


/**
 * @brief How long a line of a theme file may be, a section name in it, and the path of a theme file.
 */
#define WM_THEME_LINE_MAX    512
#define WM_THEME_SECTION_MAX 16
#define WM_THEME_FILE_MAX    256

/**
 * @brief What the name of the built-in theme is reported as.
 */
#define WM_THEME_BUILTIN "default"


_Static_assert(sizeof(ui_msg_color_t) == sizeof(ui_color_t), "a wire colour has to be laid out like a ui_color_t");


/**
 * @brief What the value of a key holds; an optional path may be left empty.
 */
typedef enum {

    WM_THEME_COLOR = 0,
    WM_THEME_NUMBER,
    WM_THEME_PATH,
    WM_THEME_PATH_OPTIONAL,

} wm_theme_kind_t;


/**
 * @brief A key a theme file may set, and where in wm_theme_t its value lands.
 */
typedef struct {

    const char* section;
    const char* name;

    wm_theme_kind_t kind;

    size_t offset;
    size_t size;

    double min;
    double max;

} wm_theme_key_t;


#define WM_THEME_KEY(section, name, kind, field, min, max) {section, name, kind, offsetof(wm_theme_t, field), sizeof(((wm_theme_t*)0)->field), min, max}

#define WM_THEME_UI_COLOR(field)            WM_THEME_KEY("ui", #field, WM_THEME_COLOR, client.field, 0.0, 0.0)
#define WM_THEME_UI_NUMBER(field, min, max) WM_THEME_KEY("ui", #field, WM_THEME_NUMBER, client.field, min, max)
#define WM_THEME_UI_PATH(field, kind)       WM_THEME_KEY("ui", #field, kind, client.field, 0.0, 0.0)
#define WM_THEME_WM_COLOR(field)            WM_THEME_KEY("wm", #field, WM_THEME_COLOR, field, 0.0, 0.0)
#define WM_THEME_WM_NUMBER(field, min, max) WM_THEME_KEY("wm", #field, WM_THEME_NUMBER, field, min, max)
#define WM_THEME_WM_PATH(field, kind)       WM_THEME_KEY("wm", #field, kind, field, 0.0, 0.0)


/**
 * @brief Every key a theme file may set: `[ui]` is what clients are sent, `[wm]` what the server draws itself.
 */
static const wm_theme_key_t wm_theme_keys[] = {

    WM_THEME_UI_COLOR(background),
    WM_THEME_UI_COLOR(surface),
    WM_THEME_UI_COLOR(surface_sunken),
    WM_THEME_UI_COLOR(primary),
    WM_THEME_UI_COLOR(on_primary),
    WM_THEME_UI_COLOR(secondary),
    WM_THEME_UI_COLOR(on_secondary),
    WM_THEME_UI_COLOR(danger),
    WM_THEME_UI_COLOR(on_danger),
    WM_THEME_UI_COLOR(hover),
    WM_THEME_UI_COLOR(active),
    WM_THEME_UI_COLOR(border),
    WM_THEME_UI_COLOR(focus_ring),
    WM_THEME_UI_COLOR(text),
    WM_THEME_UI_COLOR(text_muted),
    WM_THEME_UI_NUMBER(disabled_fade, 0.0, 1.0),
    WM_THEME_UI_NUMBER(corner_radius, 0.0, 64.0),
    WM_THEME_UI_NUMBER(font_size, 4.0, 72.0),
    WM_THEME_UI_PATH(font_regular, WM_THEME_PATH),
    WM_THEME_UI_PATH(font_medium, WM_THEME_PATH_OPTIONAL),
    WM_THEME_UI_PATH(font_bold, WM_THEME_PATH_OPTIONAL),
    WM_THEME_UI_PATH(font_mono, WM_THEME_PATH_OPTIONAL),
    WM_THEME_UI_PATH(font_mono_bold, WM_THEME_PATH_OPTIONAL),
    WM_THEME_UI_NUMBER(font_mono_size, 4.0, 72.0),

    WM_THEME_WM_COLOR(desktop_top),
    WM_THEME_WM_COLOR(desktop_bottom),
    WM_THEME_WM_COLOR(title_active),
    WM_THEME_WM_COLOR(title_idle),
    WM_THEME_WM_COLOR(outline),
    WM_THEME_WM_COLOR(highlight),
    WM_THEME_WM_COLOR(highlight_idle),
    WM_THEME_WM_COLOR(button),
    WM_THEME_WM_COLOR(button_over),
    WM_THEME_WM_COLOR(button_down),
    WM_THEME_WM_COLOR(close_over),
    WM_THEME_WM_COLOR(close_down),
    WM_THEME_WM_COLOR(accent),
    WM_THEME_WM_COLOR(taskbar),
    WM_THEME_WM_COLOR(taskbar_hover),
    WM_THEME_WM_COLOR(taskbar_active),
    WM_THEME_WM_COLOR(taskbar_down),
    WM_THEME_WM_COLOR(taskbar_mark),
    WM_THEME_WM_COLOR(taskbar_power),
    WM_THEME_WM_COLOR(taskbar_edge),
    WM_THEME_WM_NUMBER(shadow_key, 0.0, 1.0),
    WM_THEME_WM_NUMBER(shadow_ambient, 0.0, 1.0),
    WM_THEME_WM_NUMBER(taskbar_shadow, 0.0, 1.0),
    WM_THEME_WM_PATH(font, WM_THEME_PATH),
    WM_THEME_WM_NUMBER(font_size, 4.0, 72.0),
    WM_THEME_WM_PATH(wallpaper, WM_THEME_PATH_OPTIONAL),
};


/**
 * @brief Converts a colour into the form it travels in.
 *
 * @param color The colour.
 * @return The same colour, for a ui_msg_theme_t.
 */
static ui_msg_color_t wm_theme_wire(ui_color_t color) {

    ui_msg_color_t c = {color.r, color.g, color.b, color.a};

    return c;
}


/**
 * @brief Fills in the built-in theme: the WM_COLOR_* palette for the server, libui's dark theme for clients.
 *
 * @param theme The theme to fill in.
 */
static void wm_theme_defaults(wm_theme_t* theme) {

    memset(theme, 0, sizeof(*theme));

    theme->desktop_top    = (ui_color_t){WM_COLOR_DESKTOP_TOP, 1.0};
    theme->desktop_bottom = (ui_color_t){WM_COLOR_DESKTOP_BOTTOM, 1.0};
    theme->title_active   = (ui_color_t){WM_COLOR_TITLE_ACTIVE, 1.0};
    theme->title_idle     = (ui_color_t){WM_COLOR_TITLE_IDLE, 1.0};
    theme->outline        = (ui_color_t){WM_COLOR_OUTLINE};
    theme->highlight      = (ui_color_t){WM_COLOR_HIGHLIGHT};
    theme->highlight_idle = (ui_color_t){WM_COLOR_HIGHLIGHT_IDLE};
    theme->button         = (ui_color_t){WM_COLOR_BUTTON};
    theme->button_over    = (ui_color_t){WM_COLOR_BUTTON_OVER};
    theme->button_down    = (ui_color_t){WM_COLOR_BUTTON_DOWN};
    theme->close_over     = (ui_color_t){WM_COLOR_CLOSE_OVER, 1.0};
    theme->close_down     = (ui_color_t){WM_COLOR_CLOSE_DOWN, 1.0};
    theme->accent         = (ui_color_t){WM_COLOR_ACCENT, 1.0};
    theme->taskbar        = (ui_color_t){WM_COLOR_TASKBAR};
    theme->taskbar_hover  = (ui_color_t){WM_COLOR_TASKBAR_HOVER};
    theme->taskbar_active = (ui_color_t){WM_COLOR_TASKBAR_ACTIVE};
    theme->taskbar_down   = (ui_color_t){WM_COLOR_TASKBAR_DOWN};
    theme->taskbar_mark   = (ui_color_t){WM_COLOR_TASKBAR_MARK};
    theme->taskbar_power  = (ui_color_t){WM_COLOR_TASKBAR_POWER, 1.0};
    theme->taskbar_edge   = (ui_color_t){WM_COLOR_TASKBAR_EDGE};

    theme->shadow_key     = WM_SHADOW_KEY_ALPHA;
    theme->shadow_ambient = WM_SHADOW_AMBIENT_ALPHA;
    theme->taskbar_shadow = WM_SHADOW_TASKBAR;

    snprintf(theme->font, sizeof(theme->font), "%s", WM_FONT_PATH);
    snprintf(theme->wallpaper, sizeof(theme->wallpaper), "%s", WM_WALLPAPER_PATH);

    theme->font_size = WM_FONT_SIZE;


    const ui_theme_t* ui = ui_theme_dark();

    ui_msg_theme_t* client = &theme->client;

    snprintf(client->name, sizeof(client->name), "%s", WM_THEME_BUILTIN);

    client->background     = wm_theme_wire(ui->background);
    client->surface        = wm_theme_wire(ui->surface);
    client->surface_sunken = wm_theme_wire(ui->surface_sunken);
    client->primary        = wm_theme_wire(ui->primary);
    client->on_primary     = wm_theme_wire(ui->on_primary);
    client->secondary      = wm_theme_wire(ui->secondary);
    client->on_secondary   = wm_theme_wire(ui->on_secondary);
    client->danger         = wm_theme_wire(ui->danger);
    client->on_danger      = wm_theme_wire(ui->on_danger);
    client->hover          = wm_theme_wire(ui->hover);
    client->active         = wm_theme_wire(ui->active);
    client->border         = wm_theme_wire(ui->border);
    client->focus_ring     = wm_theme_wire(ui->focus_ring);
    client->text           = wm_theme_wire(ui->text);
    client->text_muted     = wm_theme_wire(ui->text_muted);

    client->disabled_fade = ui->disabled_fade;
    client->corner_radius = ui->corner_radius;
    client->font_size     = ui->font_size;

    snprintf(client->font_regular, sizeof(client->font_regular), "%s", ui->font_regular ? ui->font_regular : "");
    snprintf(client->font_medium, sizeof(client->font_medium), "%s", ui->font_medium ? ui->font_medium : "");
    snprintf(client->font_bold, sizeof(client->font_bold), "%s", ui->font_bold ? ui->font_bold : "");
    snprintf(client->font_mono, sizeof(client->font_mono), "%s", ui->font_mono ? ui->font_mono : "");
    snprintf(client->font_mono_bold, sizeof(client->font_mono_bold), "%s", ui->font_mono_bold ? ui->font_mono_bold : "");

    client->font_mono_size = ui->font_mono_size;
}


/**
 * @brief Cuts the blanks off both ends of a string, in place.
 *
 * @param s The string.
 * @return Where it now starts.
 */
static char* wm_theme_trim(char* s) {

    s += strspn(s, " \t");

    size_t length = strlen(s);

    while (length && strchr(" \t\r\n", s[length - 1])) {
        s[--length] = '\0';
    }

    return s;
}


/**
 * @brief Parses a colour: `#RRGGBB` or `#RRGGBBAA`, the first optionally followed by an alpha from 0 to 1.
 *
 * @param value The text.
 * @param out Receives the colour.
 * @return 0, or -1 when the text is not a colour.
 */
static int wm_theme_parse_color(const char* value, ui_color_t* out) {

    if (value[0] != '#') {
        return -1;
    }


    const size_t digits = strspn(value + 1, "0123456789abcdefABCDEF");

    if (digits != 6 && digits != 8) {
        return -1;
    }


    char hex[9] = {0};

    memcpy(hex, value + 1, digits);

    unsigned long rgba = strtoul(hex, NULL, 16);

    if (digits == 6) {
        rgba = (rgba << 8) | 0xFF;
    }

    out->r = (double)((rgba >> 24) & 0xFF) / 255.0;
    out->g = (double)((rgba >> 16) & 0xFF) / 255.0;
    out->b = (double)((rgba >> 8) & 0xFF) / 255.0;
    out->a = (double)(rgba & 0xFF) / 255.0;


    const char* rest = value + 1 + digits;

    rest += strspn(rest, " \t");

    if (!*rest) {
        return 0;
    }

    if (digits == 8) {
        return -1;
    }


    char* end = NULL;

    const double alpha = strtod(rest, &end);

    if (end == rest || *end || !(alpha >= 0.0 && alpha <= 1.0)) {
        return -1;
    }

    out->a = alpha;

    return 0;
}


/**
 * @brief Checks a value against its key and stores it into the theme.
 *
 * @param theme The theme being read.
 * @param key The key.
 * @param value The text after the `=`.
 * @return 0, or -1 when the value does not fit the key.
 */
static int wm_theme_store(wm_theme_t* theme, const wm_theme_key_t* key, const char* value) {

    uint8_t* field = (uint8_t*)theme + key->offset;

    switch (key->kind) {

        case WM_THEME_COLOR: {

            ui_color_t color;

            if (wm_theme_parse_color(value, &color) < 0) {
                return -1;
            }

            memcpy(field, &color, sizeof(color));

            return 0;
        }

        case WM_THEME_NUMBER: {

            char* end = NULL;

            const double number = strtod(value, &end);

            if (end == value || *end || !isfinite(number) || number < key->min || number > key->max) {
                return -1;
            }

            memcpy(field, &number, sizeof(number));

            return 0;
        }

        case WM_THEME_PATH:
        case WM_THEME_PATH_OPTIONAL:

            if (!*value && key->kind == WM_THEME_PATH_OPTIONAL) {
                field[0] = '\0';
                return 0;
            }

            if (value[0] != '/' || strlen(value) >= key->size || access(value, R_OK) < 0) {
                return -1;
            }

            memcpy(field, value, strlen(value) + 1);

            return 0;
    }

    return -1;
}


/**
 * @brief Looks a key up by the section it appears under and its name.
 *
 * @param section The section.
 * @param name The key.
 * @return The key, or NULL when there is no such one.
 */
static const wm_theme_key_t* wm_theme_find(const char* section, const char* name) {

    for (size_t i = 0; i < WM_ARRAY_COUNT(wm_theme_keys); i++) {

        if (strcmp(wm_theme_keys[i].section, section) == 0 && strcmp(wm_theme_keys[i].name, name) == 0) {
            return &wm_theme_keys[i];
        }
    }

    return NULL;
}


/**
 * @brief Reads a theme file over the built-in theme, line by line.
 *
 * @param f The open file.
 * @param path Its path, for the warnings.
 * @param theme The theme to fill in, already holding the defaults.
 * @param text Receives what was wrong, on failure.
 * @param size The size of text.
 * @return 0, or -1 when a line does not parse.
 */
static int wm_theme_parse(FILE* f, const char* path, wm_theme_t* theme, char* text, size_t size) {

    char line[WM_THEME_LINE_MAX];
    char section[WM_THEME_SECTION_MAX] = "";

    int number = 0;

    while (fgets(line, sizeof(line), f)) {

        number++;

        if (!strchr(line, '\n') && !feof(f)) {
            snprintf(text, size, "line %d: too long", number);
            return -1;
        }


        char* s = wm_theme_trim(line);

        if (!*s || *s == '#') {
            continue;
        }


        if (*s == '[') {

            char* end = strchr(s, ']');

            if (!end || end[1] || (size_t)(end - s - 1) >= sizeof(section)) {
                snprintf(text, size, "line %d: bad section", number);
                return -1;
            }

            *end = '\0';

            snprintf(section, sizeof(section), "%s", s + 1);

            continue;
        }


        char* equals = strchr(s, '=');

        if (!equals) {
            snprintf(text, size, "line %d: expected key = value", number);
            return -1;
        }

        *equals = '\0';

        const char* name  = wm_theme_trim(s);
        const char* value = wm_theme_trim(equals + 1);


        const wm_theme_key_t* key = wm_theme_find(section, name);

        if (!key) {
            ulog_warnf("%s: line %d: no such key %s.%s, ignored", path, number, section, name);
            continue;
        }

        if (wm_theme_store(theme, key, value) < 0) {
            snprintf(text, size, "line %d: bad value for %s.%s", number, section, name);
            return -1;
        }
    }

    if (ferror(f)) {
        snprintf(text, size, "cannot read %s: %s", path, strerror(errno));
        return -1;
    }

    return 0;
}


/**
 * @brief Reads a theme file into a theme of its own, leaving the one in use alone.
 *
 * @param path The file.
 * @param name What to call the theme.
 * @param theme Receives the theme.
 * @param text Receives what was wrong, on failure.
 * @param size The size of text.
 * @return 0, or -1 with errno set: ENOENT when there is no such file, EINVAL when it does not parse.
 */
static int wm_theme_load(const char* path, const char* name, wm_theme_t* theme, char* text, size_t size) {

    FILE* f = fopen(path, "r");

    if (!f) {

        const int e = errno;

        snprintf(text, size, "cannot open %s: %s", path, strerror(e));

        errno = e;
        return -1;
    }


    wm_theme_defaults(theme);

    snprintf(theme->client.name, sizeof(theme->client.name), "%s", name);

    const int e = wm_theme_parse(f, path, theme, text, size);

    fclose(f);

    if (e < 0) {
        errno = EINVAL;
        return -1;
    }

    return 0;
}


/**
 * @brief Names the theme UI_THEME_LINK points at: the file it links to, without the directory and the .conf.
 *
 * @param name Receives the name, "custom" when the link is a plain file.
 * @param size The size of name.
 */
static void wm_theme_link_name(char* name, size_t size) {

    char target[WM_THEME_FILE_MAX];

    const ssize_t length = readlink(UI_THEME_LINK, target, sizeof(target) - 1);

    if (length <= 0) {
        snprintf(name, size, "custom");
        return;
    }

    target[length] = '\0';


    const char* base = strrchr(target, '/');

    base = base ? base + 1 : target;

    size_t stem = strlen(base);

    if (stem > 5 && strcmp(base + stem - 5, ".conf") == 0) {
        stem -= 5;
    }

    snprintf(name, size, "%.*s", (int)stem, base);
}


/**
 * @brief Reads the theme UI_THEME_LINK names.
 *
 * @param theme Receives the theme.
 * @param text Receives what was wrong, on failure.
 * @param size The size of text.
 * @return 0, or -1 with errno set: ENOENT when there is no theme in use.
 */
static int wm_theme_load_link(wm_theme_t* theme, char* text, size_t size) {

    char name[UI_THEME_NAME_MAX];

    wm_theme_link_name(name, sizeof(name));

    return wm_theme_load(UI_THEME_LINK, name, theme, text, size);
}


/**
 * @brief Points UI_THEME_LINK at another theme file, replacing the link in one rename.
 *
 * @param target The theme file.
 * @return 0, or -1 with errno set: EEXIST when UI_THEME_LINK is a plain file, which is left alone.
 */
static int wm_theme_link(const char* target) {

    char dir[WM_THEME_FILE_MAX];

    snprintf(dir, sizeof(dir), "%s", UI_THEME_LINK);

    *strrchr(dir, '/') = '\0';

    if (mkdir(dir, 0755) < 0 && errno != EEXIST) {
        return -1;
    }


    struct stat st;

    if (lstat(UI_THEME_LINK, &st) == 0 && !S_ISLNK(st.st_mode)) {
        errno = EEXIST;
        return -1;
    }


    char staging[WM_THEME_FILE_MAX];

    snprintf(staging, sizeof(staging), "%s.new", UI_THEME_LINK);

    unlink(staging);

    if (symlink(target, staging) < 0) {
        return -1;
    }

    if (rename(staging, UI_THEME_LINK) < 0) {

        const int e = errno;

        unlink(staging);

        errno = e;
        return -1;
    }

    return 0;
}


/**
 * @brief Reports whether a name can only mean a file directly inside UI_THEME_DIR.
 *
 * @param name The name.
 * @return true when it is one.
 */
static bool wm_theme_valid_name(const char* name) {

    const size_t length = strlen(name);

    return length && length < UI_THEME_NAME_MAX && name[0] != '.' && strspn(name, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._-") == length;
}


/**
 * @brief Puts a theme in use: rebuilds the desktop if it differs, repaints everything and tells every client.
 *
 * @param next The theme.
 */
static void wm_theme_apply(const wm_theme_t* next) {

    const bool desktop = strcmp(wm.theme.wallpaper, next->wallpaper) != 0 || memcmp(&wm.theme.desktop_top, &next->desktop_top, sizeof(ui_color_t)) != 0 || memcmp(&wm.theme.desktop_bottom, &next->desktop_bottom, sizeof(ui_color_t)) != 0;

    wm.theme = *next;

    if (desktop && wm_display_background(&wm.display) < 0) {
        ulog_warnf("keeping the desktop of the theme before");
    }

    ui_font_face(wm.theme.font);


    wm_rect_t everything = {0, 0, wm.display.width, wm.display.height};

    wm_damage(&everything);

    wm_client_broadcast(UI_EV_THEME, UI_EV_THEME_SINCE, &wm.theme.client, sizeof(wm.theme.client));

    ulog_infof("theme %s", wm.theme.client.name);
}


/**
 * @brief Reads the theme in use at startup, falling back on the built-in one.
 */
void wm_theme_init(void) {

    char text[UI_CTL_TEXT_MAX];

    if (wm_theme_load_link(&wm.theme, text, sizeof(text)) < 0) {

        if (errno != ENOENT) {
            ulog_warnf("%s, using the built-in theme", text);
        }

        wm_theme_defaults(&wm.theme);
    }

    ulog_infof("theme %s", wm.theme.client.name);
}


/**
 * @brief Switches to an installed theme and makes it the one the next boot starts with.
 *
 * @param name A theme in UI_THEME_DIR, without the .conf.
 * @param text Receives what happened, or what was wrong.
 * @param size The size of text.
 * @return 0, or -1 with errno set: EINVAL for a bad name or a theme that does not parse, ENOENT for one not installed.
 */
int wm_theme_switch(const char* name, char* text, size_t size) {

    if (!wm_theme_valid_name(name)) {
        snprintf(text, size, "bad theme name: %s", name);
        errno = EINVAL;
        return -1;
    }


    char path[WM_THEME_FILE_MAX];

    snprintf(path, sizeof(path), "%s/%s.conf", UI_THEME_DIR, name);


    wm_theme_t next;

    if (wm_theme_load(path, name, &next, text, size) < 0) {

        const int e = errno;

        if (e == ENOENT) {
            snprintf(text, size, "no such theme: %s", name);
        }

        errno = e;
        return -1;
    }

    wm_theme_apply(&next);


    if (wm_theme_link(path) < 0) {

        const int e = errno;

        if (e == EEXIST) {
            snprintf(text, size, "switched to %s, but %s is a file, so the next boot keeps it", name, UI_THEME_LINK);
        } else {
            snprintf(text, size, "switched to %s, but cannot keep it: %s", name, strerror(e));
        }

        ulog_warnf("%s", text);

        return 0;
    }

    snprintf(text, size, "switched to %s", name);

    return 0;
}


/**
 * @brief Reads the theme in use again, for a theme file edited in place.
 *
 * @param text Receives what happened, or what was wrong.
 * @param size The size of text.
 * @return 0, or -1 with errno set when the file does not parse, which leaves the theme as it was.
 */
int wm_theme_reload(char* text, size_t size) {

    wm_theme_t next;

    if (wm_theme_load_link(&next, text, size) < 0) {

        if (errno != ENOENT) {
            return -1;
        }

        wm_theme_defaults(&next);
    }

    wm_theme_apply(&next);

    snprintf(text, size, "reloaded %s", wm.theme.client.name);

    return 0;
}


/**
 * @brief Reports what a window's frame is painted with: the background its client draws, opaque, so the two meet without a seam.
 *
 * @return The colour.
 */
ui_color_t wm_theme_frame(void) {

    const ui_msg_color_t background = wm.theme.client.background;

    return (ui_color_t){background.r, background.g, background.b, 1.0};
}


/**
 * @brief Reports the wallpaper to paint: the one on the command line, or else the theme's.
 *
 * @return The path, or NULL for the gradient.
 */
const char* wm_theme_wallpaper(void) {

    if (wm.wallpaper) {
        return wm.wallpaper;
    }

    return wm.theme.wallpaper[0] ? wm.theme.wallpaper : NULL;
}
