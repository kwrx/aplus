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
 * /dev/kbd and /dev/mouse are char devices, and device_mkdev() never installs an
 * inode poll hook. vfs_poll() then falls through to "whatever you asked for is
 * ready", so a poll() over the input devices spins at 100% without ever blocking.
 *
 * The way around it is the one the devices do support: a blocking read(), which the
 * kernel turns into a proper futex sleep woken by the IRQ handler's vfs_write(). One
 * thread per device does that and forwards each event_t down a socketpair, and the
 * socketpair -- being an AF_UNIX socket -- has a real poll implementation the main
 * loop can wait on together with the client connections.
 */

static int input_pipe[2] = {-1, -1};

static pthread_t thr_keyboard;
static pthread_t thr_mouse;

static int fd_keyboard = -1;
static int fd_mouse    = -1;

static bool thr_keyboard_started = false;
static bool thr_mouse_started    = false;


static void* input_thread(void* arg) {

    int fd = *(int*)arg;

    for (;;) {

        event_t ev;

        ssize_t e = read(fd, &ev, sizeof(ev));

        if (e == (ssize_t)sizeof(ev)) {

            if (write(input_pipe[1], &ev, sizeof(ev)) != (ssize_t)sizeof(ev)) {
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


    if ((fd_keyboard = open("/dev/kbd", O_RDONLY)) < 0) {
        fprintf(stderr, "aplus-wm: open() failed: cannot open /dev/kbd: %s\n", strerror(errno));
        return -1;
    }

    if (pthread_create(&thr_keyboard, NULL, input_thread, &fd_keyboard) != 0) {
        fprintf(stderr, "aplus-wm: pthread_create() failed for /dev/kbd: %s\n", strerror(errno));
        return -1;
    }

    thr_keyboard_started = true;


    /* A machine without a PS/2 mouse is still perfectly usable with the keyboard, so a
       missing /dev/mouse is a warning rather than a failure. */
    if ((fd_mouse = open("/dev/mouse", O_RDONLY)) < 0) {

        fprintf(stderr, "aplus-wm: warning: cannot open /dev/mouse: %s\n", strerror(errno));

    } else if (pthread_create(&thr_mouse, NULL, input_thread, &fd_mouse) != 0) {

        fprintf(stderr, "aplus-wm: warning: pthread_create() failed for /dev/mouse: %s\n", strerror(errno));

    } else {

        thr_mouse_started = true;
    }


    return input_pipe[0];
}


void wm_input_close(void) {

    if (thr_keyboard_started) {
        pthread_cancel(thr_keyboard);
    }

    if (thr_mouse_started) {
        pthread_cancel(thr_mouse);
    }

    if (fd_keyboard >= 0) {
        close(fd_keyboard);
    }

    if (fd_mouse >= 0) {
        close(fd_mouse);
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

        wm_window_t* win = NULL;

        if (wm_window_hit_test(wm.pointer.x, wm.pointer.y, &win) == WM_REGION_CONTENT) {
            input_send_pointer(win);
        }

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

        wm_window_t* win = NULL;

        if (wm_window_hit_test(wm.pointer.x, wm.pointer.y, &win) == WM_REGION_CONTENT) {
            input_send_pointer(win);
        }

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
        input_send_pointer(win);
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


int wm_input_dispatch(int fd) {

    event_t ev;

    ssize_t e = read(fd, &ev, sizeof(ev));

    if (e != (ssize_t)sizeof(ev)) {
        return e < 0 && errno == EINTR ? 0 : -1;
    }


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

            wm.pointer.x += ev.ev_rel.x;
            wm.pointer.y -= ev.ev_rel.y;

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


            const wm_rect_t now = wm_cursor_rect();

            wm_damage(&old);
            wm_damage(&now);


            input_update_hover();

            if (wm.drag.window) {

                input_update_drag();

            } else {

                wm_window_t* win = NULL;

                if (wm_window_hit_test(wm.pointer.x, wm.pointer.y, &win) == WM_REGION_CONTENT) {
                    input_send_pointer(win);
                }
            }

            break;
        }

        default:
            break;
    }

    return 0;
}
