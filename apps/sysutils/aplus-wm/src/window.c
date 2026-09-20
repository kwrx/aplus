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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <wm.h>


/**
 * @brief Reports whether the server draws nothing at all around a window.
 *
 * @param win The window to test.
 * @return true when the client owns every pixel of the frame.
 */
bool wm_window_borderless(const wm_window_t* win) {

    return (win->flags & UI_WINDOW_BORDERLESS) != 0;
}


/**
 * @brief Reports whether a window asked to come up in the middle of the display.
 *
 * @param win The window to test.
 * @return true when it is placed rather than cascaded.
 */
bool wm_window_centered(const wm_window_t* win) {

    return (win->flags & UI_WINDOW_CENTERED) != 0;
}


/**
 * @brief Reports whether a window's surface carries alpha and is blended rather than copied.
 *
 * @param win The window to test.
 * @return true when it is translucent.
 */
bool wm_window_translucent(const wm_window_t* win) {

    return (win->flags & UI_WINDOW_TRANSLUCENT) != 0;
}


/**
 * @brief Reports how far the decorations reach out from a window's content area.
 *
 * @param win The window to measure.
 * @return The insets, every one of them zero when the window is borderless.
 */
wm_insets_t wm_window_insets(const wm_window_t* win) {

    if (wm_window_borderless(win)) {

        const wm_insets_t none = {0, 0, 0, 0};

        return none;
    }


    const wm_insets_t decorated = {

        .left   = WM_BORDER_WIDTH,
        .top    = WM_TITLEBAR_HEIGHT,
        .right  = WM_BORDER_WIDTH,
        .bottom = WM_BORDER_WIDTH,
    };

    return decorated;
}


/**
 * @brief Reports the cairo format the window's surface is read and written as.
 *
 * @param win The window.
 * @return CAIRO_FORMAT_ARGB32 for a translucent window, CAIRO_FORMAT_RGB24 otherwise.
 */
cairo_format_t wm_window_format(const wm_window_t* win) {

    return wm_window_translucent(win) ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
}


wm_rect_t wm_window_frame(const wm_window_t* win) {

    const wm_insets_t in = wm_window_insets(win);

    wm_rect_t r = {

        .x      = win->x - in.left,
        .y      = win->y - in.top,
        .width  = win->width + in.left + in.right,
        .height = win->height + in.top + in.bottom,
    };

    return r;
}


/**
 * @brief Reports everything the shadow can reach, which is repainted with the window but never clickable.
 *
 * @param win The window to measure.
 * @return The rectangle.
 */
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

    if (wm_window_borderless(win)) {

        const wm_rect_t none = {win->x, win->y, 0, 0};

        return none;
    }


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


/**
 * @brief Lets go of the window's surface and removes the segment behind it, which the client may still hold.
 *
 * @param win The window to detach.
 */
static void wm_window_free_backstore(wm_window_t* win) {

    if (win->backstore) {
        cairo_surface_destroy(win->backstore);
        win->backstore = NULL;
    }

    if (win->shm_addr) {
        shmdt(win->shm_addr);
        win->shm_addr = NULL;
    }

    if (win->shm_id >= 0) {
        shmctl(win->shm_id, IPC_RMID, NULL);
        win->shm_id = -1;
    }

    win->shm_size = 0;
    win->stride   = 0;
}


/**
 * @brief Allocates the pixel store, a shared memory segment both ends map, carrying the old contents over.
 *
 * @param win The window to allocate for.
 * @param width The width in pixels.
 * @param height The height in pixels.
 * @return 0 on success, or -1 with errno set.
 */
static int wm_window_alloc_backstore(wm_window_t* win, int width, int height) {

    const cairo_format_t format = wm_window_format(win);

    const int stride = cairo_format_stride_for_width(format, width);

    if (stride <= 0) {
        return -1;
    }

    const size_t size = (size_t)stride * (size_t)height;


    int id = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);

    if (id < 0) {
        return -1;
    }


    void* addr = shmat(id, NULL, 0);

    if (addr == (void*)-1) {
        shmctl(id, IPC_RMID, NULL);
        return -1;
    }


    cairo_surface_t* surface = cairo_image_surface_create_for_data((unsigned char*)addr, format, width, height, stride);

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {

        cairo_surface_destroy(surface);

        shmdt(addr);
        shmctl(id, IPC_RMID, NULL);

        return -1;
    }


    if (win->backstore) {

        cairo_t* cr = cairo_create(surface);

        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface(cr, win->backstore, 0, 0);
        cairo_rectangle(cr, 0, 0, WM_MIN(win->width, width), WM_MIN(win->height, height));
        cairo_fill(cr);

        cairo_destroy(cr);
    }


    wm_window_free_backstore(win);

    win->backstore = surface;

    win->shm_addr = addr;
    win->shm_id   = id;
    win->shm_size = size;
    win->stride   = stride;

    return 0;
}


/**
 * @brief Clamps a window size to what the display can hold, and to the smallest window that can be grabbed.
 *
 * @param win The window being sized, whose decorations take up the rest of the display.
 * @param width In/out. The width asked for, replaced by the width allowed.
 * @param height In/out. The height asked for, replaced by the height allowed.
 */
void wm_window_clamp_size(const wm_window_t* win, int* width, int* height) {

    const wm_insets_t in = wm_window_insets(win);

    const int max_width  = wm.display.width - in.left - in.right;
    const int max_height = wm.display.height - in.top - in.bottom;

    *width  = WM_MAX(WM_MIN(*width, max_width), WM_WINDOW_MIN_WIDTH);
    *height = WM_MAX(WM_MIN(*height, max_height), WM_WINDOW_MIN_HEIGHT);
}


/**
 * @brief Creates a window and its first surface, which wm_window_notify_configure() hands to the client.
 *
 * @param client The client the window belongs to.
 * @param width The width in pixels.
 * @param height The height in pixels.
 * @param title The window title.
 * @param flags UI_WINDOW_*, which decide what the server draws around it and where it goes.
 * @return The window, or NULL with errno set.
 */
wm_window_t* wm_window_create(wm_client_t* client, int width, int height, const char* title, uint32_t flags) {

    wm_window_t* win = (wm_window_t*)calloc(1, sizeof(wm_window_t));

    if (!win) {
        return NULL;
    }

    win->id     = wm.next_window_id++;
    win->client = client;
    win->serial = 1;
    win->flags  = flags;

    win->shm_id = -1;

    if (title) {
        strncpy(win->title, title, UI_TITLE_MAX - 1);
    }


    wm_window_clamp_size(win, &width, &height);

    win->width  = width;
    win->height = height;

    if (wm_window_alloc_backstore(win, width, height) < 0) {
        free(win);
        return NULL;
    }


    const wm_insets_t in = wm_window_insets(win);

    static int cascade = 0;

    if (wm_window_centered(win)) {

        win->x = in.left + (wm.display.width - win->width - in.left - in.right) / 2;
        win->y = in.top + (wm.display.height - win->height - in.top - in.bottom) / 2;

    } else {

        win->x = in.left + 24 * (cascade % 8);
        win->y = in.top + 24 * (cascade % 8);

        cascade++;
    }

    if (win->x + win->width + in.right > wm.display.width) {
        win->x = wm.display.width - win->width - in.right;
    }

    if (win->y + win->height + in.bottom > wm.display.height) {
        win->y = wm.display.height - win->height - in.bottom;
    }


    win->next  = wm.windows;
    wm.windows = win;

    wm_damage_window(win);

    return win;
}


/**
 * @brief Asks a client to close a window, which it owns and may decline to.
 *
 * @param win The window to ask about.
 */
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

    if (wm.pointer_focus == win) {
        wm.pointer_focus = NULL;
    }

    if (wm.focused == win) {
        wm.focused = NULL;
        wm_window_focus(wm.windows);
    }


    wm_window_free_backstore(win);

    if (win->shadow) {
        cairo_surface_destroy(win->shadow);
        win->shadow = NULL;
    }

    free(win);
}


static wm_region_t wm_window_region(const wm_window_t* win, int x, int y) {

    const wm_rect_t f = wm_window_frame(win);

    if (!wm_rect_contains_point(&f, x, y)) {
        return WM_REGION_NONE;
    }


    const wm_rect_t content = {win->x, win->y, win->width, win->height};

    if (wm_rect_contains_point(&content, x, y)) {
        return WM_REGION_CONTENT;
    }


    const wm_rect_t c = wm_window_close_rect(win);

    if (wm_rect_contains_point(&c, x, y)) {
        return WM_REGION_CLOSE;
    }


    const bool left   = x < f.x + WM_BORDER_WIDTH;
    const bool right  = x >= f.x + f.width - WM_BORDER_WIDTH;
    const bool top    = y < f.y + WM_BORDER_WIDTH;
    const bool bottom = y >= f.y + f.height - WM_BORDER_WIDTH;

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

    const wm_insets_t in = wm_window_insets(win);

    const int min_x = -(win->width - WM_WINDOW_MIN_WIDTH);
    const int max_x = wm.display.width - WM_WINDOW_MIN_WIDTH;

    const int min_y = in.top;
    const int max_y = wm.display.height - WM_WINDOW_MIN_HEIGHT + in.top;

    x = WM_CLAMP(x, min_x, max_x);
    y = WM_CLAMP(y, min_y, max_y);

    if (x == win->x && y == win->y) {
        return;
    }


    wm_damage_window(win);

    win->x = x;
    win->y = y;

    wm_damage_window(win);
}


int wm_window_resize(wm_window_t* win, int width, int height) {

    wm_window_clamp_size(win, &width, &height);

    if (width == win->width && height == win->height) {
        return 0;
    }


    wm_damage_window(win);

    win->width  = width;
    win->height = height;

    win->serial++;

    wm_damage_window(win);

    return 0;
}


/**
 * @brief Tells a client the size it ended a resize drag at, allocating the surface for it.
 *
 * @param win The window that was resized.
 * @return 0 on success, or -1 with errno set.
 */
int wm_window_notify_configure(wm_window_t* win) {

    if (cairo_image_surface_get_width(win->backstore) != win->width || cairo_image_surface_get_height(win->backstore) != win->height) {

        if (wm_window_alloc_backstore(win, win->width, win->height) < 0) {

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
        .shm_id    = win->shm_id,
        .stride    = (uint32_t)win->stride,
        .shm_size  = (uint32_t)win->shm_size,
    };

    return wm_client_queue(win->client, UI_EV_CONFIGURE, &msg, sizeof(msg));
}


/**
 * @brief Takes a client's word that a rectangle of the shared surface has changed.
 *
 * The first one is also what puts the window on screen, since until it arrives the surface
 * holds nothing the client chose.
 *
 * @param win The window that committed.
 * @param x The left edge of the rectangle.
 * @param y The top edge of the rectangle.
 * @param width The width of the rectangle.
 * @param height The height of the rectangle.
 * @return 0 on success, or -1 when the rectangle does not fit the surface.
 */
int wm_window_damage_content(wm_window_t* win, int x, int y, int width, int height) {

    if (x < 0 || y < 0 || width <= 0 || height <= 0) {
        errno = EINVAL;
        return -1;
    }

    if (x + width > cairo_image_surface_get_width(win->backstore) || y + height > cairo_image_surface_get_height(win->backstore)) {
        errno = EINVAL;
        return -1;
    }


    cairo_surface_mark_dirty_rectangle(win->backstore, x, y, width, height);

    if (!win->committed) {

        win->committed = true;

        wm_damage_window(win);
    }

    return 0;
}


/**
 * @brief Draws the shadow layers around a frame, in whatever space the context is already in.
 *
 * @param cr The cairo context to draw with.
 * @param f The frame the shadow is cast around.
 * @param focused Whether the window is focused, which is what decides the strength.
 * @param radius How far the frame's corners are rounded off.
 */
static void wm_window_paint_shadow_direct(cairo_t* cr, const wm_rect_t* f, bool focused, double radius) {

    cairo_save(cr);

    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);

    cairo_rectangle(cr, f->x - WM_SHADOW_EXTENT, f->y - WM_SHADOW_EXTENT, f->width + 2 * WM_SHADOW_EXTENT, f->height + 2 * WM_SHADOW_EXTENT + WM_SHADOW_OFFSET);
    ui_draw_rounded_rect_d(cr, f->x, f->y, f->width, f->height, radius);

    cairo_clip(cr);


    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    const double strength = WM_SHADOW_ALPHA * (focused ? 1.0 : 0.55);

    for (int i = WM_SHADOW_LAYERS; i > 0; i--) {

        const double t      = (double)(i - 1) / (double)WM_SHADOW_LAYERS;
        const double spread = (double)WM_SHADOW_EXTENT * (double)i / (double)WM_SHADOW_LAYERS;

        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, strength * (1.0 - t) * (1.0 - t));

        ui_draw_rounded_rect_d(cr, f->x - spread, f->y - spread + WM_SHADOW_OFFSET, f->width + 2.0 * spread, f->height + 2.0 * spread, radius + spread);

        cairo_fill(cr);
    }

    cairo_restore(cr);
}


/**
 * @brief Reports the window's shadow mask, rendering it first when it does not match the frame.
 *
 * @param win The window whose shadow is wanted.
 * @param f The frame the shadow is cast around.
 * @param focused Whether the window is focused.
 * @return The A8 mask, with the frame at (WM_SHADOW_EXTENT, WM_SHADOW_EXTENT), or NULL.
 */
static cairo_surface_t* wm_window_shadow_mask(wm_window_t* win, const wm_rect_t* f, bool focused) {

    if (win->shadow && win->shadow_width == f->width && win->shadow_height == f->height && win->shadow_focused == focused) {
        return win->shadow;
    }


    if (win->shadow) {
        cairo_surface_destroy(win->shadow);
        win->shadow = NULL;
    }


    cairo_surface_t* mask = wm_surface_create(CAIRO_FORMAT_A8, f->width + 2 * WM_SHADOW_EXTENT, f->height + 2 * WM_SHADOW_EXTENT + WM_SHADOW_OFFSET);

    if (!mask) {
        return NULL;
    }


    cairo_t* cr = cairo_create(mask);

    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        cairo_destroy(cr);
        cairo_surface_destroy(mask);
        return NULL;
    }


    const wm_rect_t local = {WM_SHADOW_EXTENT, WM_SHADOW_EXTENT, f->width, f->height};

    wm_window_paint_shadow_direct(cr, &local, focused, (double)UI_WINDOW_RADIUS);

    cairo_destroy(cr);


    win->shadow         = mask;
    win->shadow_width   = f->width;
    win->shadow_height  = f->height;
    win->shadow_focused = focused;

    return mask;
}


/**
 * @brief Puts the window's shadow on the frame, through the cached mask where there is one.
 *
 * @param cr The cairo context to draw with.
 * @param win The window to draw the shadow of.
 * @param f The frame the shadow is cast around.
 * @param focused Whether the window is focused.
 */
static void wm_window_paint_shadow(cairo_t* cr, wm_window_t* win, const wm_rect_t* f, bool focused) {

    cairo_surface_t* mask = wm_window_shadow_mask(win, f, focused);

    if (!mask) {
        wm_window_paint_shadow_direct(cr, f, focused, (double)UI_WINDOW_RADIUS);
        return;
    }


    cairo_save(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

    cairo_mask_surface(cr, mask, f->x - WM_SHADOW_EXTENT, f->y - WM_SHADOW_EXTENT);

    cairo_restore(cr);
}


static void wm_window_paint_title(cairo_t* cr, const wm_window_t* win, const wm_rect_t* f, bool focused) {

    cairo_font_face_t* face = ui_font_face(WM_FONT_PATH);

    if (!face || !win->title[0]) {
        return;
    }


    const double left  = f->x + 8;
    const double right = wm_window_close_rect(win).x - 6;

    if (right - left < 8.0) {
        return;
    }


    cairo_save(cr);

    cairo_set_font_face(cr, face);
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

    if (hovered) {

        if (pressed) {
            cairo_set_source_rgb(cr, WM_COLOR_CLOSE_DOWN);
        } else {
            cairo_set_source_rgb(cr, WM_COLOR_CLOSE_OVER);
        }

        ui_draw_rounded_rect_d(cr, c.x, c.y, c.width, c.height, 4.0);
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

    if (!win->committed) {
        return;
    }


    const wm_rect_t f     = wm_window_frame(win);
    const bool focused    = (wm.focused == win);
    const bool borderless = wm_window_borderless(win);
    const double radius   = (double)UI_WINDOW_RADIUS;


    wm_window_paint_shadow(cr, win, &f, focused);


    cairo_save(cr);

    ui_draw_rounded_rect_d(cr, f.x, f.y, f.width, f.height, radius);
    cairo_clip(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);


    if (focused) {
        cairo_set_source_rgb(cr, WM_COLOR_FRAME_ACTIVE);
    } else {
        cairo_set_source_rgb(cr, WM_COLOR_FRAME_IDLE);
    }

    const int bw = cairo_image_surface_get_width(win->backstore);
    const int bh = cairo_image_surface_get_height(win->backstore);

    const int cw = WM_MIN(bw, win->width);
    const int ch = WM_MIN(bh, win->height);


    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);

    cairo_rectangle(cr, f.x, f.y, f.width, f.height);
    cairo_rectangle(cr, win->x, win->y, cw, ch);

    cairo_fill(cr);

    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);


    if (!borderless) {

        wm_window_paint_title(cr, win, &f, focused);
        wm_window_paint_close(cr, win, focused);
    }


    if ((cw < win->width || ch < win->height) && !wm_window_translucent(win)) {

        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);

        cairo_rectangle(cr, win->x, win->y, win->width, win->height);
        cairo_rectangle(cr, win->x, win->y, cw, ch);

        cairo_fill(cr);

        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    }

    cairo_set_source_surface(cr, win->backstore, win->x, win->y);
    cairo_rectangle(cr, win->x, win->y, cw, ch);
    cairo_fill(cr);

    cairo_restore(cr);


    if (borderless) {
        return;
    }


    cairo_save(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_line_width(cr, 1.0);

    if (focused) {
        cairo_set_source_rgba(cr, WM_COLOR_RING_ACTIVE);
    } else {
        cairo_set_source_rgba(cr, WM_COLOR_RING_IDLE);
    }

    ui_draw_rounded_rect_d(cr, f.x + 0.5, f.y + 0.5, f.width - 1.0, f.height - 1.0, radius - 0.5);
    cairo_stroke(cr);

    cairo_restore(cr);
}
