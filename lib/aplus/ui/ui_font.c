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

#include <stdio.h>
#include <string.h>

#include <cairo/cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "ui_widget_internal.h"


/**
 * @brief How many faces the cache holds; faces are keyed by path and never released.
 */

#define UI_FONT_CACHE_MAX 4


static FT_Library ui_ft = NULL;

static struct {

    char path[128];

    FT_Face ft_face;
    cairo_font_face_t* face;

} ui_font_cache[UI_FONT_CACHE_MAX];

static size_t ui_font_cached = 0;


cairo_font_face_t* ui_font_face(const char* path) {

    if (!path || !*path) {
        return NULL;
    }


    for (size_t i = 0; i < ui_font_cached; i++) {

        if (strcmp(ui_font_cache[i].path, path) == 0) {
            return ui_font_cache[i].face;
        }
    }

    if (ui_font_cached == UI_FONT_CACHE_MAX) {
        return NULL;
    }

    if (strlen(path) >= sizeof(ui_font_cache[0].path)) {
        fprintf(stderr, "libui: font path is too long: %s\n", path);
        return NULL;
    }


    if (!ui_ft) {

        if (FT_Init_FreeType(&ui_ft) != 0) {
            fprintf(stderr, "libui: cannot initialize freetype, text will not be drawn\n");
            return NULL;
        }
    }


    size_t slot = ui_font_cached++;

    strcpy(ui_font_cache[slot].path, path);

    if (FT_New_Face(ui_ft, path, 0, &ui_font_cache[slot].ft_face) != 0) {
        fprintf(stderr, "libui: cannot load %s, text will not be drawn\n", path);
        return NULL;
    }

    if ((ui_font_cache[slot].face = cairo_ft_font_face_create_for_ft_face(ui_font_cache[slot].ft_face, 0)) == NULL) {
        fprintf(stderr, "libui: cannot create a cairo font face for %s\n", path);
        return NULL;
    }

    return ui_font_cache[slot].face;
}
