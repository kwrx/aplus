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


/*
 * The input devices are char devices, and device_mkdev() never installs an inode poll
 * hook. vfs_poll() then falls through to "whatever you asked for is ready", so a poll()
 * over the input devices spins at 100% without ever blocking.
 *
 * The way around it is the one the devices do support: a blocking read(), which the
 * kernel turns into a proper futex sleep woken by the IRQ handler's vfs_write(). One
 * thread per device does that and forwards each event_t down a socketpair, and the
 * socketpair -- being an AF_UNIX socket -- has a real poll implementation the main
 * loop can wait on together with the client connections.
 *
 * Which of the pointing devices is actually live is the host's business, not this
 * server's: a virtio tablet and a PS/2 mouse can both be present while only one of them
 * is fed, and which one that is can change while the machine runs. Both are read and
 * their events merged, so the pointer follows whichever is talking.
 *
 * Those threads are the only concurrency in the server, and the socketpair is the only
 * thing they share -- so it is the only thing that needs a lock. It is a stream, not a
 * datagram queue: a write of one event_t is not promised to go out whole, and the kernel
 * hands back a short count rather than finishing the job once the buffer is nearly full.
 * Two threads pushing records into it therefore need two things that a bare write() does
 * not give them. The record has to be completed before another thread starts one, or the
 * two interleave; and a short read on the far end has to be carried over rather than
 * treated as a truncated event. Without either, one split record shifts the stream by a
 * few bytes and every event after it is read out of the middle of two others -- which
 * looks exactly like the input devices having gone mad, and never recovers.
 */

static int input_pipe[2] = {-1, -1};

//? Held for one whole record, so that what the main loop reads back is a sequence of
//? events rather than a splice of two of them.
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


/* One whole event_t onto the pipe, or a failure. The loop is what makes a short write a
   delay rather than a lost thread: taking anything but the full count as the pipe having
   gone away retires that device for the rest of the session, and leaves the bytes it did
   manage to write in the stream. */
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


int wm_input_open(void) {

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, input_pipe) < 0) {
        fprintf(stderr, "aplus-wm: socketpair() failed: %s\n", strerror(errno));
        return -1;
    }


    for (size_t i = 0; i < sizeof(input_devices) / sizeof(input_devices[0]); i++) {

        /* A machine missing a pointing device is still perfectly usable with the keyboard,
           and no machine has every pointing device, so only the keyboard is required. */
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


/* The whole of the pointer stream a content area sees: the window under the pointer gets
   the event, and whichever window had it last is told the pointer has gone once it is no
   longer that one. Tracking the transition here rather than at each call site is what
   keeps a leave from being forgotten on one of the paths that moves the pointer. */
static void input_track_pointer(void) {

    wm_window_t* win = NULL;

    if (wm_window_hit_test(wm.pointer.x, wm.pointer.y, &win) != WM_REGION_CONTENT) {
        win = NULL;
    }


    if (wm.pointer_focus && wm.pointer_focus != win) {

        ui_msg_window_t msg = {.window_id = wm.pointer_focus->id};

        wm_client_queue(wm.pointer_focus->client, UI_EV_LEAVE, &msg, sizeof(msg));
    }

    wm.pointer_focus = win;

    input_send_pointer(win);
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


/* Follows the pointer on and off a close button, damaging the button on each transition so
   it lights up and goes out again. It keeps running while that same button is held, which
   is what lets a press slide off the button and cancel instead of closing the window; any
   other drag suppresses it, since the pointer is busy. */
static void input_update_hover(void) {

    wm_window_t* hovered = NULL;

    if (!wm.drag.window || wm.drag.region == WM_REGION_CLOSE) {

        wm_window_t* win = NULL;

        if (wm_window_hit_test(wm.pointer.x, wm.pointer.y, &win) == WM_REGION_CLOSE) {
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


    /* Clamping the size after having already moved the origin would let a window slide
       once it hit a limit, so give back whatever the clamp took away -- but only on the
       edges that moved the origin in the first place. A south or east drag leaves the
       origin where it was, and correcting it there would slide the window instead. */
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
        input_track_pointer();
        return;
    }


    if (!down) {

        if (wm.drag.window) {

            wm_window_t* win      = wm.drag.window;
            const wm_region_t was = wm.drag.region;

            wm.drag.window = NULL;
            wm.drag.region = WM_REGION_NONE;


            if (was == WM_REGION_CLOSE) {

                const wm_rect_t c = wm_window_close_rect(win);

                wm_damage(&c);

                /* A button commits on release, and only if the pointer is still on it, so
                   a press can be taken back by sliding off. */
                if (wm.hovered_close == win) {
                    wm_window_request_close(win);
                }

                return;
            }

            /* Only now does the client hear about the new size, so it repaints once
               instead of on every mouse packet of the drag. */
            if (was != WM_REGION_TITLEBAR) {
                wm_window_notify_configure(win);
            }

            return;
        }

        input_track_pointer();

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
        input_track_pointer();
        return;
    }

    input_begin_drag(win, region);

    if (region == WM_REGION_CLOSE) {

        /* A press that was not preceded by any motion -- the pointer was already parked on
           the button -- leaves the hover unset, and the button has to draw itself pressed
           either way. Raising and focusing only damage a window that was not already on
           top and active, so the button needs saying so itself. */
        wm.hovered_close = win;

        const wm_rect_t c = wm_window_close_rect(win);

        wm_damage(&c);
    }
}


/* Everything a pointer movement pulls in, once the new position is in wm.pointer: both
 * pointing device kinds land here, having differed only in how they said where to go.
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


    /* With a cursor plane the pointer is not part of the frame, so a movement damages
       nothing and costs one short command instead of repainting and re-flushing the
       rectangle it left and the one it arrived at. That is the whole gain: a pointer
       moves far more often than anything else on screen, and over a still desktop it
       now moves without touching the framebuffer at all. */
    if (wm.display.hwcursor) {

        wm_display_cursor_move(&wm.display, wm.pointer.x, wm.pointer.y);

    } else {

        const wm_rect_t now = wm_cursor_rect();

        wm_damage(old);
        wm_damage(&now);
    }


    input_update_hover();

    if (wm.drag.window) {

        input_update_drag();

    } else {

        input_track_pointer();
    }
}


static void input_handle_event(const event_t ev) {

    switch (ev.ev_type) {

        case EV_KEY:

            if (ev.ev_key.vkey >= BTN_MOUSE && ev.ev_key.vkey <= BTN_TASK) {

                input_handle_button(ev.ev_key.vkey, ev.ev_key.down);

            } else if (!wm_keys_handle(ev.ev_key.vkey, ev.ev_key.down)) {

                /* A key the server claimed for a binding stops here. Passing it on as well
                   would run the binding and type the key into the focused window. */
                input_send_key(ev.ev_key.vkey, ev.ev_key.down);
            }

            break;

        case EV_REL: {

            const wm_rect_t old = wm_cursor_rect();

            /* A wheel event carries no movement of its own, and pushing it through the move
               path would re-place the pointer where it already is. */
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

            /* The axes arrive normalized to EV_ABS_MAX, whatever range the device itself
               uses, so placing the pointer needs nothing but the size of the screen. */
            const int x = ((int)ev.ev_abs.x * (wm.display.width - 1)) / EV_ABS_MAX;
            const int y = ((int)ev.ev_abs.y * (wm.display.height - 1)) / EV_ABS_MAX;

            /* An absolute device reports a position, not a change, and several of its
               positions land on the same pixel. Repainting or re-placing the cursor plane
               for a pointer that has not moved is pure cost. */
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


/* Drain whatever the pipe has and act on every whole event in it.
 *
 * The pipe is a stream, so a read is not a record: POLLIN fires on a single byte, and what
 * comes back can be several events, or one and a piece of the next. Anything left over is
 * kept for the next wakeup rather than dropped -- treating a partial read as a truncated
 * event is what turned one split write into a permanently misaligned stream, where every
 * event afterwards was assembled out of the tail of one and the head of another.
 *
 * Each event is copied out of the buffer rather than cast in place: event_t is packed, and
 * the buffer only happens to be aligned while nothing has been carried over. */
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
