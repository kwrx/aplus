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
 * @brief Icons by name, out of the theme in /usr/share/icons.
 *
 * A name is what a .desktop file's Icon key carries, which is why nothing here knows about
 * applications: the same lookup answers for a file type or a toolbar just as well.
 */

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ui_widget_internal.h"


/**
 * @brief The sizes a theme may hold, searched outwards from the one asked for.
 */
static const int ui_icon_sizes[] = {16, 24, 32, 48, 64, 128};


/**
 * @brief How many icons stay loaded, names that resolved to nothing included.
 */
#define UI_ICON_CACHE_MAX 32


static struct {

    char name[UI_ICON_NAME_MAX];
    int size;

    //? NULL when the name resolved to nothing, which is cached as well: a missing icon is
    //? looked for once and not on every frame it is not drawn in.
    cairo_surface_t* surface;

} ui_icon_cache[UI_ICON_CACHE_MAX];

static size_t ui_icon_cached = 0;


const char* ui_icon_theme(void) {

    const char* theme = getenv("UI_ICON_THEME");

    return (theme && *theme) ? theme : UI_ICON_THEME_DEFAULT;
}


/**
 * @brief Builds the path an icon of a size would be at and reports whether it is there.
 *
 * @param name The icon name.
 * @param size The pixel size of the directory to look in.
 * @param out Receives the path.
 * @param max The size of that buffer.
 * @return true when the file exists and can be read.
 */

static bool ui_icon_try(const char* name, int size, char* out, size_t max) {

    if (snprintf(out, max, "%s/%s/%dx%d/%s.png", UI_ICON_PATH, ui_icon_theme(), size, size, name) >= (int)max) {
        return false;
    }

    return access(out, R_OK) == 0;
}


bool ui_icon_find(const char* name, int size, char* out, size_t max) {

    if (!name || !*name || !out || max == 0) {
        return false;
    }


    if (strchr(name, '/')) {

        if (snprintf(out, max, "%s", name) >= (int)max) {
            return false;
        }

        return access(out, R_OK) == 0;
    }


    for (size_t i = 0; i < sizeof(ui_icon_sizes) / sizeof(ui_icon_sizes[0]); i++) {

        if (ui_icon_sizes[i] >= size && ui_icon_try(name, ui_icon_sizes[i], out, max)) {
            return true;
        }
    }

    for (size_t i = sizeof(ui_icon_sizes) / sizeof(ui_icon_sizes[0]); i > 0; i--) {

        if (ui_icon_sizes[i - 1] < size && ui_icon_try(name, ui_icon_sizes[i - 1], out, max)) {
            return true;
        }
    }

    return false;
}


/**
 * @brief Reads an icon file, refusing whatever cairo could not make an image surface of.
 *
 * @param path The file to read.
 * @return The surface, or NULL.
 */

static cairo_surface_t* ui_icon_read(const char* path) {

    cairo_surface_t* surface = cairo_image_surface_create_from_png(path);

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {

        cairo_surface_destroy(surface);
        return NULL;
    }

    return surface;
}


/**
 * @brief Makes room for one more entry, dropping the oldest when the cache is full.
 */

static void ui_icon_evict(void) {

    if (ui_icon_cached < UI_ICON_CACHE_MAX) {
        return;
    }


    cairo_surface_destroy(ui_icon_cache[0].surface);

    for (size_t i = 1; i < UI_ICON_CACHE_MAX; i++) {
        ui_icon_cache[i - 1] = ui_icon_cache[i];
    }

    ui_icon_cached--;
}


cairo_surface_t* ui_icon_load(const char* name, int size) {

    if (!name || !*name || size <= 0) {
        return NULL;
    }

    if (strlen(name) >= UI_ICON_NAME_MAX) {
        return NULL;
    }


    for (size_t i = 0; i < ui_icon_cached; i++) {

        if (ui_icon_cache[i].size == size && strcmp(ui_icon_cache[i].name, name) == 0) {
            return ui_icon_cache[i].surface;
        }
    }


    char path[PATH_MAX];

    cairo_surface_t* surface = ui_icon_find(name, size, path, sizeof(path)) ? ui_icon_read(path) : NULL;

    ui_icon_evict();

    strcpy(ui_icon_cache[ui_icon_cached].name, name);

    ui_icon_cache[ui_icon_cached].size    = size;
    ui_icon_cache[ui_icon_cached].surface = surface;

    ui_icon_cached++;

    return surface;
}


void ui_draw_icon(cairo_t* cr, ui_rect_t rect, cairo_surface_t* icon) {

    if (!cr || !icon || rect.width <= 0 || rect.height <= 0) {
        return;
    }


    const double width  = (double)cairo_image_surface_get_width(icon);
    const double height = (double)cairo_image_surface_get_height(icon);

    if (width <= 0.0 || height <= 0.0) {
        return;
    }


    double scale = (double)rect.width / width;

    if ((double)rect.height / height < scale) {
        scale = (double)rect.height / height;
    }


    const double x = round((double)rect.x + ((double)rect.width - width * scale) / 2.0);
    const double y = round((double)rect.y + ((double)rect.height - height * scale) / 2.0);

    cairo_save(cr);

    cairo_translate(cr, x, y);
    cairo_scale(cr, scale, scale);

    cairo_set_source_surface(cr, icon, 0.0, 0.0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);

    cairo_restore(cr);
}


void ui_draw_icon_named(cairo_t* cr, ui_rect_t rect, const char* name, int size) {

    ui_draw_icon(cr, rect, ui_icon_load(name, size > 0 ? size : (rect.width < rect.height ? rect.width : rect.height)));
}
