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

#ifndef NO_CAIRO_EXTENSION

    #include <errno.h>
    #include <fcntl.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>

    #include <aplus/cairo-ext/cairo-fc.h>
    #include <cairo/cairo-ft.h>
    #include <cairo/cairo.h>

    #include <aplus/base.h>
    #include <aplus/sysconfig.h>
    #include <aplus/utils/list.h>

typedef struct {
        char family[BUFSIZ];
        char path[BUFSIZ];
        cairo_font_slant_t slant;
        cairo_font_weight_t weight;
} __fc_line_t;

static list(__fc_line_t *, __fc_lines);
static FT_Library __ft_lib;


int cairo_fc_load(const char *conf) {
    if (unlikely(!conf))
        conf = sysconfig("ui.font.config", NULL);

    if (unlikely(!conf)) {
        errno = EINVAL;
        return -1;
    }

    FILE *fp = fopen(conf, "r");
    if (!fp)
        return -1;

    char buf[BUFSIZ] = {0};
    for (; fgets(buf, sizeof(buf), fp) > 0; memset(buf, 0, sizeof(buf))) {
        char *p;
        if ((p = strrchr(buf, '\n')))
            *p = '\0';

        if (buf[0] == '\0')
            continue;

        if (buf[0] == '#')
            continue;

        __fc_line_t *ln = (__fc_line_t *)__libaplus_calloc(1, sizeof(__fc_line_t));
        if (unlikely(!ln)) {
            errno = ENOMEM;
            return -1;
        }

        int i = 0;
        for (p = strtok(buf, ":"); p; p = strtok(NULL, ":"), i++) {
            switch (i) {
                case 0:
                    strncpy(ln->path, p, sizeof(ln->path));
                    break;
                case 1:
                    strncpy(ln->family, p, sizeof(ln->family));
                    break;
                case 2:
                    if (strcmp(p, "Light") == 0)
                        ln->weight = CAIRO_FONT_WEIGHT_LIGHT;
                    else if (strcmp(p, "Medium") == 0)
                        ln->weight = CAIRO_FONT_WEIGHT_MEDIUM;
                    else if (strcmp(p, "Bold") == 0)
                        ln->weight = CAIRO_FONT_WEIGHT_BOLD;
                    else
                        ln->weight = CAIRO_FONT_WEIGHT_NORMAL;
                    break;
                case 3:
                    if (strcmp(p, "Italic") == 0)
                        ln->slant = CAIRO_FONT_SLANT_ITALIC;
                    else if (strcmp(p, "Oblique") == 0)
                        ln->slant = CAIRO_FONT_SLANT_OBLIQUE;
                    else
                        ln->slant = CAIRO_FONT_SLANT_NORMAL;
                    break;
                default:
                    fprintf(stderr, "cairo_fc_load(): syntax error!!");
                    break;
            }
        }

        list_push(__fc_lines, ln);
    }


    FT_Init_FreeType(&__ft_lib);
    return list_length(__fc_lines);
}


cairo_font_face_t *cairo_fc_font_face_create(const char *family, cairo_font_slant_t slant, cairo_font_weight_t weight) {
    if (!family || strlen(family) == 0) {
        errno = EINVAL;
        return NULL;
    }

    list_each(__fc_lines, ln) {
        if (strcmp(family, ln->family) != 0)
            continue;

        if (slant != ln->slant)
            continue;

        if (weight != ln->weight)
            continue;

        FT_Face face;
        if (FT_New_Face(__ft_lib, ln->path, 0, &face) != 0)
            return NULL;

        return cairo_ft_font_face_create_for_ft_face(face, 0);
    }

    return NULL;
}

void cairo_fc_select_font_face(cairo_t *cr, const char *family, cairo_font_slant_t slant, cairo_font_weight_t weight) {

    cairo_font_face_t *face;
    if ((face = cairo_fc_font_face_create(family, slant, weight)))
        cairo_set_font_face(cr, face);
}

#endif
