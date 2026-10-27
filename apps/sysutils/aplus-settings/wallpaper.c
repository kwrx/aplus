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
 * @brief The Wallpaper page: a thumbnail of every picture the server can show, the one in use ringed.
 */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <webp/decode.h>

#include <aplus/ui-draw.h>

#include "settings.h"


#define WALLPAPER_DIR      "/usr/share/images"
#define WALLPAPER_MAX      16
#define WALLPAPER_THUMB    256
#define WALLPAPER_TILE_GAP 14
#define WALLPAPER_RING     3
#define WALLPAPER_FILE_MAX (16 * 1024 * 1024)


/**
 * @brief One picture: its file, its thumbnail once decoded, and where its tile is.
 */
typedef struct {

    char path[UI_THEME_PATH_MAX];

    cairo_surface_t* thumb;
    bool failed;

    ui_rect_t rect;

} wallpaper_item_t;


/**
 * @brief The page's widgets, the pictures, the one in use, and what the pointer is doing over them.
 */
static struct {

    settings_header_t header;

    ui_widget_t* status;

    wallpaper_item_t items[WALLPAPER_MAX];
    size_t count;

    ui_rect_t area;

    char current[UI_THEME_PATH_MAX];

    int hover;
    int pressed;
    bool down;

} wallpaper;


/**
 * @brief Shows a line at the foot of the page, or clears it.
 *
 * @param text The line, or "" for none.
 */
static void wallpaper_report(const char* text) {
    settings_fit(wallpaper.status, text);
}


/**
 * @brief Asks the server which picture is on the desktop.
 */
static void wallpaper_query(void) {

    if (ui_control_get_option("wm", "wallpaper", wallpaper.current, sizeof(wallpaper.current)) < 0) {
        wallpaper.current[0] = '\0';
    }

    settings_repaint();
}


/**
 * @brief Sets the wallpaper key, reporting a refusal.
 *
 * @param path The picture.
 */
static void wallpaper_set(const char* path) {

    ui_msg_ctl_option_t option;
    ui_msg_ctl_reply_t reply;

    memset(&option, 0, sizeof(option));
    memset(&reply, 0, sizeof(reply));

    strcpy(option.section, "wm");
    strcpy(option.key, "wallpaper");

    snprintf(option.value, sizeof(option.value), "%s", path);

    if (ui_control_set_options(&option, 1, &reply) < 0) {

        char text[SETTINGS_TEXT_MAX];

        settings_control_error(text, sizeof(text), reply.status == 0, &reply);
        wallpaper_report(text);

        return;
    }

    wallpaper_report("");
    wallpaper_query();
}


/**
 * @brief Orders two pictures by path, for qsort().
 *
 * @param a The first.
 * @param b The second.
 * @return Less than, equal to or greater than 0, as strcmp().
 */
static int wallpaper_compare(const void* a, const void* b) {
    return strcmp(((const wallpaper_item_t*)a)->path, ((const wallpaper_item_t*)b)->path);
}


/**
 * @brief Decodes a webp file straight to thumbnail size, the decoder doing the scaling.
 *
 * @param path The file.
 * @return The thumbnail, or NULL when the file cannot be read.
 */
static cairo_surface_t* wallpaper_decode(const char* path) {

    FILE* fp = fopen(path, "rb");

    if (!fp) {
        return NULL;
    }

    uint8_t* data = NULL;
    long size     = 0;

    if (fseek(fp, 0, SEEK_END) == 0 && (size = ftell(fp)) > 0 && size <= WALLPAPER_FILE_MAX && fseek(fp, 0, SEEK_SET) == 0 && (data = malloc((size_t)size))) {

        if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
            free(data);
            data = NULL;
        }
    }

    fclose(fp);

    if (!data) {
        return NULL;
    }


    cairo_surface_t* thumb = NULL;

    int width;
    int height;

    if (WebPGetInfo(data, (size_t)size, &width, &height) && width > 0 && height > 0) {

        const int tw = WALLPAPER_THUMB;

        int th = (int)((double)height * tw / width + 0.5);

        if (th < 1) {
            th = 1;
        }

        thumb = cairo_image_surface_create(CAIRO_FORMAT_RGB24, tw, th);

        WebPDecoderConfig config;

        if (cairo_surface_status(thumb) == CAIRO_STATUS_SUCCESS && WebPInitDecoderConfig(&config)) {

            cairo_surface_flush(thumb);

            config.options.use_scaling   = 1;
            config.options.scaled_width  = tw;
            config.options.scaled_height = th;

            config.output.colorspace         = MODE_BGRA;
            config.output.is_external_memory = 1;
            config.output.u.RGBA.rgba        = cairo_image_surface_get_data(thumb);
            config.output.u.RGBA.stride      = cairo_image_surface_get_stride(thumb);
            config.output.u.RGBA.size        = (size_t)cairo_image_surface_get_stride(thumb) * (size_t)th;

            if (WebPDecode(data, (size_t)size, &config) == VP8_STATUS_OK) {
                cairo_surface_mark_dirty(thumb);
            } else {
                cairo_surface_destroy(thumb);
                thumb = NULL;
            }

            WebPFreeDecBuffer(&config.output);

        } else {

            cairo_surface_destroy(thumb);
            thumb = NULL;
        }
    }

    free(data);

    return thumb;
}


/**
 * @brief Collects the webp pictures in WALLPAPER_DIR, sorted, without decoding them yet.
 */
static void wallpaper_scan(void) {

    DIR* dir = opendir(WALLPAPER_DIR);

    if (!dir) {
        return;
    }

    struct dirent* ent;

    while ((ent = readdir(dir)) && wallpaper.count < WALLPAPER_MAX) {

        const char* dot = strrchr(ent->d_name, '.');

        if (ent->d_name[0] == '.' || !dot || strcasecmp(dot, ".webp") != 0) {
            continue;
        }

        wallpaper_item_t* item = &wallpaper.items[wallpaper.count];

        if ((size_t)snprintf(item->path, sizeof(item->path), "%s/%s", WALLPAPER_DIR, ent->d_name) < sizeof(item->path)) {
            wallpaper.count++;
        }
    }

    closedir(dir);

    qsort(wallpaper.items, wallpaper.count, sizeof(wallpaper.items[0]), wallpaper_compare);
}


/**
 * @brief Creates the page's widgets and lists the pictures.
 *
 * @return 0 on success, -1 otherwise.
 */
static int wallpaper_build(void) {

    if (settings_header_build(&wallpaper.header, "Wallpaper", "The picture behind every window.") < 0) {
        return -1;
    }

    if ((wallpaper.status = settings_own(ui_label_create(settings.view, ""))) == NULL) {
        return -1;
    }

    wallpaper.hover   = -1;
    wallpaper.pressed = -1;

    wallpaper_scan();

    return 0;
}


/**
 * @brief Lays the tiles out in three or four columns at the desktop's proportions, and the foot under them.
 *
 * @param page The page's rectangle.
 */
static void wallpaper_layout(ui_rect_t page) {

    const int control = settings_control_height();
    const int top     = settings_header_place(&wallpaper.header, page, 0);
    const int foot    = page.y + page.height - control;

    ui_widget_set_rect(wallpaper.status, page.x, foot, page.width, control);


    const int inset   = WALLPAPER_RING + 2;
    const int columns = wallpaper.count > 9 ? 4 : 3;
    const int rows    = ((int)wallpaper.count + columns - 1) / columns;

    const ui_rect_t area = {page.x + inset, top + inset, page.width - 2 * inset, foot - SETTINGS_SECTION_GAP - top - 2 * inset};

    int width  = (area.width - (columns - 1) * WALLPAPER_TILE_GAP) / columns;
    int height = width * 10 / 16;

    if (rows > 0 && rows * height + (rows - 1) * WALLPAPER_TILE_GAP > area.height) {
        height = (area.height - (rows - 1) * WALLPAPER_TILE_GAP) / rows;
    }

    if (width < 0 || height < 0) {
        width  = 0;
        height = 0;
    }

    wallpaper.area = (ui_rect_t){page.x, top, page.width, rows * (height + WALLPAPER_TILE_GAP) + 2 * inset};

    for (size_t i = 0; i < wallpaper.count; i++) {
        wallpaper.items[i].rect = (ui_rect_t){area.x + (int)(i % columns) * (width + WALLPAPER_TILE_GAP), area.y + (int)(i / columns) * (height + WALLPAPER_TILE_GAP), width, height};
    }
}


/**
 * @brief Gives the page the colours of the theme in use, and asks again which picture is up, since a theme brings its own.
 */
static void wallpaper_restyle(void) {

    settings_header_restyle(&wallpaper.header);

    ui_label_set_color(wallpaper.status, ui_theme()->text_muted);

    wallpaper_query();
}


/**
 * @brief Asks which picture is up whenever the page is shown.
 */
static void wallpaper_enter(void) {

    wallpaper_report(wallpaper.count ? "" : "No pictures are installed in " WALLPAPER_DIR ".");
    wallpaper_query();
}


/**
 * @brief Decodes one thumbnail a pass while the page is on show, so the page stays live as they come in.
 *
 * @param now Unused.
 * @param visible Whether the page is on show.
 * @return 0 while thumbnails are left to decode, -1 once they are all in.
 */
static int wallpaper_tick(uint64_t now, bool visible) {

    (void)now;

    if (!visible) {
        return -1;
    }

    for (size_t i = 0; i < wallpaper.count; i++) {

        wallpaper_item_t* item = &wallpaper.items[i];

        if (item->thumb || item->failed) {
            continue;
        }

        if ((item->thumb = wallpaper_decode(item->path)) == NULL) {
            item->failed = true;
        }

        settings_repaint();

        return 0;
    }

    return -1;
}


/**
 * @brief Fills a rounded rectangle with a colour.
 *
 * @param cr The context.
 * @param rect The rectangle.
 * @param color The colour.
 */
static void wallpaper_fill(cairo_t* cr, ui_rect_t rect, ui_color_t color) {

    ui_draw_rounded_rect(cr, rect, ui_theme()->corner_radius);

    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
    cairo_fill(cr);
}


/**
 * @brief Draws one tile: the thumbnail cut to the tile's proportions, the pointer's wash over it, and a ring when it is in use.
 *
 * @param cr The context.
 * @param index The tile.
 */
static void wallpaper_paint_tile(cairo_t* cr, int index) {

    const wallpaper_item_t* item = &wallpaper.items[index];
    const ui_theme_t* theme      = ui_theme();
    const ui_rect_t rect         = item->rect;

    cairo_save(cr);

    cairo_rectangle(cr, rect.x - WALLPAPER_RING - 2, rect.y - WALLPAPER_RING - 2, rect.width + 2 * (WALLPAPER_RING + 2), rect.height + 2 * (WALLPAPER_RING + 2));
    cairo_set_source_rgb(cr, theme->background.r, theme->background.g, theme->background.b);
    cairo_fill(cr);

    if (item->thumb) {

        const int tw = cairo_image_surface_get_width(item->thumb);
        const int th = cairo_image_surface_get_height(item->thumb);

        const double sx    = (double)rect.width / tw;
        const double sy    = (double)rect.height / th;
        const double scale = sx > sy ? sx : sy;

        cairo_save(cr);

        ui_draw_rounded_rect(cr, rect, theme->corner_radius);
        cairo_clip(cr);

        cairo_translate(cr, rect.x + (rect.width - tw * scale) / 2.0, rect.y + (rect.height - th * scale) / 2.0);
        cairo_scale(cr, scale, scale);
        cairo_set_source_surface(cr, item->thumb, 0.0, 0.0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
        cairo_paint(cr);

        cairo_restore(cr);

    } else {

        wallpaper_fill(cr, rect, theme->surface_sunken);
    }

    if (index == wallpaper.pressed && index == wallpaper.hover) {
        wallpaper_fill(cr, rect, theme->active);
    } else if (index == wallpaper.hover) {
        wallpaper_fill(cr, rect, theme->hover);
    }

    if (strcmp(item->path, wallpaper.current) == 0) {

        ui_draw_rounded_rect_d(cr, rect.x - WALLPAPER_RING / 2.0 - 1.0, rect.y - WALLPAPER_RING / 2.0 - 1.0, rect.width + WALLPAPER_RING + 2.0, rect.height + WALLPAPER_RING + 2.0, theme->corner_radius + WALLPAPER_RING / 2.0 + 1.0);

        cairo_set_source_rgba(cr, theme->primary.r, theme->primary.g, theme->primary.b, theme->primary.a);
        cairo_set_line_width(cr, WALLPAPER_RING);
        cairo_stroke(cr);

    } else {

        ui_draw_rounded_rect_d(cr, rect.x + 0.5, rect.y + 0.5, rect.width - 1.0, rect.height - 1.0, theme->corner_radius);

        cairo_set_source_rgba(cr, theme->border.r, theme->border.g, theme->border.b, theme->border.a);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    }

    cairo_restore(cr);
}


/**
 * @brief Lays every tile down again over the background the view painted.
 *
 * @param cr A context over the window.
 */
static void wallpaper_paint(cairo_t* cr) {

    if (wallpaper.area.width <= 0 || wallpaper.area.height <= 0) {
        return;
    }

    for (size_t i = 0; i < wallpaper.count; i++) {

        if (wallpaper.items[i].rect.width > 0 && wallpaper.items[i].rect.height > 0) {
            wallpaper_paint_tile(cr, (int)i);
        }
    }

    ui_window_damage(settings.window, wallpaper.area.x, wallpaper.area.y, wallpaper.area.width, wallpaper.area.height);
}


/**
 * @brief Finds the tile under a point.
 *
 * @param x The point's x.
 * @param y The point's y.
 * @return The tile, or -1 for none.
 */
static int wallpaper_hit(int x, int y) {

    for (size_t i = 0; i < wallpaper.count; i++) {

        const ui_rect_t r = wallpaper.items[i].rect;

        if (x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height) {
            return (int)i;
        }
    }

    return -1;
}


/**
 * @brief Tracks the pointer over the tiles: hover, press, and a click released over the tile it went down on.
 *
 * @param event A pointer or leave event.
 */
static void wallpaper_pointer(const ui_event_t* event) {

    const int hover   = wallpaper.hover;
    const int pressed = wallpaper.pressed;

    if (event->type == UI_EVENT_LEAVE) {

        wallpaper.hover   = -1;
        wallpaper.pressed = -1;
        wallpaper.down    = false;

    } else {

        const int over  = wallpaper_hit(event->pointer.x, event->pointer.y);
        const bool down = (event->pointer.buttons & UI_BUTTON_LEFT) != 0;

        if (down && !wallpaper.down) {
            wallpaper.pressed = over;
        }

        if (!down && wallpaper.down) {

            if (wallpaper.pressed >= 0 && wallpaper.pressed == over) {
                wallpaper_set(wallpaper.items[over].path);
            }

            wallpaper.pressed = -1;
        }

        wallpaper.hover = over;
        wallpaper.down  = down;
    }

    if (hover != wallpaper.hover || pressed != wallpaper.pressed) {
        settings_repaint();
    }
}


/**
 * @brief The Wallpaper page.
 */
const settings_page_t settings_wallpaper = {
    .name    = "wallpaper",
    .label   = "Wallpaper",
    .icon    = "preferences-desktop-wallpaper",
    .build   = wallpaper_build,
    .layout  = wallpaper_layout,
    .restyle = wallpaper_restyle,
    .enter   = wallpaper_enter,
    .tick    = wallpaper_tick,
    .paint   = wallpaper_paint,
    .pointer = wallpaper_pointer,
};
