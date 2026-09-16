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

/**
 * @brief aplus-wm, the display server: it owns the framebuffer and the input devices and hands out windows.
 *
 * A window's surface is a shared memory segment mapped by both ends, so the socket carries only control.
 */

#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <wm.h>


wm_server_t wm = {0};


static int64_t wm_rect_area(const wm_rect_t* r) {

    return (int64_t)r->width * (int64_t)r->height;
}


static wm_rect_t wm_rect_union(const wm_rect_t* a, const wm_rect_t* b) {

    const int x0 = a->x < b->x ? a->x : b->x;
    const int y0 = a->y < b->y ? a->y : b->y;

    const int ax1 = a->x + a->width;
    const int ay1 = a->y + a->height;
    const int bx1 = b->x + b->width;
    const int by1 = b->y + b->height;

    const int x1 = ax1 > bx1 ? ax1 : bx1;
    const int y1 = ay1 > by1 ? ay1 : by1;

    wm_rect_t r = {x0, y0, x1 - x0, y1 - y0};

    return r;
}


static int64_t wm_rect_overlap(const wm_rect_t* a, const wm_rect_t* b) {

    const int x0 = a->x > b->x ? a->x : b->x;
    const int y0 = a->y > b->y ? a->y : b->y;

    const int ax1 = a->x + a->width;
    const int ay1 = a->y + a->height;
    const int bx1 = b->x + b->width;
    const int by1 = b->y + b->height;

    const int x1 = ax1 < bx1 ? ax1 : bx1;
    const int y1 = ay1 < by1 ? ay1 : by1;

    if (x0 >= x1 || y0 >= y1) {
        return 0;
    }

    return (int64_t)(x1 - x0) * (int64_t)(y1 - y0);
}


/**
 * @brief Reports what merging two rectangles would cost, as the area neither of them covers today.
 *
 * @param a The first rectangle.
 * @param b The second rectangle.
 * @return The area the merged rectangle would repaint for nothing; zero when one contains the other.
 */
static int64_t wm_rect_merge_cost(const wm_rect_t* a, const wm_rect_t* b) {

    const wm_rect_t u = wm_rect_union(a, b);

    return wm_rect_area(&u) - (wm_rect_area(a) + wm_rect_area(b) - wm_rect_overlap(a, b));
}


/**
 * @brief Adds a rectangle to what the next frame repaints, merging it in wherever that is cheaper.
 *
 * Two rectangles are worth merging exactly when their union is no larger than the two of them
 * added up, since whatever they overlap on would otherwise be painted twice. That keeps the slow
 * drag -- where the window barely moves and the two rectangles almost coincide -- down to one
 * rectangle, and leaves a fast one, where they share nothing, as two.
 *
 * @param rect The rectangle that has to be repainted.
 */
void wm_damage(const wm_rect_t* rect) {

    int x0 = rect->x;
    int y0 = rect->y;
    int x1 = rect->x + rect->width;
    int y1 = rect->y + rect->height;

    if (x0 < 0) {
        x0 = 0;
    }

    if (y0 < 0) {
        y0 = 0;
    }

    if (x1 > wm.display.width) {
        x1 = wm.display.width;
    }

    if (y1 > wm.display.height) {
        y1 = wm.display.height;
    }

    if (x0 >= x1 || y0 >= y1) {
        return;
    }


    wm_rect_t add = {x0, y0, x1 - x0, y1 - y0};


    for (size_t i = 0; i < wm.damage.count;) {

        if (wm_rect_merge_cost(&wm.damage.rects[i], &add) > wm_rect_overlap(&wm.damage.rects[i], &add)) {
            i++;
            continue;
        }

        add = wm_rect_union(&wm.damage.rects[i], &add);

        wm.damage.rects[i] = wm.damage.rects[--wm.damage.count];

        i = 0;
    }

    if (wm.damage.count < WM_DAMAGE_MAX) {

        wm.damage.rects[wm.damage.count++] = add;

        return;
    }


    wm_rect_t pool[WM_DAMAGE_MAX + 1];

    for (size_t i = 0; i < WM_DAMAGE_MAX; i++) {
        pool[i] = wm.damage.rects[i];
    }

    pool[WM_DAMAGE_MAX] = add;


    size_t best_a = 0;
    size_t best_b = 1;
    int64_t best  = wm_rect_merge_cost(&pool[0], &pool[1]);

    for (size_t i = 0; i < WM_DAMAGE_MAX + 1; i++) {

        for (size_t j = i + 1; j < WM_DAMAGE_MAX + 1; j++) {

            const int64_t cost = wm_rect_merge_cost(&pool[i], &pool[j]);

            if (cost < best) {

                best   = cost;
                best_a = i;
                best_b = j;
            }
        }
    }

    pool[best_a] = wm_rect_union(&pool[best_a], &pool[best_b]);
    pool[best_b] = pool[WM_DAMAGE_MAX];

    for (size_t i = 0; i < WM_DAMAGE_MAX; i++) {
        wm.damage.rects[i] = pool[i];
    }
}


static bool wm_rect_intersects(const wm_rect_t* a, const wm_rect_t* b) {

    return a->x < b->x + b->width && b->x < a->x + a->width && a->y < b->y + b->height && b->y < a->y + a->height;
}


/**
 * @brief Repaints a window's shadow rectangle, which reaches past the frame on every side.
 *
 * @param win The window to repaint.
 */
void wm_damage_window(const wm_window_t* win) {

    const wm_rect_t shadow = wm_window_shadow_rect(win);

    wm_damage(&shadow);
}


/**
 * @brief Reports whether some window paints over the whole of a rectangle anyway.
 *
 * Only the part of a content area clear of the frame's rounded corners counts: the corners are
 * clipped out of the window and leave the desktop showing through.
 *
 * @param rect The rectangle to test.
 * @return true when the desktop behind it never becomes visible.
 */
static bool wm_damage_is_covered(const wm_rect_t* rect) {

    for (wm_window_t* win = wm.windows; win; win = win->next) {

        const int x0 = win->x + WM_CORNER_RADIUS;
        const int y0 = win->y;
        const int x1 = win->x + win->width - WM_CORNER_RADIUS;
        const int y1 = win->y + win->height - WM_CORNER_RADIUS;

        if (x0 >= x1 || y0 >= y1) {
            continue;
        }

        if (rect->x >= x0 && rect->y >= y0 && rect->x + rect->width <= x1 && rect->y + rect->height <= y1) {
            return true;
        }
    }

    return false;
}


static void wm_composite(void) {

    if (!wm.damage.count) {
        return;
    }


    wm_rect_t damage[WM_DAMAGE_MAX];

    const size_t count = wm.damage.count;

    for (size_t i = 0; i < count; i++) {
        damage[i] = wm.damage.rects[i];
    }

    wm.damage.count = 0;


    cairo_t* cr = wm.display.cr;

    cairo_save(cr);

    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);

    for (size_t i = 0; i < count; i++) {
        cairo_rectangle(cr, damage[i].x, damage[i].y, damage[i].width, damage[i].height);
    }

    cairo_clip(cr);


    size_t exposed = 0;

    for (size_t i = 0; i < count; i++) {

        if (wm_damage_is_covered(&damage[i])) {
            continue;
        }

        cairo_rectangle(cr, damage[i].x, damage[i].y, damage[i].width, damage[i].height);

        exposed++;
    }

    if (exposed) {

        cairo_save(cr);

        cairo_clip(cr);

        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source(cr, wm.display.background);
        cairo_paint(cr);

        cairo_restore(cr);

    } else {

        cairo_new_path(cr);
    }


    wm_window_t* stack[64];

    size_t depth = 0;

    for (wm_window_t* win = wm.windows; win && depth < (sizeof(stack) / sizeof(stack[0])); win = win->next) {
        stack[depth++] = win;
    }

    while (depth--) {

        const wm_rect_t extent = wm_window_shadow_rect(stack[depth]);

        bool visible = false;

        for (size_t i = 0; i < count && !visible; i++) {
            visible = wm_rect_intersects(&extent, &damage[i]);
        }

        if (!visible) {
            continue;
        }

        wm_window_paint(cr, stack[depth]);
    }


    if (!wm.display.hwcursor) {
        wm_cursor_paint(cr, wm.pointer.x, wm.pointer.y);
    }

    cairo_restore(cr);


    wm_display_flush(&wm.display, damage, count);
}


static int wm_listen(const char* path) {

    unlink(path);


    int fd;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "aplus-wm: socket() failed: %s\n", strerror(errno));
        return -1;
    }


    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    if (strlen(path) >= sizeof(addr.sun_path)) {
        fprintf(stderr, "aplus-wm: socket path is too long: %s\n", path);
        close(fd);
        return -1;
    }

    strcpy(addr.sun_path, path);


    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "aplus-wm: bind() failed on %s: %s\n", path, strerror(errno));
        close(fd);
        return -1;
    }

    if (listen(fd, 16) < 0) {
        fprintf(stderr, "aplus-wm: listen() failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}


static void wm_on_signal(int signum) {

    (void)signum;

    wm.running = false;
}


static void show_usage(void) {

    printf("Use: aplus-wm [options]...\n"
           "Run the aplus display server.\n\n"
           "   -d, --device                framebuffer device (default: /dev/fb0)\n"
           "   -s, --socket                listening socket (default: " UI_DEFAULT_SOCKET ")\n"
           "       --help                  show this help\n");

    exit(0);
}


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);


    static struct option long_options[] = {
        {"device", required_argument, NULL, 'd'},
        {"socket", required_argument, NULL, 's'},
        {"help",   no_argument,       NULL, 'h'},
        {NULL,     0,                 NULL, 0  }
    };

    const char* device = "/dev/fb0";
    const char* path   = UI_DEFAULT_SOCKET;


    int c, idx;

    while ((c = getopt_long(argc, argv, "d:s:h", long_options, &idx)) != -1) {

        switch (c) {

            case 'd':
                device = optarg;
                break;
            case 's':
                path = optarg;
                break;
            case 'h':
            case '?':
                show_usage();
                break;
            default:
                abort();
        }
    }


    wm.next_window_id = 1;
    wm.running        = true;


    if (wm_display_open(&wm.display, device) < 0) {
        return 1;
    }

    wm.pointer.x = wm.display.width / 2;
    wm.pointer.y = wm.display.height / 2;


    if (wm_font_init() < 0) {
        wm_font_fini();
    }


    int listener = wm_listen(path);

    if (listener < 0) {
        wm_display_close(&wm.display);
        return 1;
    }


    int input = wm_input_open();

    if (input < 0) {
        close(listener);
        wm_display_close(&wm.display);
        return 1;
    }


    signal(SIGINT, wm_on_signal);
    signal(SIGTERM, wm_on_signal);

    signal(SIGPIPE, SIG_IGN);


    fprintf(stderr, "aplus-wm: listening on %s\n", path);


    wm_rect_t everything = {0, 0, wm.display.width, wm.display.height};

    wm_damage(&everything);
    wm_composite();


    while (wm.running) {

        struct pollfd pfds[64];
        wm_client_t* owners[64];

        nfds_t count = 0;

        pfds[count].fd     = listener;
        pfds[count].events = POLLIN;
        owners[count]      = NULL;
        count++;

        pfds[count].fd     = input;
        pfds[count].events = POLLIN;
        owners[count]      = NULL;
        count++;

        for (wm_client_t* client = wm.clients; client && count < (sizeof(pfds) / sizeof(pfds[0])); client = client->next) {

            pfds[count].fd      = client->fd;
            pfds[count].events  = POLLIN | (wm_client_wants_write(client) ? POLLOUT : 0);
            pfds[count].revents = 0;
            owners[count]       = client;

            count++;
        }


        int e = poll(pfds, count, -1);

        if (e < 0) {

            if (errno == EINTR) {
                continue;
            }

            fprintf(stderr, "aplus-wm: poll() failed: %s\n", strerror(errno));
            break;
        }


        if (pfds[0].revents & POLLIN) {
            wm_client_accept(listener);
        }

        if (pfds[1].revents & POLLIN) {

            if (wm_input_dispatch(input) < 0) {
                fprintf(stderr, "aplus-wm: the input pipe went away\n");
                break;
            }
        }


        for (nfds_t i = 2; i < count; i++) {

            wm_client_t* client = owners[i];

            if (!client) {
                continue;
            }

            if (pfds[i].revents & POLLOUT) {

                if (wm_client_flush(client) < 0) {
                    client->dead = true;
                }
            }

            if (pfds[i].revents & POLLIN) {

                if (wm_client_read(client) < 0) {
                    client->dead = true;
                }
            }

            if (pfds[i].revents & (POLLHUP | POLLERR)) {
                client->dead = true;
            }
        }


        wm_client_t* client = wm.clients;

        while (client) {

            wm_client_t* next = client->next;

            if (client->dead) {
                wm_client_destroy(client);
            }

            client = next;
        }


        wm_keys_reap();

        wm_composite();
    }


    fprintf(stderr, "aplus-wm: shutting down\n");

    for (wm_window_t* win = wm.windows; win; win = win->next) {

        ui_msg_window_t msg = {.window_id = win->id};

        wm_client_queue(win->client, UI_EV_CLOSE, &msg, sizeof(msg));
        wm_client_flush(win->client);
    }

    while (wm.clients) {
        wm_client_destroy(wm.clients);
    }

    wm_input_close();
    wm_font_fini();
    wm_cursor_fini();
    wm_display_close(&wm.display);

    close(listener);
    unlink(path);

    return 0;
}
