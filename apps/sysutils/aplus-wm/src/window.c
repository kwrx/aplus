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
#include <sys/ipc.h>
#include <sys/shm.h>

#include <aplus/ulog.h>

#include <wm.h>


/**
 * @brief Reports whether the server draws nothing at all around a window.
 *
 * @param win The window to test.
 * @return true when the client owns every pixel of the frame.
 */
bool wm_window_borderless(const wm_window_t* win) {

    return (win->flags & (UI_WINDOW_BORDERLESS | UI_WINDOW_DRAWER)) != 0;
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
 * @brief Reports whether a window slides out of the rail as a drawer.
 *
 * @param win The window to test.
 * @return true when it asked to be one.
 */
bool wm_window_drawer(const wm_window_t* win) {

    return (win->flags & UI_WINDOW_DRAWER) != 0;
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
 * @brief Reports how far a window's corners are rounded off, which is not at all once it fills the display.
 *
 * @param win The window.
 * @return The radius in pixels.
 */
int wm_window_radius(const wm_window_t* win) {

    return win->maximized ? 0 : UI_WINDOW_RADIUS;
}


/**
 * @brief Reports everything the shadow can reach, which is repainted with the window but never clickable.
 *
 * @param win The window to measure.
 * @return The rectangle, which is the frame alone for a window that casts no shadow.
 */
wm_rect_t wm_window_shadow_rect(const wm_window_t* win) {

    const wm_rect_t f = wm_window_frame(win);

    if (win->maximized) {
        return f;
    }


    wm_rect_t r = {

        .x      = f.x - WM_SHADOW_KEY_BLUR,
        .y      = f.y - WM_SHADOW_KEY_BLUR,
        .width  = f.width + 2 * WM_SHADOW_KEY_BLUR,
        .height = f.height + 2 * WM_SHADOW_KEY_BLUR + WM_SHADOW_KEY_OFFSET,
    };

    return r;
}


/**
 * @brief Reports where a titlebar button sits, counting leftwards from the close button at the right end.
 *
 * @param win The window.
 * @param region WM_REGION_CLOSE, WM_REGION_MAXIMIZE or WM_REGION_MINIMIZE.
 * @return The button's square, empty for a window without a titlebar.
 */
wm_rect_t wm_window_button_rect(const wm_window_t* win, wm_region_t region) {

    if (wm_window_borderless(win)) {

        const wm_rect_t none = {win->x, win->y, 0, 0};

        return none;
    }


    const wm_rect_t f = wm_window_frame(win);

    const int slot = region == WM_REGION_CLOSE ? 0 : region == WM_REGION_MAXIMIZE ? 1 : 2;

    wm_rect_t r = {

        .x      = f.x + f.width - WM_BUTTON_MARGIN - WM_BUTTON_SIZE - slot * (WM_BUTTON_SIZE + WM_BUTTON_GAP),
        .y      = f.y + (WM_TITLEBAR_HEIGHT - WM_BUTTON_SIZE) / 2,
        .width  = WM_BUTTON_SIZE,
        .height = WM_BUTTON_SIZE,
    };

    return r;
}


wm_rect_t wm_window_close_rect(const wm_window_t* win) {
    return wm_window_button_rect(win, WM_REGION_CLOSE);
}


wm_rect_t wm_window_maximize_rect(const wm_window_t* win) {
    return wm_window_button_rect(win, WM_REGION_MAXIMIZE);
}


/**
 * @brief Reports the transition that lights a titlebar button while the pointer is on it.
 *
 * @param win The window.
 * @param region The button.
 * @return The transition.
 */
ui_anim_t* wm_window_button_hover(wm_window_t* win, wm_region_t region) {

    switch (region) {

        case WM_REGION_CLOSE:
            return &win->hover_close;

        case WM_REGION_MAXIMIZE:
            return &win->hover_maximize;

        default:
            return &win->hover_minimize;
    }
}


/**
 * @brief Reports whether a window paints every pixel of its content area solid right now.
 *
 * @param win The window.
 * @return false for a translucent window, one not yet drawn, one minimised, and one still fading, morphing or minimising.
 */
bool wm_window_opaque(const wm_window_t* win) {

    if (!win->committed || wm_window_translucent(win) || win->morph || win->minimized) {
        return false;
    }

    if (ui_anim_value(&win->minimize, wm.frame_time) > 0.0) {
        return false;
    }

    return ui_anim_value(&win->appear, wm.frame_time) >= 1.0;
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
 * @brief Reports the frontmost window that is not minimised, which is where the focus falls back to.
 *
 * @return The window, or NULL when there is none.
 */
wm_window_t* wm_window_topmost(void) {

    for (wm_window_t* w = wm.windows; w; w = w->next) {

        if (!w->minimized) {
            return w;
        }
    }

    return NULL;
}


/**
 * @brief Reports whether a window has an entry on the taskbar: one that has drawn and has a titlebar.
 *
 * @param win The window.
 * @return true when it is listed.
 */
bool wm_window_listed(const wm_window_t* win) {

    return win->committed && !wm_window_borderless(win);
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
    const wm_rect_t work = wm_workarea();

    const int max_width  = work.width - in.left - in.right;
    const int max_height = work.height - in.top - in.bottom;

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
/**
 * @brief Reports whether a window already sits at a spot, which a new one cascades away from.
 *
 * @param x The left edge of a content area.
 * @param y The top edge of a content area.
 * @return true when an open window's content area starts within a few pixels of it.
 */
static bool wm_window_spot_taken(int x, int y) {

    for (wm_window_t* w = wm.windows; w; w = w->next) {

        if (abs(w->x - x) < WM_CASCADE_STEP / 2 && abs(w->y - y) < WM_CASCADE_STEP / 2) {
            return true;
        }
    }

    return false;
}


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
    const wm_rect_t work = wm_workarea();

    win->x = work.x + in.left + (work.width - win->width - in.left - in.right) / 2;
    win->y = work.y + in.top + (work.height - win->height - in.top - in.bottom) / 2;

    if (!wm_window_centered(win)) {

        for (int step = 0; step < 8 && wm_window_spot_taken(win->x, win->y); step++) {

            win->x += WM_CASCADE_STEP;
            win->y += WM_CASCADE_STEP;
        }
    }

    if (win->x + win->width + in.right > work.x + work.width) {
        win->x = work.x + work.width - win->width - in.right;
    }

    if (win->y + win->height + in.bottom > work.y + work.height) {
        win->y = work.y + work.height - win->height - in.bottom;
    }

    if (wm_window_drawer(win)) {
        wm_drawer_attach(win);
    }

    wm_window_set_icon(win, NULL);


    win->next  = wm.windows;
    wm.windows = win;

    wm_damage_window(win);

    return win;
}


/**
 * @brief Gives a window the icon its titlebar shows, scaled here once so that painting it is a plain copy.
 *
 * @param win The window.
 * @param name A name in the server's icon theme; a path, a name the theme lacks, or NULL gets WM_ICON_FALLBACK.
 */
void wm_window_set_icon(wm_window_t* win, const char* name) {

    cairo_surface_t* icon = wm_icon_create(name, WM_ICON_SIZE);

    if (!icon) {
        return;
    }

    if (win->icon) {
        cairo_surface_destroy(win->icon);
    }

    win->icon = icon;

    wm_damage_window(win);
    wm_taskbar_damage();
}


/**
 * @brief The shape the pointer takes over a content area, by what the client asked for.
 */
static const wm_cursor_shape_t wm_window_cursors[UI_CURSOR_COUNT] = {

    [UI_CURSOR_DEFAULT] = WM_CURSOR_ARROW,
    [UI_CURSOR_TEXT]    = WM_CURSOR_TEXT,
};


/**
 * @brief Gives a window the pointer shape its content area shows, at once if the pointer is over it.
 *
 * @param win The window.
 * @param cursor The shape the client asked for; one this server does not know is the arrow.
 */
void wm_window_set_cursor(wm_window_t* win, ui_cursor_t cursor) {

    const wm_cursor_shape_t shape = (unsigned)cursor < UI_CURSOR_COUNT ? wm_window_cursors[cursor] : WM_CURSOR_ARROW;

    if (win->cursor == shape) {
        return;
    }

    win->cursor = shape;

    wm_input_refresh_cursor();
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


    wm_drawer_detach(win);
    wm_ghost_create(win);

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

    if (wm.hover.window == win) {
        wm.hover.window = NULL;
        wm.hover.region = WM_REGION_NONE;
    }

    if (wm.pointer_focus == win) {
        wm.pointer_focus = NULL;
        wm_input_refresh_cursor();
    }

    if (wm.focused == win) {
        wm.focused = NULL;
        wm_window_focus(wm_window_topmost());
    }

    wm_taskbar_forget(win);


    wm_window_free_backstore(win);

    if (win->morph) {
        cairo_surface_destroy(win->morph);
        win->morph = NULL;
    }

    if (win->icon) {
        cairo_surface_destroy(win->icon);
        win->icon = NULL;
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

    const wm_rect_t m = wm_window_maximize_rect(win);

    if (wm_rect_contains_point(&m, x, y)) {
        return WM_REGION_MAXIMIZE;
    }

    const wm_rect_t n = wm_window_button_rect(win, WM_REGION_MINIMIZE);

    if (wm_rect_contains_point(&n, x, y)) {
        return WM_REGION_MINIMIZE;
    }

    if (win->maximized) {
        return WM_REGION_TITLEBAR;
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

    wm_window_t* drawer = wm_drawer_at(x, y);

    if (drawer) {

        if (out) {
            *out = drawer;
        }

        return WM_REGION_CONTENT;
    }


    if (wm_taskbar_contains(x, y)) {

        if (out) {
            *out = NULL;
        }

        return WM_REGION_TASKBAR;
    }


    for (wm_window_t* win = wm.windows; win; win = win->next) {

        if (win->minimized || wm_window_drawer(win)) {
            continue;
        }

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

    const uint64_t now = ui_anim_now();


    if (previous) {

        ui_msg_focus_t msg = {.window_id = previous->id, .focused = 0};

        wm_client_queue(previous->client, UI_EV_FOCUS, &msg, sizeof(msg));
        wm_damage_window(previous);

        ui_anim_to(&previous->focus, 0.0, UI_MOTION_NORMAL, now);
    }

    if (win) {

        ui_msg_focus_t msg = {.window_id = win->id, .focused = 1};

        wm_client_queue(win->client, UI_EV_FOCUS, &msg, sizeof(msg));
        wm_damage_window(win);

        if (win->committed) {
            ui_anim_to(&win->focus, 1.0, UI_MOTION_NORMAL, now);
        } else {
            ui_anim_set(&win->focus, 1.0);
        }
    }

    wm_taskbar_damage();
}


void wm_window_move(wm_window_t* win, int x, int y) {

    if (wm_window_drawer(win)) {
        return;
    }


    const wm_insets_t in = wm_window_insets(win);
    const wm_rect_t work = wm_workarea();

    const int min_x = work.x - (win->width - WM_WINDOW_MIN_WIDTH);
    const int max_x = work.x + work.width - WM_WINDOW_MIN_WIDTH;

    const int min_y = work.y + in.top;
    const int max_y = work.y + work.height - WM_WINDOW_MIN_HEIGHT + in.top;

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

    if (wm_window_drawer(win)) {
        wm_drawer_resized(win);
    }

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

            ulog_warnf("cannot allocate a %dx%d backstore, keeping the window at %dx%d", win->width, win->height, cairo_image_surface_get_width(win->backstore), cairo_image_surface_get_height(win->backstore));

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

        ui_anim_set(&win->appear, 0.0);
        ui_anim_to(&win->appear, 1.0, UI_MOTION_SLOW, ui_anim_now());

        if (wm_window_drawer(win)) {
            wm_drawer_reveal(win);
        }

        wm_damage_window(win);
        wm_taskbar_damage();
    }

    return 0;
}


/**
 * @brief Starts the morph from where a frame was to where it is now, out of a picture taken before it moved.
 *
 * @param win The window that changed shape.
 * @param snapshot The picture, which the window takes over, or NULL for no morph.
 * @param snapshot_rect Where the picture was on screen.
 * @param from The frame the picture shows.
 */
static void wm_window_begin_morph(wm_window_t* win, cairo_surface_t* snapshot, const wm_rect_t* snapshot_rect, const wm_rect_t* from) {

    if (!snapshot) {
        return;
    }

    if (win->morph) {
        cairo_surface_destroy(win->morph);
    }

    win->morph      = snapshot;
    win->morph_rect = *snapshot_rect;
    win->morph_from = *from;

    ui_anim_set(&win->morph_anim, 0.0);

    win->morph_anim.ease = UI_EASE_IN_OUT;

    ui_anim_to(&win->morph_anim, 1.0, UI_MOTION_SLOW, ui_anim_now());
}


/**
 * @brief Fills the display with a window, or puts it back where it was before it did.
 *
 * @param win The window, which must have a titlebar to be maximised at all.
 */
void wm_window_toggle_maximize(wm_window_t* win) {

    if (!win || wm_window_borderless(win)) {
        return;
    }


    const wm_rect_t before = wm_window_frame(win);

    wm_rect_t snapshot_rect;

    cairo_surface_t* snapshot = win->committed ? wm_window_snapshot(win, &snapshot_rect) : NULL;


    wm_damage_window(win);

    if (win->maximized) {

        win->maximized = false;

        win->x = win->restore.x;
        win->y = win->restore.y;

        wm_window_resize(win, win->restore.width, win->restore.height);

    } else {

        const wm_insets_t in = wm_window_insets(win);
        const wm_rect_t work = wm_workarea();

        win->restore.x      = win->x;
        win->restore.y      = win->y;
        win->restore.width  = win->width;
        win->restore.height = win->height;

        win->maximized = true;

        win->x = work.x + in.left;
        win->y = work.y + in.top;

        wm_window_resize(win, work.width - in.left - in.right, work.height - in.top - in.bottom);
    }

    wm_damage_window(win);

    wm_window_notify_configure(win);

    wm_window_begin_morph(win, snapshot, &snapshot_rect, &before);
}


/**
 * @brief Lets go of the display when a maximised window is dragged, keeping the titlebar under the pointer.
 *
 * @param win The maximised window.
 * @param pointer_x Where the titlebar was grabbed.
 * @param pointer_y Where the titlebar was grabbed.
 */
void wm_window_unmaximize_at(wm_window_t* win, int pointer_x, int pointer_y) {

    if (!win->maximized) {
        return;
    }


    const wm_rect_t f = wm_window_frame(win);

    const double fraction = f.width > 0 ? (double)(pointer_x - f.x) / (double)f.width : 0.5;
    const int grab_y      = pointer_y - f.y;

    wm_damage_window(win);

    if (win->morph) {

        cairo_surface_destroy(win->morph);

        win->morph = NULL;

        ui_anim_set(&win->morph_anim, 1.0);
    }

    win->maximized = false;

    wm_window_resize(win, win->restore.width, win->restore.height);


    const wm_insets_t in = wm_window_insets(win);

    const int frame_width = win->width + in.left + in.right;

    win->x = pointer_x - (int)(fraction * frame_width) + in.left;
    win->y = pointer_y - grab_y + in.top;

    wm_damage_window(win);

    wm_window_notify_configure(win);
}


/**
 * @brief Hides a window in its taskbar entry, handing the focus to the frontmost one left.
 *
 * @param win The window, which must be listed on the taskbar to be minimised at all.
 */
void wm_window_minimize(wm_window_t* win) {

    if (!win || win->minimized || !wm_window_listed(win)) {
        return;
    }


    wm_damage_window(win);

    if (win->morph) {

        cairo_surface_destroy(win->morph);

        win->morph = NULL;

        ui_anim_set(&win->morph_anim, 1.0);
    }

    win->minimized = true;

    win->minimize.ease = UI_EASE_IN_OUT;

    ui_anim_to(&win->minimize, 1.0, UI_MOTION_SLOW, ui_anim_now());


    if (wm.drag.window == win) {
        wm.drag.window = NULL;
        wm.drag.region = WM_REGION_NONE;
    }

    if (wm.hover.window == win) {

        ui_anim_set(wm_window_button_hover(win, wm.hover.region), 0.0);

        wm.hover.window = NULL;
        wm.hover.region = WM_REGION_NONE;
    }

    if (wm.pointer_focus == win) {

        ui_msg_window_t msg = {.window_id = win->id};

        wm_client_queue(win->client, UI_EV_LEAVE, &msg, sizeof(msg));

        wm.pointer_focus = NULL;

        wm_input_refresh_cursor();
    }

    if (wm.focused == win) {
        wm_window_focus(wm_window_topmost());
    }

    wm_taskbar_damage();
}


/**
 * @brief Brings a minimised window back out of its taskbar entry, in front and focused.
 *
 * @param win The window.
 */
void wm_window_restore(wm_window_t* win) {

    if (!win || !win->minimized) {
        return;
    }


    win->minimized = false;

    win->minimize.ease = UI_EASE_IN_OUT;

    ui_anim_to(&win->minimize, 0.0, UI_MOTION_SLOW, ui_anim_now());

    wm_window_raise(win);
    wm_window_focus(win);

    wm_damage_window(win);
    wm_taskbar_damage();
}


/**
 * @brief Draws the title centred on the frame, pushed aside by the buttons when it is too long.
 *
 * @param cr The cairo context.
 * @param win The window.
 * @param f The window's frame.
 * @param focus How focused the window is, 0 to 1.
 */
static void wm_window_paint_title(cairo_t* cr, const wm_window_t* win, const wm_rect_t* f, double focus) {

    cairo_font_face_t* face = ui_font_face(wm.theme.font);

    if (!face || !win->title[0]) {
        return;
    }


    const wm_rect_t buttons = wm_window_button_rect(win, WM_REGION_MINIMIZE);

    const double right = buttons.x - WM_BUTTON_GAP;
    const double left  = f->x + (f->x + f->width - right);

    if (right - left < 8.0) {
        return;
    }


    cairo_save(cr);

    cairo_set_font_face(cr, face);
    cairo_set_font_size(cr, wm.theme.font_size);

    cairo_rectangle(cr, left, f->y, right - left, WM_TITLEBAR_HEIGHT);
    cairo_clip(cr);


    cairo_text_extents_t extents;
    cairo_font_extents_t font;

    cairo_text_extents(cr, win->title, &extents);
    cairo_font_extents(cr, &font);

    double x = f->x + (f->width - extents.x_advance) / 2.0;

    if (x + extents.x_advance > right) {
        x = right - extents.x_advance;
    }

    if (x < left) {
        x = left;
    }

    const double y = f->y + (WM_TITLEBAR_HEIGHT - (font.ascent + font.descent)) / 2.0 + font.ascent;


    wm_source_mix(cr, wm.theme.title_idle, wm.theme.title_active, focus);

    cairo_move_to(cr, round(x), round(y));
    cairo_show_text(cr, win->title);

    cairo_restore(cr);
}


/**
 * @brief Draws the icon at the left end of the titlebar, in the slot that mirrors the close button.
 *
 * @param cr The cairo context.
 * @param win The window.
 * @param f The window's frame.
 * @param focus How focused the window is, 0 to 1.
 */
static void wm_window_paint_icon(cairo_t* cr, const wm_window_t* win, const wm_rect_t* f, double focus) {

    if (!win->icon) {
        return;
    }


    const int x = f->x + WM_BUTTON_MARGIN + (WM_BUTTON_SIZE - WM_ICON_SIZE) / 2;
    const int y = f->y + (WM_TITLEBAR_HEIGHT - WM_ICON_SIZE) / 2;

    cairo_save(cr);

    cairo_rectangle(cr, x, y, WM_ICON_SIZE, WM_ICON_SIZE);
    cairo_clip(cr);

    cairo_set_source_surface(cr, win->icon, x, y);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
    cairo_paint_with_alpha(cr, ui_lerp(WM_ICON_IDLE, 1.0, focus));

    cairo_restore(cr);
}


/**
 * @brief Draws one round titlebar button: a faint disc that lights up under the pointer, and its glyph.
 *
 * @param cr The cairo context.
 * @param win The window the button belongs to.
 * @param region WM_REGION_CLOSE, WM_REGION_MAXIMIZE or WM_REGION_MINIMIZE.
 * @param focus How focused the window is, 0 to 1.
 */
static void wm_window_paint_button(cairo_t* cr, wm_window_t* win, wm_region_t region, double focus) {

    const wm_rect_t r = wm_window_button_rect(win, region);

    const bool close   = region == WM_REGION_CLOSE;
    const double hover = ui_anim_value(wm_window_button_hover(win, region), wm.frame_time);
    const bool pressed = wm.hover.window == win && wm.hover.region == region && wm.drag.window == win && wm.drag.region == region;

    const double cx = r.x + r.width / 2.0;
    const double cy = r.y + r.height / 2.0;


    cairo_save(cr);

    cairo_new_path(cr);
    cairo_arc(cr, cx, cy, r.width / 2.0, 0.0, 2.0 * M_PI);

    ui_color_t idle = wm.theme.button;

    idle.a *= ui_lerp(0.5, 1.0, focus);

    if (close) {

        if (pressed) {
            wm_source(cr, wm.theme.close_down, 1.0);
        } else {
            wm_source_mix(cr, idle, wm.theme.close_over, hover);
        }

    } else {

        if (pressed) {
            wm_source(cr, wm.theme.button_down, 1.0);
        } else {
            wm_source_mix(cr, idle, wm.theme.button_over, hover);
        }
    }

    cairo_fill(cr);


    ui_color_t glyph = {

        .r = ui_lerp(wm.theme.title_idle.r, wm.theme.title_active.r, focus),
        .g = ui_lerp(wm.theme.title_idle.g, wm.theme.title_active.g, focus),
        .b = ui_lerp(wm.theme.title_idle.b, wm.theme.title_active.b, focus),
        .a = 1.0,
    };

    wm_source_mix(cr, glyph, (ui_color_t){1.0, 1.0, 1.0, 1.0}, close ? hover : 0.0);

    cairo_set_line_width(cr, 1.5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    const double h = WM_BUTTON_GLYPH / 2.0;

    if (close) {

        cairo_move_to(cr, cx - h, cy - h);
        cairo_line_to(cr, cx + h, cy + h);

        cairo_move_to(cr, cx + h, cy - h);
        cairo_line_to(cr, cx - h, cy + h);

    } else if (region == WM_REGION_MINIMIZE) {

        cairo_move_to(cr, cx - h, cy);
        cairo_line_to(cr, cx + h, cy);

    } else if (win->maximized) {

        ui_draw_rounded_rect_d(cr, cx - h, cy - h + 2.0, 2.0 * h - 2.0, 2.0 * h - 2.0, 1.5);

        cairo_move_to(cr, cx - h + 2.0, cy - h);
        cairo_line_to(cr, cx + h, cy - h);
        cairo_line_to(cr, cx + h, cy + h - 2.0);

    } else {

        ui_draw_rounded_rect_d(cr, cx - h, cy - h, 2.0 * h, 2.0 * h, 1.5);
    }

    cairo_stroke(cr);

    cairo_restore(cr);
}


/**
 * @brief Draws a window as it stands, with no transition applied: shadow, frame, content and outline.
 *
 * @param cr The cairo context to draw with.
 * @param win The window to draw.
 */
void wm_window_paint_still(cairo_t* cr, wm_window_t* win) {

    if (!win->committed) {
        return;
    }


    const wm_rect_t f      = wm_window_frame(win);
    const bool borderless  = wm_window_borderless(win);
    const bool translucent = wm_window_translucent(win);
    const double radius    = (double)wm_window_radius(win);
    const double focus     = ui_anim_value(&win->focus, wm.frame_time);


    if (!win->maximized) {

        const double strength = ui_lerp(WM_SHADOW_IDLE, 1.0, focus);

        cairo_save(cr);

        if (translucent) {

            const wm_rect_t extent = wm_window_shadow_rect(win);

            cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);

            cairo_rectangle(cr, extent.x, extent.y, extent.width, extent.height);
            ui_draw_rounded_rect_d(cr, f.x, f.y, f.width, f.height, radius);

            cairo_clip(cr);
        }

        wm_shadow_paint(cr, &wm.shadow_key, &f, WM_SHADOW_KEY_OFFSET, wm.theme.shadow_key * strength);
        wm_shadow_paint(cr, &wm.shadow_ambient, &f, 0, wm.theme.shadow_ambient * strength);

        cairo_restore(cr);
    }


    cairo_save(cr);

    ui_draw_rounded_rect_d(cr, f.x, f.y, f.width, f.height, radius);
    cairo_clip(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);


    const int bw = cairo_image_surface_get_width(win->backstore);
    const int bh = cairo_image_surface_get_height(win->backstore);

    const int cw = WM_MIN(bw, win->width);
    const int ch = WM_MIN(bh, win->height);


    if (!borderless) {

        wm_source(cr, wm_theme_frame(), 1.0);

        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);

        cairo_rectangle(cr, f.x, f.y, f.width, f.height);
        cairo_rectangle(cr, win->x, win->y, cw, ch);

        cairo_fill(cr);

        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);


        wm_window_paint_icon(cr, win, &f, focus);
        wm_window_paint_title(cr, win, &f, focus);
        wm_window_paint_button(cr, win, WM_REGION_MINIMIZE, focus);
        wm_window_paint_button(cr, win, WM_REGION_MAXIMIZE, focus);
        wm_window_paint_button(cr, win, WM_REGION_CLOSE, focus);
    }


    if ((cw < win->width || ch < win->height) && !translucent) {

        wm_source(cr, wm_theme_frame(), 1.0);

        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);

        cairo_rectangle(cr, win->x, win->y, win->width, win->height);
        cairo_rectangle(cr, win->x, win->y, cw, ch);

        cairo_fill(cr);

        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    }

    cairo_set_source_surface(cr, win->backstore, win->x, win->y);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
    cairo_rectangle(cr, win->x, win->y, cw, ch);
    cairo_fill(cr);

    cairo_restore(cr);


    if (borderless) {
        return;
    }


    cairo_save(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_line_width(cr, 1.0);

    if (!win->maximized) {

        wm_source(cr, wm.theme.outline, 1.0);

        ui_draw_rounded_rect_d(cr, f.x + 0.5, f.y + 0.5, f.width - 1.0, f.height - 1.0, radius - 0.5);
        cairo_stroke(cr);
    }

    wm_source_mix(cr, wm.theme.highlight_idle, wm.theme.highlight, focus);

    ui_draw_rounded_rect_d(cr, f.x + 1.5, f.y + 1.5, f.width - 3.0, f.height - 3.0, radius > 1.5 ? radius - 1.5 : 0.0);
    cairo_stroke(cr);

    cairo_restore(cr);
}


/**
 * @brief Maps one frame rectangle onto another in the current transformation.
 *
 * @param cr The cairo context.
 * @param from The rectangle as drawn.
 * @param to Where it has to land.
 */
static void wm_window_map(cairo_t* cr, const wm_rect_t* from, const wm_rect_t* to) {

    if (from->width <= 0 || from->height <= 0) {
        return;
    }

    cairo_translate(cr, to->x, to->y);
    cairo_scale(cr, (double)to->width / from->width, (double)to->height / from->height);
    cairo_translate(cr, -from->x, -from->y);
}


/**
 * @brief Draws a window changing shape: the old picture stretching towards the new frame, the live window growing out of it.
 *
 * Both go over a solid frame at the in-between size, so the desktop never shows through the crossfade.
 *
 * @param cr The cairo context.
 * @param win The window.
 */
static void wm_window_paint_morph(cairo_t* cr, wm_window_t* win) {

    const double t = ui_anim_value(&win->morph_anim, wm.frame_time);

    const wm_rect_t to   = wm_window_frame(win);
    const wm_rect_t from = win->morph_from;

    const wm_rect_t now = {

        .x      = (int)lround(ui_lerp(from.x, to.x, t)),
        .y      = (int)lround(ui_lerp(from.y, to.y, t)),
        .width  = (int)lround(ui_lerp(from.width, to.width, t)),
        .height = (int)lround(ui_lerp(from.height, to.height, t)),
    };

    const double radius = ui_lerp(win->maximized ? UI_WINDOW_RADIUS : 0.0, wm_window_radius(win), t);


    cairo_save(cr);

    wm_source(cr, wm_theme_frame(), 1.0);

    ui_draw_rounded_rect_d(cr, now.x, now.y, now.width, now.height, radius);
    cairo_fill(cr);

    wm_window_map(cr, &from, &now);

    cairo_set_source_surface(cr, win->morph, win->morph_rect.x, win->morph_rect.y);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
    cairo_paint_with_alpha(cr, 1.0 - t);

    cairo_restore(cr);


    cairo_save(cr);

    cairo_push_group(cr);

    wm_window_map(cr, &to, &now);
    wm_window_paint_still(cr, win);

    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, t);

    cairo_restore(cr);
}


/**
 * @brief Draws a window on its way into its taskbar entry or back out, shrinking towards it and fading.
 *
 * @param cr The cairo context.
 * @param win The window.
 * @param t How far it has gone, 0 where it stands to 1 inside the entry.
 */
static void wm_window_paint_minimizing(cairo_t* cr, wm_window_t* win, double t) {

    const wm_rect_t f  = wm_window_frame(win);
    const wm_rect_t to = wm_taskbar_entry_rect(win);

    if (f.width <= 0 || f.height <= 0) {
        return;
    }


    const double end   = WM_MIN((double)to.width / f.width, (double)to.height / f.height);
    const double scale = ui_lerp(1.0, end, t);

    const double fx = f.x + f.width / 2.0;
    const double fy = f.y + f.height / 2.0;

    const double cx = ui_lerp(fx, to.x + to.width / 2.0, t);
    const double cy = ui_lerp(fy, to.y + to.height / 2.0, t);


    cairo_save(cr);

    cairo_push_group(cr);

    cairo_translate(cr, cx, cy);
    cairo_scale(cr, scale, scale);
    cairo_translate(cr, -fx, -fy);

    wm_window_paint_still(cr, win);

    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, 1.0 - t);

    cairo_restore(cr);
}


/**
 * @brief Draws a window as it is this frame: minimising, growing in, morphing, or still.
 *
 * @param cr The cairo context to draw with.
 * @param win The window to draw.
 */
void wm_window_paint(cairo_t* cr, wm_window_t* win) {

    if (!win->committed) {
        return;
    }


    const double gone = ui_anim_value(&win->minimize, wm.frame_time);

    if (gone >= 1.0) {
        return;
    }

    if (gone > 0.0) {
        wm_window_paint_minimizing(cr, win, gone);
        return;
    }

    if (win->morph) {
        wm_window_paint_morph(cr, win);
        return;
    }


    const double shown = ui_anim_value(&win->appear, wm.frame_time);

    if (shown >= 1.0) {
        wm_window_paint_still(cr, win);
        return;
    }

    if (shown <= 0.0) {
        return;
    }


    const wm_rect_t f = wm_window_frame(win);

    const double scale = ui_lerp(WM_OPEN_SCALE, 1.0, shown);
    const double cx    = f.x + f.width / 2.0;
    const double cy    = f.y + f.height / 2.0;


    cairo_save(cr);

    cairo_push_group(cr);

    cairo_translate(cr, cx, cy);
    cairo_scale(cr, scale, scale);
    cairo_translate(cr, -cx, -cy);

    wm_window_paint_still(cr, win);

    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, shown);

    cairo_restore(cr);
}
