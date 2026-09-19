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


/**
 * @brief Adds a rectangle to what the next frame repaints, clipped to the screen.
 *
 * Which rectangles are worth merging into one is ui_damage_add()'s decision, the same one the
 * clients make about their own surfaces.
 *
 * @param rect The rectangle that has to be repainted.
 */
void wm_damage(const wm_rect_t* rect) {

    ui_damage_add(&wm.damage, wm_rect_clip(rect, wm.display.width, wm.display.height));
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

        const wm_rect_t opaque = {

            .x      = win->x + WM_CORNER_RADIUS,
            .y      = win->y,
            .width  = win->width - 2 * WM_CORNER_RADIUS,
            .height = win->height - WM_CORNER_RADIUS,
        };

        if (opaque.width <= 0 || opaque.height <= 0) {
            continue;
        }

        if (wm_rect_contains(&opaque, rect)) {
            return true;
        }
    }

    return false;
}


static void wm_composite(void) {

    if (!wm.damage.count) {
        return;
    }


    wm_rect_t damage[UI_DAMAGE_MAX];

    const size_t count = wm.damage.count;

    for (size_t i = 0; i < count; i++) {
        damage[i] = wm.damage.rects[i];
    }

    ui_damage_reset(&wm.damage);


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

    for (wm_window_t* win = wm.windows; win && depth < WM_ARRAY_COUNT(stack); win = win->next) {
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
           "   -w, --wallpaper             desktop wallpaper (default: " WM_WALLPAPER_PATH ")\n"
           "       --help                  show this help\n");

    exit(0);
}


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);


    static struct option long_options[] = {
        {"device",    required_argument, NULL, 'd'},
        {"socket",    required_argument, NULL, 's'},
        {"wallpaper", required_argument, NULL, 'w'},
        {"help",      no_argument,       NULL, 'h'},
        {NULL,        0,                 NULL, 0  }
    };

    const char* device    = "/dev/fb0";
    const char* path      = UI_DEFAULT_SOCKET;
    const char* wallpaper = WM_WALLPAPER_PATH;


    int c, idx;

    while ((c = getopt_long(argc, argv, "d:s:w:h", long_options, &idx)) != -1) {

        switch (c) {

            case 'd':
                device = optarg;
                break;
            case 's':
                path = optarg;
                break;
            case 'w':
                wallpaper = optarg;
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


    if (wm_display_open(&wm.display, device, wallpaper) < 0) {
        return 1;
    }

    wm.pointer.x = wm.display.width / 2;
    wm.pointer.y = wm.display.height / 2;


    ui_font_face(WM_FONT_PATH);


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

        for (wm_client_t* client = wm.clients; client && count < WM_ARRAY_COUNT(pfds); client = client->next) {

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
    wm_cursor_fini();
    wm_display_close(&wm.display);

    close(listener);
    unlink(path);

    return 0;
}
