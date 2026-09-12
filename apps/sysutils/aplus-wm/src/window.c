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

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cairo/cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <wm.h>


#define WM_FONT_PATH "/usr/share/fonts/ttf/Ubuntu-R.ttf"
#define WM_FONT_SIZE 14.0


static FT_Library wm_ft            = NULL;
static FT_Face wm_ft_face          = NULL;
static cairo_font_face_t* wm_face  = NULL;


int wm_font_init(void) {

    /* There is no fontconfig in the sysroot, so cairo's toy font API has nothing to
       resolve a family name against. Loading the file explicitly through FreeType is the
       only way to get a usable face. */
    if (FT_Init_FreeType(&wm_ft) != 0) {
        fprintf(stderr, "aplus-wm: warning: cannot initialize freetype, titles will be blank\n");
        return -1;
    }

    if (FT_New_Face(wm_ft, WM_FONT_PATH, 0, &wm_ft_face) != 0) {
        fprintf(stderr, "aplus-wm: warning: cannot load %s, titles will be blank\n", WM_FONT_PATH);
        return -1;
    }

    if ((wm_face = cairo_ft_font_face_create_for_ft_face(wm_ft_face, 0)) == NULL) {
        fprintf(stderr, "aplus-wm: warning: cannot create a cairo font face, titles will be blank\n");
        return -1;
    }

    return 0;
}


void wm_font_fini(void) {

    if (wm_face) {
        cairo_font_face_destroy(wm_face);
        wm_face = NULL;
    }

    if (wm_ft_face) {
        FT_Done_Face(wm_ft_face);
        wm_ft_face = NULL;
    }

    if (wm_ft) {
        FT_Done_FreeType(wm_ft);
        wm_ft = NULL;
    }
}


wm_rect_t wm_window_frame(const wm_window_t* win) {

    wm_rect_t r = {

        .x      = win->x - WM_BORDER_WIDTH,
        .y      = win->y - WM_TITLEBAR_HEIGHT,
        .width  = win->width + 2 * WM_BORDER_WIDTH,
        .height = win->height + WM_TITLEBAR_HEIGHT + WM_BORDER_WIDTH,
    };

    return r;
}


/* Everything the shadow can reach. Kept apart from wm_window_frame(), which stays the
   interactive outline: the shadow must be repainted with the window or it leaves a trail
   behind a drag, but it must not be clickable. */
wm_rect_t wm_window_shadow_rect(const wm_window_t* win) {

    const wm_rect_t f = wm_window_frame(win);

    wm_rect_t r = {

        .x      = f.x - WM_SHADOW_EXTENT,
        .y      = f.y - WM_SHADOW_EXTENT,
        .width  = f.width + 2 * WM_SHADOW_EXTENT,
        .height = f.height + 2 * WM_SHADOW_EXTENT + WM_SHADOW_OFFSET,
    };

    return r;
}


wm_rect_t wm_window_close_rect(const wm_window_t* win) {

    const wm_rect_t f = wm_window_frame(win);

    wm_rect_t r = {

        .x      = f.x + f.width - WM_BUTTON_MARGIN - WM_BUTTON_SIZE,
        .y      = f.y + (WM_TITLEBAR_HEIGHT - WM_BUTTON_SIZE) / 2,
        .width  = WM_BUTTON_SIZE,
        .height = WM_BUTTON_SIZE,
    };

    return r;
}


wm_window_t* wm_window_from_id(uint32_t id) {

    for (wm_window_t* w = wm.windows; w; w = w->next) {

        if (w->id == id) {
            return w;
        }
    }

    return NULL;
}


/* Allocates the pixel store only; the window's geometry is tracked separately so that a
   resize drag can move the frame around without reallocating a surface per mouse packet. */
static int wm_window_alloc_backstore(wm_window_t* win, int width, int height) {

    /* RGB24 rather than ARGB32: windows are opaque, and cairo's ARGB32 wants premultiplied
       data, which nothing on the client side produces. The memory layout is identical, so
       a client writing 0xFFRRGGBB lands exactly where it expects to. */
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return -1;
    }


    cairo_t* cr = cairo_create(surface);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    /* Carry the old contents over. Without this a window goes black for the whole of a
       resize drag, because the client is not told the new size until the mouse is
       released and so has nothing to redraw with in the meantime. */
    if (win->backstore) {

        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface(cr, win->backstore, 0, 0);
        cairo_rectangle(cr, 0, 0, win->width < width ? win->width : width, win->height < height ? win->height : height);
        cairo_fill(cr);
    }

    cairo_destroy(cr);


    if (win->backstore) {
        cairo_surface_destroy(win->backstore);
    }

    win->backstore = surface;

    return 0;
}


/* The size a window is allowed to have. A window that does not fit gets shrunk rather
   than refused: a client asking for a size larger than the screen is asking for "as much
   as you have".
 *
 * The upper bound is not cosmetic. A drag on the north or west edge moves the origin as
 * well as the size, so dragging a corner back to (0, 0) adds the window's own offset to
 * its width and height -- and with only the lower bound enforced, "move it away, drag the
 * corner back" grew the window by most of the display every time round. Both sides of the
 * socket allocate width * height * 4 bytes to follow it, so the pair of them consumed
 * quadratically more memory per cycle until calloc() failed.
 *
 * The minimum is applied last so that a display too small to hold one still yields a
 * window that can be grabbed. */
void wm_window_clamp_size(int* width, int* height) {

    const int max_width  = wm.display.width - 2 * WM_BORDER_WIDTH;
    const int max_height = wm.display.height - WM_TITLEBAR_HEIGHT - WM_BORDER_WIDTH;

    if (*width > max_width) {
        *width = max_width;
    }

    if (*height > max_height) {
        *height = max_height;
    }

    if (*width < WM_WINDOW_MIN_WIDTH) {
        *width = WM_WINDOW_MIN_WIDTH;
    }

    if (*height < WM_WINDOW_MIN_HEIGHT) {
        *height = WM_WINDOW_MIN_HEIGHT;
    }
}


wm_window_t* wm_window_create(wm_client_t* client, int width, int height, const char* title) {

    wm_window_clamp_size(&width, &height);


    wm_window_t* win = (wm_window_t*)calloc(1, sizeof(wm_window_t));

    if (!win) {
        return NULL;
    }

    win->id     = wm.next_window_id++;
    win->client = client;
    win->serial = 1;

    if (title) {
        strncpy(win->title, title, UI_TITLE_MAX - 1);
    }


    win->width  = width;
    win->height = height;

    if (wm_window_alloc_backstore(win, width, height) < 0) {
        free(win);
        return NULL;
    }


    /* Cascade from the top-left so that a second client does not land exactly on top of
       the first one. */
    static int cascade = 0;

    win->x = WM_BORDER_WIDTH + 24 * (cascade % 8);
    win->y = WM_TITLEBAR_HEIGHT + 24 * (cascade % 8);

    cascade++;

    if (win->x + win->width + WM_BORDER_WIDTH > wm.display.width) {
        win->x = wm.display.width - win->width - WM_BORDER_WIDTH;
    }

    if (win->y + win->height + WM_BORDER_WIDTH > wm.display.height) {
        win->y = wm.display.height - win->height - WM_BORDER_WIDTH;
    }


    win->next  = wm.windows;
    wm.windows = win;

    wm_damage_window(win);

    return win;
}


/* Asking rather than destroying: the client owns the window. The one client there is exits
   on this event, which drops the socket and takes the window with it; a client that ignores
   it keeps its window, which is the whole point of making it a request. */
void wm_window_request_close(wm_window_t* win) {

    if (!win) {
        return;
    }


    ui_msg_window_t msg = {.window_id = win->id};

    wm_client_queue(win->client, UI_EV_CLOSE, &msg, sizeof(msg));
}


void wm_window_destroy(wm_window_t* win) {

    if (!win) {
        return;
    }


    wm_damage_window(win);


    wm_window_t** it = &wm.windows;

    while (*it) {

        if (*it == win) {
            *it = win->next;
            break;
        }

        it = &(*it)->next;
    }


    if (wm.drag.window == win) {
        wm.drag.window = NULL;
        wm.drag.region = WM_REGION_NONE;
    }

    if (wm.hovered_close == win) {
        wm.hovered_close = NULL;
    }

    if (wm.focused == win) {
        wm.focused = NULL;
        wm_window_focus(wm.windows);
    }


    if (win->backstore) {
        cairo_surface_destroy(win->backstore);
    }

    free(win);
}


static wm_region_t wm_window_region(const wm_window_t* win, int x, int y) {

    const wm_rect_t f = wm_window_frame(win);

    if (x < f.x || y < f.y || x >= f.x + f.width || y >= f.y + f.height) {
        return WM_REGION_NONE;
    }

    /* The content area is tested first, so the resize grips only ever cover the border
       itself however generous WM_RESIZE_GRIP is. */
    if (x >= win->x && x < win->x + win->width && y >= win->y && y < win->y + win->height) {
        return WM_REGION_CONTENT;
    }


    /* Ahead of the grips on purpose. The button sits eight pixels down from the top of the
       frame and within the north-east corner's reach, so testing the grips first would
       hand them its top row and the window would resize instead of closing. */
    const wm_rect_t c = wm_window_close_rect(win);

    if (x >= c.x && x < c.x + c.width && y >= c.y && y < c.y + c.height) {
        return WM_REGION_CLOSE;
    }


    /* An edge is only an edge within the border thickness. Measuring it with the corner
       reach instead would put the top 16 pixels of a 32 pixel titlebar inside the north
       resize zone, and dragging a window by its title would resize it. */
    const bool left   = x < f.x + WM_BORDER_WIDTH;
    const bool right  = x >= f.x + f.width - WM_BORDER_WIDTH;
    const bool top    = y < f.y + WM_BORDER_WIDTH;
    const bool bottom = y >= f.y + f.height - WM_BORDER_WIDTH;

    /* Corners reach further, but only *along* an edge: the grip is the L-shaped bit of
       border near the corner, never a square cut out of the content or the titlebar. */
    const bool near_left   = x < f.x + WM_RESIZE_GRIP;
    const bool near_right  = x >= f.x + f.width - WM_RESIZE_GRIP;
    const bool near_top    = y < f.y + WM_RESIZE_GRIP;
    const bool near_bottom = y >= f.y + f.height - WM_RESIZE_GRIP;

    if ((top && near_left) || (left && near_top)) {
        return WM_REGION_RESIZE_NW;
    }

    if ((top && near_right) || (right && near_top)) {
        return WM_REGION_RESIZE_NE;
    }

    if ((bottom && near_left) || (left && near_bottom)) {
        return WM_REGION_RESIZE_SW;
    }

    if ((bottom && near_right) || (right && near_bottom)) {
        return WM_REGION_RESIZE_SE;
    }

    if (left) {
        return WM_REGION_RESIZE_W;
    }

    if (right) {
        return WM_REGION_RESIZE_E;
    }

    if (bottom) {
        return WM_REGION_RESIZE_S;
    }

    if (top) {
        return WM_REGION_RESIZE_N;
    }

    return WM_REGION_TITLEBAR;
}


wm_region_t wm_window_hit_test(int x, int y, wm_window_t** out) {

    for (wm_window_t* win = wm.windows; win; win = win->next) {

        wm_region_t region = wm_window_region(win, x, y);

        if (region == WM_REGION_NONE) {
            continue;
        }

        if (out) {
            *out = win;
        }

        return region;
    }

    if (out) {
        *out = NULL;
    }

    return WM_REGION_NONE;
}


void wm_window_raise(wm_window_t* win) {

    if (!win || wm.windows == win) {
        return;
    }


    wm_window_t** it = &wm.windows;

    while (*it) {

        if (*it == win) {
            *it = win->next;
            break;
        }

        it = &(*it)->next;
    }

    win->next  = wm.windows;
    wm.windows = win;

    wm_damage_window(win);
}


void wm_window_focus(wm_window_t* win) {

    if (wm.focused == win) {
        return;
    }


    wm_window_t* previous = wm.focused;

    wm.focused = win;


    /* The titlebar is drawn differently for the focused window, so both frames have to be
       repainted even though nothing about their contents changed. */
    if (previous) {

        ui_msg_focus_t msg = {.window_id = previous->id, .focused = 0};

        wm_client_queue(previous->client, UI_EV_FOCUS, &msg, sizeof(msg));
        wm_damage_window(previous);
    }

    if (win) {

        ui_msg_focus_t msg = {.window_id = win->id, .focused = 1};

        wm_client_queue(win->client, UI_EV_FOCUS, &msg, sizeof(msg));
        wm_damage_window(win);
    }
}


void wm_window_move(wm_window_t* win, int x, int y) {

    /* Keep at least the titlebar reachable: a window dragged fully off-screen could never
       be dragged back. */
    const int min_x = -(win->width - WM_WINDOW_MIN_WIDTH);
    const int min_y = WM_TITLEBAR_HEIGHT;

    if (x < min_x) {
        x = min_x;
    }

    if (y < min_y) {
        y = min_y;
    }

    if (x > wm.display.width - WM_WINDOW_MIN_WIDTH) {
        x = wm.display.width - WM_WINDOW_MIN_WIDTH;
    }

    if (y > wm.display.height - WM_BORDER_WIDTH) {
        y = wm.display.height - WM_BORDER_WIDTH;
    }

    if (x == win->x && y == win->y) {
        return;
    }


    wm_damage_window(win);

    win->x = x;
    win->y = y;

    wm_damage_window(win);
}


int wm_window_resize(wm_window_t* win, int width, int height) {

    wm_window_clamp_size(&width, &height);

    if (width == win->width && height == win->height) {
        return 0;
    }


    wm_damage_window(win);

    win->width  = width;
    win->height = height;

    /* Every in-flight commit describes the size the client last heard about. Bumping the
       serial is what lets those be recognised and dropped rather than blitted at the wrong
       stride. */
    win->serial++;

    wm_damage_window(win);

    return 0;
}


/* Kept apart from wm_window_resize() on purpose. A resize drag walks through a new size
   on every mouse packet, and announcing each one would make the client repaint and
   re-upload the whole surface a hundred times a second -- over a socket, since there is
   no shared memory to hand it. The server tracks the geometry live and reaches here once,
   when the drag ends.
 *
 * Reallocating the backstore here rather than in wm_window_resize() matters for the same
 * reason it matters on the client: one allocation per drag instead of one per mouse
 * packet, on a kernel whose mmap cursor never rewinds.
 */
int wm_window_notify_configure(wm_window_t* win) {

    if (cairo_image_surface_get_width(win->backstore) != win->width || cairo_image_surface_get_height(win->backstore) != win->height) {

        if (wm_window_alloc_backstore(win, win->width, win->height) < 0) {

            /* Nothing to draw the new size into. Keep the size that does exist rather than
               leaving the window describing a surface that was never allocated: every
               commit against it would be rejected by wm_window_blit() and the client
               dropped for what is really this server's failure. */
            fprintf(stderr, "aplus-wm: cannot allocate a %dx%d backstore, keeping the window at %dx%d\n", win->width, win->height, cairo_image_surface_get_width(win->backstore), cairo_image_surface_get_height(win->backstore));

            win->width  = cairo_image_surface_get_width(win->backstore);
            win->height = cairo_image_surface_get_height(win->backstore);
        }

        wm_damage_window(win);
    }


    ui_msg_configure_t msg = {

        .window_id = win->id,
        .serial    = win->serial,
        .width     = (uint16_t)win->width,
        .height    = (uint16_t)win->height,
    };

    return wm_client_queue(win->client, UI_EV_CONFIGURE, &msg, sizeof(msg));
}


int wm_window_blit(wm_window_t* win, int x, int y, int width, int height, const uint8_t* pixels) {

    if (x < 0 || y < 0 || width <= 0 || height <= 0) {
        errno = EINVAL;
        return -1;
    }

    /* Bounded by the surface rather than by win->width/height: during a resize drag the
       two disagree on purpose, and it is the surface that says how much memory there
       actually is. */
    if (x + width > cairo_image_surface_get_width(win->backstore) || y + height > cairo_image_surface_get_height(win->backstore)) {
        errno = EINVAL;
        return -1;
    }


    cairo_surface_flush(win->backstore);

    unsigned char* data = cairo_image_surface_get_data(win->backstore);
    const int stride    = cairo_image_surface_get_stride(win->backstore);

    const size_t row = (size_t)width * sizeof(uint32_t);

    for (int i = 0; i < height; i++) {
        memcpy(data + (size_t)(y + i) * (size_t)stride + (size_t)x * sizeof(uint32_t), pixels + (size_t)i * row, row);
    }

    cairo_surface_mark_dirty_rectangle(win->backstore, x, y, width, height);

    return 0;
}


/* A rectangle with all four corners rounded, as a path. */
void wm_rounded_rect(cairo_t* cr, double x, double y, double width, double height, double radius) {

    if (radius > width / 2.0) {
        radius = width / 2.0;
    }

    if (radius > height / 2.0) {
        radius = height / 2.0;
    }

    if (radius < 0.0) {
        radius = 0.0;
    }


    cairo_new_sub_path(cr);

    cairo_arc(cr, x + width - radius, y + radius, radius, -M_PI / 2.0, 0.0);
    cairo_arc(cr, x + width - radius, y + height - radius, radius, 0.0, M_PI / 2.0);
    cairo_arc(cr, x + radius, y + height - radius, radius, M_PI / 2.0, M_PI);
    cairo_arc(cr, x + radius, y + radius, radius, M_PI, 3.0 * M_PI / 2.0);

    cairo_close_path(cr);
}


static void wm_window_paint_shadow(cairo_t* cr, const wm_rect_t* f, bool focused) {

    cairo_save(cr);

    /* Only the ring outside the frame can ever show, because the window is painted opaque
       over the rest of it. Clipping the frame away first means each layer below rasterises
       a border rather than a whole window -- and it is what keeps a shadow affordable
       without a GPU to blur with. */
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);

    cairo_rectangle(cr, f->x - WM_SHADOW_EXTENT, f->y - WM_SHADOW_EXTENT, f->width + 2 * WM_SHADOW_EXTENT, f->height + 2 * WM_SHADOW_EXTENT + WM_SHADOW_OFFSET);
    wm_rounded_rect(cr, f->x, f->y, f->width, f->height, WM_CORNER_RADIUS);

    cairo_clip(cr);


    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    const double strength = WM_SHADOW_ALPHA * (focused ? 1.0 : 0.55);

    /* Nested rounded rectangles, widest first. Where more of them overlap the black
       accumulates, so the edge fades out instead of stopping dead -- a blur for the price
       of a handful of fills. Pushed down by WM_SHADOW_OFFSET so the light reads as coming
       from above.
     *
     * The layers are not equally opaque. Giving them all the same alpha puts a step of the
     * full layer alpha where the outermost one meets the desktop, and that reads as a box
     * drawn around the window; fading the outer layers to nothing hides where the shadow
     * ends. */
    for (int i = WM_SHADOW_LAYERS; i > 0; i--) {

        const double t      = (double)(i - 1) / (double)WM_SHADOW_LAYERS;
        const double spread = (double)WM_SHADOW_EXTENT * (double)i / (double)WM_SHADOW_LAYERS;

        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, strength * (1.0 - t) * (1.0 - t));

        wm_rounded_rect(cr, f->x - spread, f->y - spread + WM_SHADOW_OFFSET, f->width + 2.0 * spread, f->height + 2.0 * spread, WM_CORNER_RADIUS + spread);

        cairo_fill(cr);
    }

    cairo_restore(cr);
}


static void wm_window_paint_title(cairo_t* cr, const wm_window_t* win, const wm_rect_t* f, bool focused) {

    if (!wm_face || !win->title[0]) {
        return;
    }


    /* The strip the title gets to itself: from the left inset up to a gap before the close
       button. Centring in the whole titlebar instead would run a long title underneath the
       button, and clipping alone would not stop it. */
    const double left  = f->x + 8;
    const double right = wm_window_close_rect(win).x - 6;

    if (right - left < 8.0) {
        return;
    }


    cairo_save(cr);

    cairo_set_font_face(cr, wm_face);
    cairo_set_font_size(cr, WM_FONT_SIZE);

    cairo_rectangle(cr, left, f->y, right - left, WM_TITLEBAR_HEIGHT);
    cairo_clip(cr);


    cairo_text_extents_t extents;

    cairo_text_extents(cr, win->title, &extents);

    double x = left + (right - left - extents.width) / 2.0 - extents.x_bearing;
    double y = f->y + (WM_TITLEBAR_HEIGHT + extents.height) / 2.0;

    if (x < left) {
        x = left;
    }

    if (focused) {
        cairo_set_source_rgb(cr, WM_COLOR_TITLE_ACTIVE);
    } else {
        cairo_set_source_rgb(cr, WM_COLOR_TITLE_IDLE);
    }

    cairo_move_to(cr, x, y);
    cairo_show_text(cr, win->title);

    cairo_restore(cr);
}


static void wm_window_paint_close(cairo_t* cr, const wm_window_t* win, bool focused) {

    const wm_rect_t c = wm_window_close_rect(win);

    const bool hovered = (wm.hovered_close == win);
    const bool pressed = (hovered && wm.drag.window == win && wm.drag.region == WM_REGION_CLOSE);


    cairo_save(cr);

    /* Nothing at all until the pointer arrives -- a permanently red button in a titlebar
       this dark would be the loudest thing on the screen. */
    if (hovered) {

        if (pressed) {
            cairo_set_source_rgb(cr, WM_COLOR_CLOSE_DOWN);
        } else {
            cairo_set_source_rgb(cr, WM_COLOR_CLOSE_OVER);
        }

        wm_rounded_rect(cr, c.x, c.y, c.width, c.height, 4.0);
        cairo_fill(cr);
    }


    if (hovered) {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    } else if (focused) {
        cairo_set_source_rgb(cr, WM_COLOR_TITLE_ACTIVE);
    } else {
        cairo_set_source_rgb(cr, WM_COLOR_TITLE_IDLE);
    }

    const double inset = 4.5;

    cairo_set_line_width(cr, 1.4);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    cairo_move_to(cr, c.x + inset, c.y + inset);
    cairo_line_to(cr, c.x + c.width - inset, c.y + c.height - inset);

    cairo_move_to(cr, c.x + c.width - inset, c.y + inset);
    cairo_line_to(cr, c.x + inset, c.y + c.height - inset);

    cairo_stroke(cr);

    cairo_restore(cr);
}


void wm_window_paint(cairo_t* cr, wm_window_t* win) {

    const wm_rect_t f  = wm_window_frame(win);
    const bool focused = (wm.focused == win);


    wm_window_paint_shadow(cr, &f, focused);


    cairo_save(cr);

    /* Confine the window to its rounded outline. The content area is inset by less than
       the corner radius, so this is also what gives the client's bottom corners the same
       curve without the client knowing anything about it. */
    wm_rounded_rect(cr, f.x, f.y, f.width, f.height, WM_CORNER_RADIUS);
    cairo_clip(cr);

    /* OVER rather than SOURCE from here on. The clip edge is antialiased, and SOURCE
       writes partial coverage straight into an opaque surface, which would fringe every
       curve with black instead of blending it into the desktop. */
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);


    /* One flat colour for the titlebar and the border under it: the frame is a single
       shape, and shading it would only compete with the shadow for the reader's eye. */
    if (focused) {
        cairo_set_source_rgb(cr, WM_COLOR_FRAME_ACTIVE);
    } else {
        cairo_set_source_rgb(cr, WM_COLOR_FRAME_IDLE);
    }

    cairo_rectangle(cr, f.x, f.y, f.width, f.height);
    cairo_fill(cr);


    wm_window_paint_title(cr, win, &f, focused);
    wm_window_paint_close(cr, win, focused);


    /* Mid-drag the surface can be smaller than the content area it has to fill, so lay
       down a floor first: painting the surface alone would leave whatever the compositor
       drew there last frame showing through the uncovered strip. */
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_rectangle(cr, win->x, win->y, win->width, win->height);
    cairo_fill(cr);

    const int bw = cairo_image_surface_get_width(win->backstore);
    const int bh = cairo_image_surface_get_height(win->backstore);

    cairo_set_source_surface(cr, win->backstore, win->x, win->y);
    cairo_rectangle(cr, win->x, win->y, bw < win->width ? bw : win->width, bh < win->height ? bh : win->height);
    cairo_fill(cr);

    cairo_restore(cr);


    /* A hairline just inside the outline, drawn last so the content cannot cover it. With
       the frames this dark it is doing most of the work of telling two stacked windows
       apart, and the accent on the active one is the clearest focus cue on screen. */
    cairo_save(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_line_width(cr, 1.0);

    if (focused) {
        cairo_set_source_rgba(cr, WM_COLOR_RING_ACTIVE);
    } else {
        cairo_set_source_rgba(cr, WM_COLOR_RING_IDLE);
    }

    wm_rounded_rect(cr, f.x + 0.5, f.y + 0.5, f.width - 1.0, f.height - 1.0, WM_CORNER_RADIUS - 0.5);
    cairo_stroke(cr);

    cairo_restore(cr);
}
