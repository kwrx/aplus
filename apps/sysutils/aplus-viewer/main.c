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
 * @brief A document and picture viewer: mupdf draws the pages, laid out one under the other, a tile at a time.
 */

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cairo/cairo.h>
#include <mupdf/fitz.h>

#include <aplus/input.h>
#include <aplus/ui-widgets.h>
#include <aplus/ui.h>

#include "webp.h"


#define VIEWER_NAME "aplus-viewer"

#define VIEWER_WINDOW_WIDTH  680
#define VIEWER_WINDOW_HEIGHT 560

#define VIEWER_MARGIN 12
#define VIEWER_GAP    8

#define VIEWER_TOOLBAR_HEIGHT 36
#define VIEWER_STATUS_HEIGHT  20

/**
 * @brief How far inside the frame the pages are drawn, so that the panel's border stays visible.
 */
#define VIEWER_FRAME_INSET 2

/**
 * @brief The space around the document and between two of its pages, in window pixels.
 */
#define VIEWER_PAGE_MARGIN 12
#define VIEWER_PAGE_GAP    12

/**
 * @brief The size a page is given when mupdf cannot tell its own, in points.
 */
#define VIEWER_PAGE_WIDTH  612.0
#define VIEWER_PAGE_HEIGHT 792.0

/**
 * @brief How many points mupdf lays an inch out in, which turns a picture's resolution into a scale.
 */
#define VIEWER_POINTS_PER_INCH 72.0

#define VIEWER_SCALE_MIN  0.05
#define VIEWER_SCALE_MAX  32.0
#define VIEWER_SCALE_STEP 1.25

#define VIEWER_SCROLL_STEP 48

/**
 * @brief The side of a rendered tile, in window pixels.
 */
#define VIEWER_TILE_SIZE 256

/**
 * @brief How many rendered tiles are kept, and how many pages keep their display list.
 */
#define VIEWER_TILE_CACHE 96
#define VIEWER_LIST_CACHE 8

/**
 * @brief How much mupdf may keep of the fonts and images it has decoded.
 */
#define VIEWER_STORE_SIZE (64 << 20)

#define VIEWER_CTRL_LEFT  (1 << 0)
#define VIEWER_CTRL_RIGHT (1 << 1)


/**
 * @brief What a toolbar button does.
 */

typedef enum {

    VIEWER_ACTION_ZOOM_OUT = 0,
    VIEWER_ACTION_ZOOM_IN,
    VIEWER_ACTION_FIT,
    VIEWER_ACTION_ACTUAL,

    VIEWER_ACTION_COUNT,

} viewer_action_t;


/**
 * @brief The toolbar, laid out from the right edge in this order.
 */

static const struct {

    viewer_action_t action;

    const char* label;

    int width;

} viewer_buttons[] = {

    {VIEWER_ACTION_ZOOM_OUT, "\xE2\x88\x92", 36},
    {VIEWER_ACTION_ZOOM_IN,  "+",            36},
    {VIEWER_ACTION_FIT,      "Fit",          52},
    {VIEWER_ACTION_ACTUAL,   "1:1",          52},
};


/**
 * @brief A page: its size in points, where it sits at the current scale, and its display list.
 */

typedef struct {

    fz_rect bounds;

    fz_display_list* list;

    bool broken;

    unsigned int used;

    int x;
    int y;
    int width;
    int height;

} viewer_page_t;


/**
 * @brief A rendered piece of a page, at the scale the document is laid out at.
 */

typedef struct {

    cairo_surface_t* surface;

    int page;
    int column;
    int row;

    unsigned int used;

} viewer_tile_t;


/**
 * @brief A point of the document that a change of scale keeps in place: a page, where on it, and how far off it.
 */

typedef struct {

    int page;

    double fx;
    double fy;

    double ex;
    double ey;

} viewer_anchor_t;


/**
 * @brief Everything the viewer keeps between one event and the next.
 */

typedef struct {

    ui_connection_t* conn;
    ui_window_t* window;
    ui_view_t* view;

    ui_widget_t* toolbar;
    ui_widget_t* name;
    ui_widget_t* frame;
    ui_widget_t* status;
    ui_widget_t* buttons[VIEWER_ACTION_COUNT];

    ui_rect_t area;

    fz_context* ctx;
    fz_document* document;

    viewer_page_t* pages;
    int count;

    viewer_tile_t tiles[VIEWER_TILE_CACHE];

    unsigned int clock;

    int doc_width;
    int doc_height;

    bool picture;

    double unit;
    double scale;
    double scroll_x;
    double scroll_y;

    bool fit;

    bool panning;

    int pan_x;
    int pan_y;

    int pointer_x;
    int pointer_y;

    uint8_t ctrl;

    bool quit;

    char path[PATH_MAX];

} viewer_t;


static viewer_t viewer;


/**
 * @brief The rectangle the pages are drawn in, which is the frame minus the border the view draws.
 *
 * @return The rectangle, in window coordinates.
 */

static ui_rect_t viewer_canvas(void) {

    return ui_rect_inset(viewer.area, VIEWER_FRAME_INSET);
}


/**
 * @brief Holds a scale inside the range the viewer works in.
 *
 * @param scale The scale to clamp.
 * @return The clamped scale.
 */

static double viewer_clamp_scale(double scale) {

    if (scale < VIEWER_SCALE_MIN) {
        return VIEWER_SCALE_MIN;
    }

    if (scale > VIEWER_SCALE_MAX) {
        return VIEWER_SCALE_MAX;
    }

    return scale;
}


/**
 * @brief Converts a length in points to whole window pixels at the current scale.
 *
 * @param points The length.
 * @return The length in pixels, never less than one.
 */

static int viewer_pixels(double points) {

    const long pixels = lround(points * viewer.unit * viewer.scale);

    return pixels < 1 ? 1 : (int)pixels;
}


/**
 * @brief Places every page at the current scale, centred in a column as wide as the widest of them.
 */

static void viewer_layout_pages(void) {

    int widest = 0;
    int y      = VIEWER_PAGE_MARGIN;

    for (int i = 0; i < viewer.count; i++) {

        viewer_page_t* page = &viewer.pages[i];

        page->width  = viewer_pixels(page->bounds.x1 - page->bounds.x0);
        page->height = viewer_pixels(page->bounds.y1 - page->bounds.y0);
        page->y      = y;

        y += page->height + VIEWER_PAGE_GAP;

        if (page->width > widest) {
            widest = page->width;
        }
    }


    viewer.doc_width  = viewer.count > 0 ? widest + VIEWER_PAGE_MARGIN * 2 : 0;
    viewer.doc_height = viewer.count > 0 ? y - VIEWER_PAGE_GAP + VIEWER_PAGE_MARGIN : 0;

    for (int i = 0; i < viewer.count; i++) {
        viewer.pages[i].x = (viewer.doc_width - viewer.pages[i].width) / 2;
    }
}


/**
 * @brief Throws away every rendered tile.
 */

static void viewer_flush_tiles(void) {

    for (size_t i = 0; i < VIEWER_TILE_CACHE; i++) {

        if (viewer.tiles[i].surface) {
            cairo_surface_destroy(viewer.tiles[i].surface);
        }

        viewer.tiles[i].surface = NULL;
    }
}


/**
 * @brief Changes the scale and lays the pages out again, dropping the tiles drawn at the old one.
 *
 * @param scale The scale to go to, clamped to the viewer's range.
 */

static void viewer_set_scale(double scale) {

    scale = viewer_clamp_scale(scale);

    if (scale != viewer.scale) {

        viewer.scale = scale;
        viewer_flush_tiles();
    }

    viewer_layout_pages();
}


/**
 * @brief Keeps one axis of the scroll position inside the document, centring a document that fits.
 *
 * @param scroll The scroll position to clamp.
 * @param content The length of the document along the axis.
 * @param view The length of the canvas along the axis.
 */

static void viewer_clamp_axis(double* scroll, int content, int view) {

    if (content <= view) {

        *scroll = -((view - content) / 2);
        return;
    }

    if (*scroll < 0.0) {
        *scroll = 0.0;
    }

    if (*scroll > content - view) {
        *scroll = content - view;
    }
}


/**
 * @brief Keeps the scroll position inside the document.
 */

static void viewer_clamp_scroll(void) {

    const ui_rect_t canvas = viewer_canvas();

    viewer_clamp_axis(&viewer.scroll_x, viewer.doc_width, canvas.width);
    viewer_clamp_axis(&viewer.scroll_y, viewer.doc_height, canvas.height);
}


/**
 * @brief Reports how far along a length a point is, held between its two ends.
 *
 * @param offset The distance from the start.
 * @param length The length.
 * @return The fraction, between 0 and 1.
 */

static double viewer_fraction(double offset, int length) {

    const double fraction = offset / length;

    return fraction < 0.0 ? 0.0 : fraction > 1.0 ? 1.0 : fraction;
}


/**
 * @brief Names the document point under a position, in a way that survives a change of scale.
 *
 * @param x The position across the document, in pixels at the current scale.
 * @param y The position down the document, in pixels at the current scale.
 * @return The anchor.
 */

static viewer_anchor_t viewer_anchor_at(double x, double y) {

    viewer_anchor_t anchor = {0};

    if (viewer.count <= 0) {
        return anchor;
    }


    int index = viewer.count - 1;

    for (int i = 0; i < viewer.count; i++) {

        if (y < viewer.pages[i].y + viewer.pages[i].height + VIEWER_PAGE_GAP) {
            index = i;
            break;
        }
    }


    const viewer_page_t* page = &viewer.pages[index];

    anchor.page = index;
    anchor.fx   = viewer_fraction(x - page->x, page->width);
    anchor.fy   = viewer_fraction(y - page->y, page->height);
    anchor.ex   = x - page->x - anchor.fx * page->width;
    anchor.ey   = y - page->y - anchor.fy * page->height;

    return anchor;
}


/**
 * @brief Finds where an anchor is now, after the pages were laid out again.
 *
 * @param anchor The anchor.
 * @param x Receives the position across the document.
 * @param y Receives the position down the document.
 */

static void viewer_anchor_point(const viewer_anchor_t* anchor, double* x, double* y) {

    const viewer_page_t* page = &viewer.pages[anchor->page];

    *x = page->x + anchor->fx * page->width + anchor->ex;
    *y = page->y + anchor->fy * page->height + anchor->ey;
}


/**
 * @brief Rescales about a window point, which stays over the same point of the document.
 *
 * @param scale The scale to go to, clamped to the viewer's range.
 * @param x The window x the gesture is anchored at.
 * @param y The window y the gesture is anchored at.
 */

static void viewer_zoom_at(double scale, double x, double y) {

    if (viewer.count <= 0) {
        return;
    }


    const ui_rect_t canvas = viewer_canvas();

    const double cx = x - canvas.x;
    const double cy = y - canvas.y;

    const viewer_anchor_t anchor = viewer_anchor_at(viewer.scroll_x + cx, viewer.scroll_y + cy);

    viewer_set_scale(scale);


    double ax;
    double ay;

    viewer_anchor_point(&anchor, &ax, &ay);

    viewer.scroll_x = ax - cx;
    viewer.scroll_y = ay - cy;
    viewer.fit      = false;

    viewer_clamp_scroll();
}


/**
 * @brief Rescales about the middle of the canvas, which is what the toolbar and the keys do.
 *
 * @param scale The scale to go to.
 */

static void viewer_zoom_centered(double scale) {

    const ui_rect_t canvas = viewer_canvas();

    viewer_zoom_at(scale, canvas.x + canvas.width / 2.0, canvas.y + canvas.height / 2.0);
}


/**
 * @brief The scale at which the widest page fills the width of the canvas, or a picture fits whole without being enlarged.
 *
 * @return The scale.
 */

static double viewer_fit_scale(void) {

    const ui_rect_t canvas = viewer_canvas();

    double widest  = 0.0;
    double tallest = 0.0;

    for (int i = 0; i < viewer.count; i++) {
        widest  = fmax(widest, viewer.pages[i].bounds.x1 - viewer.pages[i].bounds.x0);
        tallest = fmax(tallest, viewer.pages[i].bounds.y1 - viewer.pages[i].bounds.y0);
    }

    if (widest <= 0.0 || tallest <= 0.0 || canvas.width <= VIEWER_PAGE_MARGIN * 2 || canvas.height <= VIEWER_PAGE_MARGIN * 2) {
        return 1.0;
    }

    const double across = (canvas.width - VIEWER_PAGE_MARGIN * 2) / (widest * viewer.unit);
    const double down   = (canvas.height - VIEWER_PAGE_MARGIN * 2) / (tallest * viewer.unit);

    return viewer_clamp_scale(viewer.picture ? fmin(fmin(across, down), 1.0) : across);
}


/**
 * @brief Fits the pages to the width of the canvas, keeping the top of the view where it is.
 */

static void viewer_refit(void) {

    const ui_rect_t canvas = viewer_canvas();

    viewer_zoom_at(viewer_fit_scale(), canvas.x + canvas.width / 2.0, canvas.y);

    viewer.fit = true;
}


/**
 * @brief Moves the view over the document by a number of window pixels.
 *
 * @param dx How far to move it across.
 * @param dy How far to move it down.
 */

static void viewer_scroll_by(double dx, double dy) {

    if (viewer.count <= 0) {
        return;
    }

    viewer.scroll_x += dx;
    viewer.scroll_y += dy;

    viewer_clamp_scroll();
}


/**
 * @brief The page that takes up most of the canvas.
 *
 * @return The page index.
 */

static int viewer_current_page(void) {

    const ui_rect_t canvas = viewer_canvas();

    const double top    = viewer.scroll_y;
    const double bottom = viewer.scroll_y + canvas.height;

    int best = 0;

    double most = 0.0;

    for (int i = 0; i < viewer.count; i++) {

        const viewer_page_t* page = &viewer.pages[i];

        if (page->y >= bottom) {
            break;
        }

        const double shown = fmin(bottom, page->y + page->height) - fmax(top, page->y);

        if (shown > most) {
            most = shown;
            best = i;
        }
    }

    return best;
}


/**
 * @brief Writes the page, or the size of a picture, and the magnification into the status line.
 */

static void viewer_status(void) {

    if (viewer.count <= 0) {
        return;
    }


    const int current = viewer_current_page();
    const int percent = (int)(viewer.scale * 100.0 + 0.5);

    const fz_rect bounds = viewer.pages[current].bounds;

    char text[64];

    if (viewer.picture) {
        snprintf(text, sizeof(text), "%ld x %ld      %d%%", lround((bounds.x1 - bounds.x0) * viewer.unit), lround((bounds.y1 - bounds.y0) * viewer.unit), percent);
    } else {
        snprintf(text, sizeof(text), "page %d of %d      %d%%", current + 1, viewer.count, percent);
    }

    ui_label_set_text(viewer.status, text);
}


/**
 * @brief The display list of a page, recorded the first time the page is shown.
 *
 * @param index The page.
 * @return The list, or NULL when the page cannot be read.
 */

static fz_display_list* viewer_page_list(int index) {

    viewer_page_t* page = &viewer.pages[index];

    page->used = ++viewer.clock;

    if (page->list || page->broken) {
        return page->list;
    }


    int kept   = 0;
    int oldest = -1;

    for (int i = 0; i < viewer.count; i++) {

        if (!viewer.pages[i].list) {
            continue;
        }

        kept++;

        if (oldest < 0 || viewer.pages[i].used < viewer.pages[oldest].used) {
            oldest = i;
        }
    }

    if (kept >= VIEWER_LIST_CACHE) {

        fz_drop_display_list(viewer.ctx, viewer.pages[oldest].list);
        viewer.pages[oldest].list = NULL;
    }


    fz_display_list* list = NULL;

    fz_var(list);

    fz_try(viewer.ctx) {
        list = fz_new_display_list_from_page_number(viewer.ctx, viewer.document, index);
    }
    fz_catch(viewer.ctx) {

        fz_report_error(viewer.ctx);

        page->broken = true;
        return NULL;
    }

    page->list = list;

    return list;
}


/**
 * @brief Renders one tile of a page at the current scale.
 *
 * @param index The page.
 * @param column The tile column.
 * @param row The tile row.
 * @return The tile, or NULL when it could not be rendered.
 */

static cairo_surface_t* viewer_render_tile(int index, int column, int row) {

    const viewer_page_t* page = &viewer.pages[index];

    fz_display_list* list = viewer_page_list(index);

    if (!list) {
        return NULL;
    }


    const fz_irect bbox = {
        column * VIEWER_TILE_SIZE,
        row * VIEWER_TILE_SIZE,
        fz_mini((column + 1) * VIEWER_TILE_SIZE, page->width),
        fz_mini((row + 1) * VIEWER_TILE_SIZE, page->height),
    };

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, bbox.x1 - bbox.x0, bbox.y1 - bbox.y0);

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS || cairo_image_surface_get_stride(surface) != (bbox.x1 - bbox.x0) * 4) {
        cairo_surface_destroy(surface);
        return NULL;
    }


    const double sx = page->width / (page->bounds.x1 - page->bounds.x0);
    const double sy = page->height / (page->bounds.y1 - page->bounds.y0);

    const fz_matrix ctm = fz_concat(fz_translate(-page->bounds.x0, -page->bounds.y0), fz_scale(sx, sy));


    fz_pixmap* pixmap = NULL;
    fz_device* device = NULL;

    fz_var(pixmap);
    fz_var(device);

    cairo_surface_flush(surface);

    fz_try(viewer.ctx) {

        pixmap = fz_new_pixmap_with_bbox_and_data(viewer.ctx, fz_device_bgr(viewer.ctx), bbox, NULL, 1, cairo_image_surface_get_data(surface));

        fz_clear_pixmap_with_value(viewer.ctx, pixmap, 0xFF);

        device = fz_new_draw_device(viewer.ctx, fz_identity, pixmap);

        fz_run_display_list(viewer.ctx, list, device, ctm, fz_rect_from_irect(bbox), NULL);
        fz_close_device(viewer.ctx, device);
    }
    fz_always(viewer.ctx) {

        fz_drop_device(viewer.ctx, device);
        fz_drop_pixmap(viewer.ctx, pixmap);
    }
    fz_catch(viewer.ctx) {

        fz_report_error(viewer.ctx);

        cairo_surface_destroy(surface);
        return NULL;
    }

    cairo_surface_mark_dirty(surface);

    return surface;
}


/**
 * @brief A tile of a page, out of the cache or rendered into it in place of the one used longest ago.
 *
 * @param index The page.
 * @param column The tile column.
 * @param row The tile row.
 * @return The tile, owned by the cache, or NULL when it could not be rendered.
 */

static cairo_surface_t* viewer_tile(int index, int column, int row) {

    viewer_tile_t* slot = NULL;

    for (size_t i = 0; i < VIEWER_TILE_CACHE; i++) {

        viewer_tile_t* tile = &viewer.tiles[i];

        if (tile->surface && tile->page == index && tile->column == column && tile->row == row) {

            tile->used = ++viewer.clock;
            return tile->surface;
        }

        if (!slot || (slot->surface && (!tile->surface || tile->used < slot->used))) {
            slot = tile;
        }
    }


    cairo_surface_t* surface = viewer_render_tile(index, column, row);

    if (!surface) {
        return NULL;
    }

    if (slot->surface) {
        cairo_surface_destroy(slot->surface);
    }

    slot->surface = surface;
    slot->page    = index;
    slot->column  = column;
    slot->row     = row;
    slot->used    = ++viewer.clock;

    return surface;
}


/**
 * @brief Draws the part of a page the canvas shows, with a border around it.
 *
 * @param cr The context over the window's pixels, clipped to the canvas.
 * @param index The page.
 * @param rect Where the page is, in window coordinates.
 * @param canvas The rectangle the pages have to stay within.
 */

static void viewer_paint_page(cairo_t* cr, int index, ui_rect_t rect, ui_rect_t canvas) {

    const ui_theme_t* theme = ui_theme();

    cairo_set_source_rgba(cr, theme->border.r, theme->border.g, theme->border.b, theme->border.a);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, rect.x - 0.5, rect.y - 0.5, rect.width + 1.0, rect.height + 1.0);
    cairo_stroke(cr);


    const int x0 = fz_maxi(canvas.x, rect.x) - rect.x;
    const int y0 = fz_maxi(canvas.y, rect.y) - rect.y;
    const int x1 = fz_mini(canvas.x + canvas.width, rect.x + rect.width) - rect.x;
    const int y1 = fz_mini(canvas.y + canvas.height, rect.y + rect.height) - rect.y;

    if (x0 >= x1 || y0 >= y1) {
        return;
    }


    for (int row = y0 / VIEWER_TILE_SIZE; row <= (y1 - 1) / VIEWER_TILE_SIZE; row++) {

        for (int column = x0 / VIEWER_TILE_SIZE; column <= (x1 - 1) / VIEWER_TILE_SIZE; column++) {

            const int x = rect.x + column * VIEWER_TILE_SIZE;
            const int y = rect.y + row * VIEWER_TILE_SIZE;

            cairo_surface_t* tile = viewer_tile(index, column, row);

            if (tile) {
                cairo_set_source_surface(cr, tile, x, y);
            } else {
                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            }

            cairo_rectangle(cr, x, y, fz_mini(VIEWER_TILE_SIZE, rect.width - column * VIEWER_TILE_SIZE), fz_mini(VIEWER_TILE_SIZE, rect.height - row * VIEWER_TILE_SIZE));
            cairo_fill(cr);
        }
    }
}


/**
 * @brief Draws the pages the canvas shows, already clipped to it and in window coordinates.
 *
 * @param cr The context over the window's pixels.
 * @param canvas The rectangle inside the frame the pages have to stay within.
 */

static void viewer_paint(cairo_t* cr, ui_rect_t canvas) {

    const int origin_x = canvas.x - (int)lround(viewer.scroll_x);
    const int origin_y = canvas.y - (int)lround(viewer.scroll_y);

    for (int i = 0; i < viewer.count; i++) {

        const viewer_page_t* page = &viewer.pages[i];

        const ui_rect_t rect = {origin_x + page->x, origin_y + page->y, page->width, page->height};

        if (rect.y > canvas.y + canvas.height) {
            break;
        }

        if (rect.y + rect.height < canvas.y) {
            continue;
        }

        viewer_paint_page(cr, i, rect, canvas);
    }
}


/**
 * @brief Repaints the canvas over the frame the view has just laid down, and damages it.
 */

static void viewer_draw(void) {

    if (viewer.area.width <= 0 || viewer.area.height <= 0) {
        return;
    }


    cairo_surface_t* surface = cairo_image_surface_create_for_data((unsigned char*)ui_window_pixels(viewer.window), CAIRO_FORMAT_RGB24, ui_window_width(viewer.window), ui_window_height(viewer.window), (int)ui_window_stride(viewer.window));

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return;
    }


    cairo_t* cr = cairo_create(surface);

    const ui_rect_t inner = viewer_canvas();

    cairo_rectangle(cr, inner.x, inner.y, inner.width, inner.height);
    cairo_clip(cr);

    viewer_paint(cr, inner);

    cairo_destroy(cr);

    cairo_surface_flush(surface);
    cairo_surface_destroy(surface);


    ui_window_damage(viewer.window, viewer.area.x, viewer.area.y, viewer.area.width, viewer.area.height);
}


/**
 * @brief Releases the document, its pages and everything rendered out of them.
 */

static void viewer_close_document(void) {

    viewer_flush_tiles();

    for (int i = 0; i < viewer.count; i++) {
        fz_drop_display_list(viewer.ctx, viewer.pages[i].list);
    }

    free(viewer.pages);

    fz_drop_document(viewer.ctx, viewer.document);

    viewer.pages    = NULL;
    viewer.count    = 0;
    viewer.document = NULL;
    viewer.picture  = false;
    viewer.unit     = 1.0;
}


/**
 * @brief Reads the size of a page, falling back on a letter-sized one when the page cannot be read.
 *
 * @param document The document.
 * @param index The page.
 * @return The page's bounds, in points.
 */

static fz_rect viewer_page_bounds(fz_document* document, int index) {

    fz_page* page = NULL;
    fz_rect bounds = fz_empty_rect;

    fz_var(page);
    fz_var(bounds);

    fz_try(viewer.ctx) {

        page   = fz_load_page(viewer.ctx, document, index);
        bounds = fz_bound_page(viewer.ctx, page);
    }
    fz_always(viewer.ctx) {
        fz_drop_page(viewer.ctx, page);
    }
    fz_catch(viewer.ctx) {
        fz_report_error(viewer.ctx);
    }

    if (fz_is_empty_rect(bounds) || fz_is_infinite_rect(bounds)) {
        return fz_make_rect(0.0f, 0.0f, VIEWER_PAGE_WIDTH, VIEWER_PAGE_HEIGHT);
    }

    return bounds;
}


/**
 * @brief Reads the resolution of a picture, at which one of its pixels covers one window pixel at 1:1.
 *
 * @param path The file.
 * @return The horizontal resolution in dots per inch, or 0 when the file is not a picture.
 */

static int viewer_picture_resolution(const char* path) {

    fz_stream* stream = NULL;
    fz_image* image   = NULL;

    unsigned char head[VIEWER_WEBP_HEAD];

    int xres = 0;
    int yres = 0;

    fz_var(stream);
    fz_var(image);
    fz_var(xres);

    fz_try(viewer.ctx) {

        stream = fz_open_file(viewer.ctx, path);

        if (fz_read(viewer.ctx, stream, head, sizeof(head)) == sizeof(head)) {

            if (viewer_webp_recognize(head)) {

                xres = (int)VIEWER_POINTS_PER_INCH;

            } else if (fz_recognize_image_format(viewer.ctx, head) != FZ_IMAGE_UNKNOWN) {

                image = fz_new_image_from_file(viewer.ctx, path);

                fz_image_resolution(image, &xres, &yres);
            }
        }
    }
    fz_always(viewer.ctx) {

        fz_drop_image(viewer.ctx, image);
        fz_drop_stream(viewer.ctx, stream);
    }
    fz_catch(viewer.ctx) {

        fz_report_error(viewer.ctx);
        xres = 0;
    }

    return xres;
}


/**
 * @brief Opens a document and reads the size of every page, replacing whatever was loaded before.
 *
 * @param path The file to open.
 * @return NULL when the document is loaded, or what to tell the user when it is not.
 */

static const char* viewer_load(const char* path) {

    fz_document* document = NULL;

    int count = 0;

    bool locked = false;

    fz_var(document);
    fz_var(count);
    fz_var(locked);

    fz_try(viewer.ctx) {

        document = fz_open_document(viewer.ctx, path);

        if (fz_needs_password(viewer.ctx, document) && !fz_authenticate_password(viewer.ctx, document, "")) {
            locked = true;
        } else {
            count = fz_count_pages(viewer.ctx, document);
        }
    }
    fz_catch(viewer.ctx) {

        fz_report_error(viewer.ctx);
        fz_drop_document(viewer.ctx, document);

        return "cannot open this file";
    }


    const char* error = NULL;

    if (locked) {
        error = "this document needs a password";
    } else if (count <= 0) {
        error = "this document has no pages";
    }


    viewer_page_t* pages = error ? NULL : calloc((size_t)count, sizeof(viewer_page_t));

    if (!error && !pages) {
        error = "not enough memory for this document";
    }

    if (error) {

        fz_drop_document(viewer.ctx, document);
        return error;
    }


    for (int i = 0; i < count; i++) {
        pages[i].bounds = viewer_page_bounds(document, i);
    }

    const int resolution = viewer_picture_resolution(path);

    viewer.document = document;
    viewer.pages    = pages;
    viewer.count    = count;
    viewer.picture  = resolution > 0;
    viewer.unit     = resolution > 0 ? resolution / VIEWER_POINTS_PER_INCH : 1.0;

    return NULL;
}


/**
 * @brief Shows a file, replacing whatever was loaded before it.
 *
 * @param path The file to open.
 * @return 0 when the document is on screen, -1 when the file could not be shown.
 */

static int viewer_open(const char* path) {

    viewer_close_document();

    const char* error = viewer_load(path);


    snprintf(viewer.path, sizeof(viewer.path), "%s", path);

    const char* name = strrchr(viewer.path, '/');

    name = name ? name + 1 : viewer.path;

    ui_label_set_text(viewer.name, name);


    char title[UI_TITLE_MAX];

    snprintf(title, sizeof(title), "%.40s - %s", name, VIEWER_NAME);

    ui_window_set_title(viewer.window, error ? VIEWER_NAME : title);


    viewer.scroll_x = 0.0;
    viewer.scroll_y = 0.0;

    viewer_set_scale(1.0);

    if (error) {

        ui_label_set_text(viewer.status, error);

    } else {

        viewer_refit();
        viewer_status();
    }

    ui_view_invalidate(viewer.view);

    return error ? -1 : 0;
}


/**
 * @brief Runs a toolbar action.
 *
 * @param widget The button that was clicked.
 * @param user The action, carried as the button's user pointer.
 */

static void viewer_on_action(ui_widget_t* widget, void* user) {

    (void)widget;

    switch ((viewer_action_t)(intptr_t)user) {

        case VIEWER_ACTION_ZOOM_OUT:
            viewer_zoom_centered(viewer.scale / VIEWER_SCALE_STEP);
            break;

        case VIEWER_ACTION_ZOOM_IN:
            viewer_zoom_centered(viewer.scale * VIEWER_SCALE_STEP);
            break;

        case VIEWER_ACTION_FIT:
            viewer_refit();
            break;

        case VIEWER_ACTION_ACTUAL:
            viewer_zoom_centered(1.0);
            break;

        default:
            return;
    }

    viewer_status();
    ui_view_invalidate(viewer.view);
}


/**
 * @brief Reports whether a window point is inside the canvas.
 *
 * @param x The window x.
 * @param y The window y.
 * @return true when the point is over the canvas.
 */

static bool viewer_inside(int x, int y) {

    const ui_rect_t canvas = viewer_canvas();

    return x >= canvas.x && y >= canvas.y && x < canvas.x + canvas.width && y < canvas.y + canvas.height;
}


/**
 * @brief Drags the document with the left button, the gesture only starting inside the canvas.
 *
 * @param event The pointer event.
 */

static void viewer_on_pointer(const ui_event_t* event) {

    const int x = event->pointer.x;
    const int y = event->pointer.y;

    viewer.pointer_x = x;
    viewer.pointer_y = y;


    if (!(event->pointer.buttons & UI_BUTTON_LEFT)) {

        viewer.panning = false;
        return;
    }


    if (!viewer.panning) {

        if (viewer.count <= 0 || !viewer_inside(x, y)) {
            return;
        }

        viewer.panning = true;
        viewer.pan_x   = x;
        viewer.pan_y   = y;

        return;
    }


    viewer_scroll_by(viewer.pan_x - x, viewer.pan_y - y);

    viewer.pan_x = x;
    viewer.pan_y = y;

    viewer_status();
    ui_view_invalidate(viewer.view);
}


/**
 * @brief Scrolls the document a detent at a time, or zooms about the pointer over a picture or while Ctrl is held.
 *
 * @param event The scroll event.
 */

static void viewer_on_scroll(const ui_event_t* event) {

    if (viewer.count <= 0) {
        return;
    }


    if (viewer.ctrl || viewer.picture) {

        double scale = viewer.scale;

        int steps = event->scroll.dy;

        for (; steps > 0; steps--) {
            scale *= VIEWER_SCALE_STEP;
        }

        for (; steps < 0; steps++) {
            scale /= VIEWER_SCALE_STEP;
        }

        if (viewer_inside(viewer.pointer_x, viewer.pointer_y)) {
            viewer_zoom_at(scale, viewer.pointer_x, viewer.pointer_y);
        } else {
            viewer_zoom_centered(scale);
        }

    } else {

        viewer_scroll_by(-event->scroll.dx * VIEWER_SCROLL_STEP, -event->scroll.dy * VIEWER_SCROLL_STEP);
    }

    viewer_status();
    ui_view_invalidate(viewer.view);
}


/**
 * @brief Folds a Ctrl key into the modifier state.
 *
 * @param vkey The KEY_* code.
 * @param down Whether the key went down or came up.
 */

static void viewer_track_ctrl(uint16_t vkey, bool down) {

    uint8_t bit = 0;

    switch (vkey) {

        case KEY_LEFTCTRL:
            bit = VIEWER_CTRL_LEFT;
            break;

        case KEY_RIGHTCTRL:
            bit = VIEWER_CTRL_RIGHT;
            break;

        default:
            return;
    }

    if (down) {
        viewer.ctrl |= bit;
    } else {
        viewer.ctrl &= (uint8_t)~bit;
    }
}


/**
 * @brief Handles a key the widgets did not want.
 *
 * @param view The view the key arrived on.
 * @param vkey The KEY_* code.
 * @param down Whether the key went down or came up.
 * @param user Unused.
 * @return true when the key was acted on.
 */

static bool viewer_on_key(ui_view_t* view, uint16_t vkey, bool down, void* user) {

    (void)view;
    (void)user;

    viewer_track_ctrl(vkey, down);

    if (!down) {
        return false;
    }


    const ui_rect_t canvas = viewer_canvas();

    const int page = canvas.height > VIEWER_SCROLL_STEP * 2 ? canvas.height - VIEWER_SCROLL_STEP : canvas.height;

    switch (vkey) {

        case KEY_ESC:
        case KEY_Q:
            viewer.quit = true;
            return true;

        case KEY_EQUAL:
        case KEY_KPPLUS:
            viewer_zoom_centered(viewer.scale * VIEWER_SCALE_STEP);
            break;

        case KEY_MINUS:
        case KEY_KPMINUS:
            viewer_zoom_centered(viewer.scale / VIEWER_SCALE_STEP);
            break;

        case KEY_0:
        case KEY_F:
            viewer_refit();
            break;

        case KEY_1:
            viewer_zoom_centered(1.0);
            break;

        case KEY_LEFT:
            viewer_scroll_by(-VIEWER_SCROLL_STEP, 0);
            break;

        case KEY_RIGHT:
            viewer_scroll_by(VIEWER_SCROLL_STEP, 0);
            break;

        case KEY_UP:
            viewer_scroll_by(0, -VIEWER_SCROLL_STEP);
            break;

        case KEY_DOWN:
            viewer_scroll_by(0, VIEWER_SCROLL_STEP);
            break;

        case KEY_PAGEUP:
            viewer_scroll_by(0, -page);
            break;

        case KEY_PAGEDOWN:
        case KEY_SPACE:
            viewer_scroll_by(0, page);
            break;

        case KEY_HOME:
            viewer_scroll_by(0, -viewer.doc_height);
            break;

        case KEY_END:
            viewer_scroll_by(0, viewer.doc_height);
            break;

        default:
            return false;
    }


    viewer_status();
    ui_view_invalidate(viewer.view);

    return true;
}


/**
 * @brief Places the widgets, and records the rectangle the pages are drawn in.
 *
 * @param view Unused.
 * @param width The content width.
 * @param height The content height.
 * @param user Unused.
 */

static void viewer_layout(ui_view_t* view, int width, int height, void* user) {

    (void)view;
    (void)user;

    const int inner = width - VIEWER_MARGIN * 2;

    ui_rect_t toolbar = {VIEWER_MARGIN, VIEWER_MARGIN, inner > 0 ? inner : 0, VIEWER_TOOLBAR_HEIGHT};

    ui_widget_place(viewer.toolbar, toolbar);


    int x = toolbar.x + toolbar.width - 4;

    for (int i = (int)(sizeof(viewer_buttons) / sizeof(viewer_buttons[0])) - 1; i >= 0; i--) {

        ui_rect_t rect = {x - viewer_buttons[i].width, toolbar.y + 4, viewer_buttons[i].width, toolbar.height - 8};

        ui_widget_place(viewer.buttons[viewer_buttons[i].action], rect);

        x -= viewer_buttons[i].width + 4;
    }


    ui_rect_t name = {toolbar.x, toolbar.y, x - toolbar.x, toolbar.height};

    if (name.width < 0) {
        name.width = 0;
    }

    ui_widget_place(viewer.name, name);


    ui_rect_t status = {VIEWER_MARGIN, height - VIEWER_MARGIN - VIEWER_STATUS_HEIGHT, toolbar.width, VIEWER_STATUS_HEIGHT};

    ui_widget_place(viewer.status, status);


    const int frame_y = toolbar.y + toolbar.height + VIEWER_GAP;

    int frame_height = status.y - frame_y - VIEWER_GAP;

    if (frame_height < 0) {
        frame_height = 0;
    }

    ui_rect_t frame = {VIEWER_MARGIN, frame_y, toolbar.width, frame_height};

    ui_widget_place(viewer.frame, frame);

    viewer.area = frame;


    if (viewer.fit) {
        viewer_refit();
    } else {
        viewer_clamp_scroll();
    }

    viewer_status();
}


/**
 * @brief Creates the widgets.
 *
 * @param view The view to create them in.
 * @return 0 on success, -1 when one of them could not be created.
 */

static int viewer_build(ui_view_t* view) {

    const ui_theme_t* theme = ui_theme();

    viewer.toolbar = ui_panel_create(view);
    viewer.name    = ui_label_create(view, VIEWER_NAME);
    viewer.frame   = ui_panel_create(view);
    viewer.status  = ui_label_create(view, "no document");

    if (!viewer.toolbar || !viewer.name || !viewer.frame || !viewer.status) {
        return -1;
    }


    for (size_t i = 0; i < sizeof(viewer_buttons) / sizeof(viewer_buttons[0]); i++) {

        ui_widget_t* button = ui_button_create(view, viewer_buttons[i].label, viewer_on_action, (void*)(intptr_t)viewer_buttons[i].action);

        if (!button) {
            return -1;
        }

        viewer.buttons[viewer_buttons[i].action] = button;
    }


    ui_panel_set_color(viewer.toolbar, theme->surface);
    ui_panel_set_radius(viewer.toolbar, theme->corner_radius);

    ui_panel_set_color(viewer.frame, theme->surface_sunken);
    ui_panel_set_radius(viewer.frame, theme->corner_radius);
    ui_panel_set_border(viewer.frame, theme->border, 1.0);

    ui_label_set_padding(viewer.name, 8);

    ui_label_set_color(viewer.status, theme->text_muted);
    ui_label_set_align(viewer.status, UI_ALIGN_RIGHT);

    return 0;
}


/**
 * @brief The event loop: ui_view_run() with the canvas painted over every frame the view sends out.
 *
 * @return 0 on a clean close, -1 otherwise.
 */

static int viewer_run(void) {

    while (!ui_view_closed(viewer.view) && !viewer.quit) {

        const int timeout = ui_view_timeout(viewer.view);

        const int painted = ui_view_present(viewer.view);

        if (painted < 0) {
            return -1;
        }

        if (painted > 0) {

            viewer_draw();

            if (ui_window_commit(viewer.window) < 0) {
                return -1;
            }
        }


        ui_event_t event;

        const int e = ui_next_event(viewer.conn, &event, timeout);

        if (e < 0) {
            return -1;
        }

        if (e == 0) {
            continue;
        }

        ui_view_dispatch(viewer.view, &event);


        switch (event.type) {

            case UI_EVENT_POINTER:
                viewer_on_pointer(&event);
                break;

            case UI_EVENT_SCROLL:
                viewer_on_scroll(&event);
                break;

            case UI_EVENT_LEAVE:
                viewer.panning = false;
                break;

            case UI_EVENT_FOCUS:

                if (!event.focus.focused) {
                    viewer.panning = false;
                    viewer.ctrl    = 0;
                }

                break;

            default:
                break;
        }
    }

    return 0;
}


/**
 * @brief Releases whatever main() got as far as creating.
 */

static void viewer_shutdown(void) {

    if (viewer.ctx) {

        viewer_close_document();
        fz_drop_context(viewer.ctx);
    }

    if (viewer.view) {
        ui_view_destroy(viewer.view);
    }

    if (viewer.window) {
        ui_window_destroy(viewer.window);
    }

    if (viewer.conn) {
        ui_disconnect(viewer.conn);
    }
}


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);

    viewer.unit  = 1.0;
    viewer.scale = 1.0;
    viewer.fit   = true;


    viewer.ctx = fz_new_context(NULL, NULL, VIEWER_STORE_SIZE);

    if (!viewer.ctx) {
        fprintf(stderr, "%s: fz_new_context() failed\n", VIEWER_NAME);
        return 1;
    }

    fz_try(viewer.ctx) {

        fz_register_document_handlers(viewer.ctx);
        fz_register_document_handler(viewer.ctx, &viewer_webp_handler);
    }
    fz_catch(viewer.ctx) {

        fz_report_error(viewer.ctx);
        fz_drop_context(viewer.ctx);

        return 1;
    }


    viewer.conn = ui_connect(NULL, 5000);

    if (!viewer.conn) {
        fprintf(stderr, "%s: ui_connect() failed: %s\n", VIEWER_NAME, strerror(errno));
        viewer_shutdown();
        return 1;
    }


    viewer.window = ui_window_create(viewer.conn, VIEWER_WINDOW_WIDTH, VIEWER_WINDOW_HEIGHT, VIEWER_NAME);

    if (!viewer.window) {
        fprintf(stderr, "%s: ui_window_create() failed: %s\n", VIEWER_NAME, strerror(errno));
        viewer_shutdown();
        return 1;
    }


    viewer.view = ui_view_create(viewer.window);

    if (!viewer.view) {
        fprintf(stderr, "%s: ui_view_create() failed: %s\n", VIEWER_NAME, strerror(errno));
        viewer_shutdown();
        return 1;
    }


    if (viewer_build(viewer.view) < 0) {
        fprintf(stderr, "%s: cannot create the widgets\n", VIEWER_NAME);
        viewer_shutdown();
        return 1;
    }

    ui_view_on_key(viewer.view, viewer_on_key, NULL);
    ui_view_on_layout(viewer.view, viewer_layout, NULL);


    if (argc > 1 && viewer_open(argv[1]) < 0) {
        fprintf(stderr, "%s: cannot show %s\n", VIEWER_NAME, argv[1]);
    }


    const int status = viewer_run();

    if (status < 0) {
        fprintf(stderr, "%s: the connection went away: %s\n", VIEWER_NAME, strerror(errno));
    }

    viewer_shutdown();

    return status < 0 ? 1 : 0;
}
