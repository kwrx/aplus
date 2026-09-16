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
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <aplus/events.h>
#include <aplus/input.h>

#include <wm.h>


/**
 * @brief One thread per input device reads it blocking and forwards each event down a socketpair.
 *
 * The socketpair is the only thing those threads share, and the only thing the main loop can poll.
 */

static int input_pipe[2] = {-1, -1};

/**
 * @brief Held for one whole record, so that what the main loop reads back is a sequence of events.
 */
static pthread_mutex_t input_pipe_lock = PTHREAD_MUTEX_INITIALIZER;

static struct {

    const char* path;
    bool required;

    int fd;
    pthread_t thread;
    bool started;

} input_devices[] = {

    {.path = "/dev/kbd", .required = true, .fd = -1},
    {.path = "/dev/mouse", .required = false, .fd = -1},
    {.path = "/dev/tablet", .required = false, .fd = -1},
};


/**
 * @brief Writes one whole event onto the pipe, looping over a short write.
 *
 * @param ev The event to forward.
 * @return 0 on success, or -1 with errno set.
 */
static int input_forward(const event_t* ev) {

    const uint8_t* data = (const uint8_t*)ev;

    size_t left = sizeof(*ev);

    int e = 0;


    pthread_mutex_lock(&input_pipe_lock);

    while (left > 0) {

        ssize_t n = write(input_pipe[1], data, left);

        if (n > 0) {

            data += n;
            left -= (size_t)n;

            continue;
        }

        if (n < 0 && errno == EINTR) {
            continue;
        }

        e = -1;

        break;
    }

    pthread_mutex_unlock(&input_pipe_lock);

    return e;
}


static void* input_thread(void* arg) {

    int fd = *(int*)arg;

    for (;;) {

        event_t ev;

        ssize_t e = read(fd, &ev, sizeof(ev));

        if (e == (ssize_t)sizeof(ev)) {

            if (input_forward(&ev) < 0) {
                break;
            }

            continue;
        }

        if (e < 0 && errno == EINTR) {
            continue;
        }

        break;
    }

    return NULL;
}


/**
 * @brief Opens every input device and starts a thread reading each one; only the keyboard is required.
 *
 * @return The read end of the merged pipe, for the main loop to poll, or -1 with errno set.
 */
int wm_input_open(void) {

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, input_pipe) < 0) {
        fprintf(stderr, "aplus-wm: socketpair() failed: %s\n", strerror(errno));
        return -1;
    }


    for (size_t i = 0; i < sizeof(input_devices) / sizeof(input_devices[0]); i++) {

        if ((input_devices[i].fd = open(input_devices[i].path, O_RDONLY)) < 0) {

            if (input_devices[i].required) {
                fprintf(stderr, "aplus-wm: open() failed: cannot open %s: %s\n", input_devices[i].path, strerror(errno));
                return -1;
            }

            continue;
        }

        if (pthread_create(&input_devices[i].thread, NULL, input_thread, &input_devices[i].fd) != 0) {

            fprintf(stderr, "aplus-wm: pthread_create() failed for %s: %s\n", input_devices[i].path, strerror(errno));

            if (input_devices[i].required) {
                return -1;
            }

            close(input_devices[i].fd);
            input_devices[i].fd = -1;

            continue;
        }

        input_devices[i].started = true;
    }


    return input_pipe[0];
}


void wm_input_close(void) {

    for (size_t i = 0; i < sizeof(input_devices) / sizeof(input_devices[0]); i++) {

        if (input_devices[i].started) {
            pthread_cancel(input_devices[i].thread);
        }

        if (input_devices[i].fd >= 0) {
            close(input_devices[i].fd);
        }
    }

    if (input_pipe[0] >= 0) {
        close(input_pipe[0]);
    }

    if (input_pipe[1] >= 0) {
        close(input_pipe[1]);
    }
}


static void input_send_key(uint16_t vkey, uint8_t down) {

    if (!wm.focused) {
        return;
    }


    ui_msg_key_t msg = {

        .window_id = wm.focused->id,
        .vkey      = vkey,
        .down      = down,
    };

    wm_client_queue(wm.focused->client, UI_EV_KEY, &msg, sizeof(msg));
}


static void input_send_pointer(wm_window_t* win) {

    if (!win) {
        return;
    }


    ui_msg_pointer_t msg = {

        .window_id = win->id,
        .x         = (int16_t)(wm.pointer.x - win->x),
        .y         = (int16_t)(wm.pointer.y - win->y),
        .buttons   = wm.pointer.buttons,
    };

    wm_client_queue(win->client, UI_EV_POINTER, &msg, sizeof(msg));
}


/**
 * @brief Gives the pointer event to the window under the pointer, and tells the last one the pointer left.
 *
 * @param win The window under the pointer, as the hit test found it.
 * @param region The region of that window the pointer is over.
 */
static void input_track_pointer(wm_window_t* win, wm_region_t region) {

    if (region != WM_REGION_CONTENT) {
        win = NULL;
    }


    if (wm.pointer_focus && wm.pointer_focus != win) {

        ui_msg_window_t msg = {.window_id = wm.pointer_focus->id};

        wm_client_queue(wm.pointer_focus->client, UI_EV_LEAVE, &msg, sizeof(msg));
    }

    wm.pointer_focus = win;

    input_send_pointer(win);
}


/**
 * @brief Reports the shape the pointer takes over a region.
 *
 * @param region The region under the pointer.
 * @return The cursor shape for it.
 */
static wm_cursor_shape_t input_cursor_for(wm_region_t region) {

    switch (region) {

        case WM_REGION_RESIZE_N:
        case WM_REGION_RESIZE_S:
            return WM_CURSOR_SIZE_VER;

        case WM_REGION_RESIZE_E:
        case WM_REGION_RESIZE_W:
            return WM_CURSOR_SIZE_HOR;

        case WM_REGION_RESIZE_NW:
        case WM_REGION_RESIZE_SE:
            return WM_CURSOR_SIZE_FDIAG;

        case WM_REGION_RESIZE_NE:
        case WM_REGION_RESIZE_SW:
            return WM_CURSOR_SIZE_BDIAG;

        case WM_REGION_CLOSE:
            return WM_CURSOR_HAND;

        default:
            return WM_CURSOR_ARROW;
    }
}


/**
 * @brief Points the cursor at whatever it is about to act on, pinning the shape for the length of a drag.
 */
static void input_update_cursor(wm_region_t region) {

    if (wm.drag.window) {

        wm_cursor_set(wm.drag.region == WM_REGION_TITLEBAR ? WM_CURSOR_SIZE_ALL : input_cursor_for(wm.drag.region));

        return;
    }

    wm_cursor_set(input_cursor_for(region));
}


static void input_begin_drag(wm_window_t* win, wm_region_t region) {

    wm.drag.window = win;
    wm.drag.region = region;
    wm.drag.grab_x = wm.pointer.x;
    wm.drag.grab_y = wm.pointer.y;

    wm.drag.origin.x      = win->x;
    wm.drag.origin.y      = win->y;
    wm.drag.origin.width  = win->width;
    wm.drag.origin.height = win->height;
}


/**
 * @brief Follows the pointer on and off a close button, damaging the button on each transition.
 */
static void input_update_hover(wm_window_t* win, wm_region_t region) {

    wm_window_t* hovered = NULL;

    if (!wm.drag.window || wm.drag.region == WM_REGION_CLOSE) {

        if (region == WM_REGION_CLOSE) {
            hovered = win;
        }
    }

    if (hovered == wm.hovered_close) {
        return;
    }


    if (wm.hovered_close) {

        const wm_rect_t c = wm_window_close_rect(wm.hovered_close);

        wm_damage(&c);
    }

    wm.hovered_close = hovered;

    if (wm.hovered_close) {

        const wm_rect_t c = wm_window_close_rect(wm.hovered_close);

        wm_damage(&c);
    }
}


/**
 * @brief Applies the pointer's travel since the grab to the window being dragged or resized.
 */
static void input_update_drag(void) {

    wm_window_t* win = wm.drag.window;

    const int dx = wm.pointer.x - wm.drag.grab_x;
    const int dy = wm.pointer.y - wm.drag.grab_y;

    if (wm.drag.region == WM_REGION_CLOSE) {
        return;
    }

    if (wm.drag.region == WM_REGION_TITLEBAR) {
        wm_window_move(win, wm.drag.origin.x + dx, wm.drag.origin.y + dy);
        return;
    }


    int x = wm.drag.origin.x;
    int y = wm.drag.origin.y;
    int w = wm.drag.origin.width;
    int h = wm.drag.origin.height;


    switch (wm.drag.region) {

        case WM_REGION_RESIZE_E:
        case WM_REGION_RESIZE_NE:
        case WM_REGION_RESIZE_SE:
            w += dx;
            break;

        case WM_REGION_RESIZE_W:
        case WM_REGION_RESIZE_NW:
        case WM_REGION_RESIZE_SW:
            w -= dx;
            x += dx;
            break;

        default:
            break;
    }

    switch (wm.drag.region) {

        case WM_REGION_RESIZE_S:
        case WM_REGION_RESIZE_SE:
        case WM_REGION_RESIZE_SW:
            h += dy;
            break;

        case WM_REGION_RESIZE_N:
        case WM_REGION_RESIZE_NE:
        case WM_REGION_RESIZE_NW:
            h -= dy;
            y += dy;
            break;

        default:
            break;
    }


    const bool anchored_right  = (wm.drag.region == WM_REGION_RESIZE_W || wm.drag.region == WM_REGION_RESIZE_NW || wm.drag.region == WM_REGION_RESIZE_SW);
    const bool anchored_bottom = (wm.drag.region == WM_REGION_RESIZE_N || wm.drag.region == WM_REGION_RESIZE_NE || wm.drag.region == WM_REGION_RESIZE_NW);

    int cw = w;
    int ch = h;

    wm_window_clamp_size(&cw, &ch);

    if (anchored_right) {
        x -= cw - w;
    }

    if (anchored_bottom) {
        y -= ch - h;
    }

    w = cw;
    h = ch;

    if (x != win->x || y != win->y) {
        wm_window_move(win, x, y);
    }

    if (w != win->width || h != win->height) {
        wm_window_resize(win, w, h);
    }
}


/**
 * @brief Acts on the left pointer button going down or up.
 *
 * A close button commits on release and only if the pointer is still on it; a resize tells the client on release.
 *
 * @param down Whether the button went down or came up.
 */
static void input_handle_button_left(uint8_t down) {

    if (!down) {

        if (wm.drag.window) {

            wm_window_t* win      = wm.drag.window;
            const wm_region_t was = wm.drag.region;

            wm.drag.window = NULL;
            wm.drag.region = WM_REGION_NONE;


            if (was == WM_REGION_CLOSE) {

                const wm_rect_t c = wm_window_close_rect(win);

                wm_damage(&c);

                if (wm.hovered_close == win) {
                    wm_window_request_close(win);
                }

                return;
            }

            if (was != WM_REGION_TITLEBAR) {
                wm_window_notify_configure(win);
            }

            return;
        }

        wm_window_t* over = NULL;

        const wm_region_t at = wm_window_hit_test(wm.pointer.x, wm.pointer.y, &over);

        input_track_pointer(over, at);

        return;
    }


    wm_window_t* win      = NULL;
    wm_region_t region    = wm_window_hit_test(wm.pointer.x, wm.pointer.y, &win);

    if (region == WM_REGION_NONE) {
        return;
    }

    wm_window_raise(win);
    wm_window_focus(win);

    if (region == WM_REGION_CONTENT) {
        input_track_pointer(win, region);
        return;
    }

    input_begin_drag(win, region);

    if (region == WM_REGION_CLOSE) {

        wm.hovered_close = win;

        const wm_rect_t c = wm_window_close_rect(win);

        wm_damage(&c);
    }
}


static void input_handle_button(uint16_t vkey, uint8_t down) {

    uint8_t mask = 0;

    switch (vkey) {

        case BTN_LEFT:
            mask = UI_BUTTON_LEFT;
            break;
        case BTN_RIGHT:
            mask = UI_BUTTON_RIGHT;
            break;
        case BTN_MIDDLE:
            mask = UI_BUTTON_MIDDLE;
            break;

        default:
            return;
    }


    if (down) {
        wm.pointer.buttons |= mask;
    } else {
        wm.pointer.buttons &= (uint8_t)~mask;
    }


    if (mask != UI_BUTTON_LEFT) {

        wm_window_t* over = NULL;

        const wm_region_t region = wm_window_hit_test(wm.pointer.x, wm.pointer.y, &over);

        input_track_pointer(over, region);

        return;
    }


    input_handle_button_left(down);

    input_update_cursor(wm_window_hit_test(wm.pointer.x, wm.pointer.y, NULL));
}


/**
 * @brief Everything a pointer movement pulls in, once the new position is in wm.pointer.
 *
 * @param old The rectangle the pointer occupied before it moved.
 */

static void input_pointer_moved(const wm_rect_t* old) {

    if (wm.pointer.x < 0) {
        wm.pointer.x = 0;
    }

    if (wm.pointer.y < 0) {
        wm.pointer.y = 0;
    }

    if (wm.pointer.x >= wm.display.width) {
        wm.pointer.x = wm.display.width - 1;
    }

    if (wm.pointer.y >= wm.display.height) {
        wm.pointer.y = wm.display.height - 1;
    }


    wm_window_t* over = NULL;

    const wm_region_t region = wm_window_hit_test(wm.pointer.x, wm.pointer.y, &over);


    input_update_cursor(region);


    if (wm.display.hwcursor) {

        wm_display_cursor_move(&wm.display, wm.pointer.x, wm.pointer.y);

    } else {

        const wm_rect_t now = wm_cursor_rect();

        wm_damage(old);
        wm_damage(&now);
    }


    input_update_hover(over, region);

    if (wm.drag.window) {

        input_update_drag();

    } else {

        input_track_pointer(over, region);
    }
}


/**
 * @brief Acts on one decoded input event.
 *
 * @param ev The event to act on.
 */
static void input_handle_event(const event_t ev) {

    switch (ev.ev_type) {

        case EV_KEY:

            if (ev.ev_key.vkey >= BTN_MOUSE && ev.ev_key.vkey <= BTN_TASK) {

                input_handle_button(ev.ev_key.vkey, ev.ev_key.down);

            } else if (!wm_keys_handle(ev.ev_key.vkey, ev.ev_key.down)) {

                input_send_key(ev.ev_key.vkey, ev.ev_key.down);
            }

            break;

        case EV_REL: {

            const wm_rect_t old = wm_cursor_rect();

            if (!ev.ev_rel.x && !ev.ev_rel.y) {
                break;
            }

            wm.pointer.x += ev.ev_rel.x;
            wm.pointer.y -= ev.ev_rel.y;

            input_pointer_moved(&old);

            break;
        }

        case EV_ABS: {

            const wm_rect_t old = wm_cursor_rect();

            const int x = ((int)ev.ev_abs.x * (wm.display.width - 1)) / EV_ABS_MAX;
            const int y = ((int)ev.ev_abs.y * (wm.display.height - 1)) / EV_ABS_MAX;

            if (x == wm.pointer.x && y == wm.pointer.y) {
                break;
            }

            wm.pointer.x = x;
            wm.pointer.y = y;

            input_pointer_moved(&old);

            break;
        }

        default:
            break;
    }
}


/**
 * @brief Drains whatever the pipe has and acts on every whole event in it, keeping the remainder.
 *
 * @param fd The read end of the merged pipe.
 * @return 0 on success, or -1 with errno set.
 */
int wm_input_dispatch(int fd) {

    static uint8_t pending[sizeof(event_t) * 64];
    static size_t held = 0;

    ssize_t e = read(fd, pending + held, sizeof(pending) - held);

    if (e <= 0) {
        return e < 0 && errno == EINTR ? 0 : -1;
    }

    held += (size_t)e;


    size_t offset = 0;

    for (; held - offset >= sizeof(event_t); offset += sizeof(event_t)) {

        event_t ev;

        memcpy(&ev, pending + offset, sizeof(ev));

        input_handle_event(ev);
    }

    if (offset) {

        held -= offset;

        memmove(pending, pending + offset, held);
    }

    return 0;
}
