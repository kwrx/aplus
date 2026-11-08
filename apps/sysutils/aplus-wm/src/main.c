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

#include <aplus/ulog.h>

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

    ui_damage_add(&wm.damage, ui_rect_clip(rect, wm.display.width, wm.display.height));
}


void wm_damage_glass(const wm_rect_t* rect) {

    ui_damage_add(&wm.glass_damage, ui_rect_clip(rect, wm.display.width, wm.display.height));
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


void wm_damage_window_edges(const wm_window_t* win) {

    const wm_rect_t s     = wm_window_shadow_rect(win);
    const wm_rect_t f     = wm_window_frame(win);
    const wm_insets_t in  = wm_window_insets(win);
    const int corner      = WM_MAX(wm_window_radius(win), 3);
    const int top         = WM_MAX(in.top, corner);
    const int bottom      = WM_MAX(in.bottom, corner);
    const int left        = WM_MAX(in.left, 3);
    const int right       = WM_MAX(in.right, 3);

    const wm_rect_t strips[] = {
        {s.x,                    s.y,                       s.width,                                  f.y - s.y + top                          },
        {s.x,                    f.y + f.height - bottom,   s.width,                                  s.y + s.height - (f.y + f.height) + bottom},
        {s.x,                    f.y + top,                 f.x - s.x + left,                         f.height - top - bottom                  },
        {f.x + f.width - right,  f.y + top,                 s.x + s.width - (f.x + f.width) + right,  f.height - top - bottom                  },
    };

    for (size_t i = 0; i < WM_ARRAY_COUNT(strips); i++) {
        wm_damage(&strips[i]);
    }
}


/**
 * @brief Reports whether some window paints over the whole of a rectangle anyway.
 *
 * Only the part of a content area clear of the frame's rounded corners counts, and only a
 * window that is solid right now covers anything: see wm_window_opaque().
 *
 * @param rect The rectangle to test.
 * @return true when the desktop behind it never becomes visible.
 */
static bool wm_damage_is_covered(const wm_rect_t* rect, const wm_window_t* below) {

    bool covered = false;

    for (wm_window_t* win = wm.windows; win && win != below; win = win->next) {

        if (wm_window_drawer(win)) {
            continue;
        }

        if (wm_window_frosted(win)) {

            const wm_rect_t frame = wm_window_frame(win);
            const wm_rect_t reach = {frame.x - WM_BLUR_RADIUS, frame.y - WM_BLUR_RADIUS, frame.width + 2 * WM_BLUR_RADIUS, frame.height + 2 * WM_BLUR_RADIUS};

            if (ui_rect_intersects(&reach, rect)) {
                covered = false;
            }

            continue;
        }

        if (!wm_window_opaque(win)) {
            continue;
        }


        const int radius = wm_window_radius(win);

        const wm_rect_t opaque = {

            .x      = win->x + radius,
            .y      = win->y,
            .width  = win->width - 2 * radius,
            .height = win->height - radius,
        };

        if (opaque.width > 0 && opaque.height > 0 && ui_rect_contains(&opaque, rect)) {
            covered = true;
        }
    }

    return covered;
}


/**
 * @brief Follows the adapter into a new mode: rebuilds the display and repaints all of it.
 *
 * @return 0 on success, or -1 when the new mode cannot be drawn on.
 */
static int wm_display_changed(void) {

    if (wm_display_resize(&wm.display) < 0) {
        ulog_errorf("cannot follow the display mode change");
        return -1;
    }

    wm.pointer.x = WM_CLAMP(wm.pointer.x, 0, wm.display.width - 1);
    wm.pointer.y = WM_CLAMP(wm.pointer.y, 0, wm.display.height - 1);

    wm_display_cursor_move(&wm.display, wm.pointer.x, wm.pointer.y);


    wm_rect_t everything = {0, 0, wm.display.width, wm.display.height};

    wm_damage(&everything);

    return 0;
}


static struct {

    struct pollfd* pfds;
    wm_client_t** owners;
    size_t size;

} wm_poll;


static size_t wm_poll_reserve(size_t count) {

    if (count <= wm_poll.size) {
        return wm_poll.size;
    }

    const size_t grown = WM_MAX(count, 2 * wm_poll.size);

    struct pollfd* pfds  = realloc(wm_poll.pfds, grown * sizeof(*pfds));

    if (pfds) {
        wm_poll.pfds = pfds;
    }

    wm_client_t** owners = realloc(wm_poll.owners, grown * sizeof(*owners));

    if (owners) {
        wm_poll.owners = owners;
    }

    if (pfds && owners) {
        wm_poll.size = grown;
    }

    return wm_poll.size;
}


static void wm_composite(void) {

    if (!wm.damage.count && !wm.glass_damage.count) {
        return;
    }

    wm_blur_damage();

    for (size_t i = 0; i < wm.glass_damage.count; i++) {
        ui_damage_add(&wm.damage, wm.glass_damage.rects[i]);
    }

    ui_damage_reset(&wm.glass_damage);


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

        if (wm_damage_is_covered(&damage[i], NULL)) {
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


    static wm_window_t** stack;
    static size_t stack_size;

    size_t depth = 0;

    for (wm_window_t* win = wm.windows; win; win = win->next) {

        if (depth == stack_size) {

            const size_t grown = stack_size ? stack_size * 2 : 64;

            wm_window_t** bigger = realloc(stack, grown * sizeof(*stack));

            if (!bigger) {
                break;
            }

            stack      = bigger;
            stack_size = grown;
        }

        stack[depth++] = win;
    }

    while (depth--) {

        if (wm_window_drawer(stack[depth])) {
            continue;
        }

        const wm_rect_t extent = wm_window_shadow_rect(stack[depth]);

        bool visible = stack[depth]->morph != NULL;

        for (size_t i = 0; i < count && !visible; i++) {

            const wm_rect_t shown = ui_rect_intersect(&extent, &damage[i]);

            visible = !ui_rect_empty(&shown) && !wm_damage_is_covered(&shown, stack[depth]);
        }

        if (!visible) {
            continue;
        }

        wm_window_paint(cr, stack[depth]);
    }

    wm_ghost_paint(cr, damage, count);

    wm_taskbar_capture(damage, count);

    wm_taskbar_paint(cr, damage, count);

    wm_drawer_paint(cr, damage, count);


    if (!wm.display.hwcursor) {
        wm_cursor_paint(cr, wm.pointer.x, wm.pointer.y);
    }

    cairo_restore(cr);


    if (wm_display_flush(&wm.display, damage, count) < 0 && errno == ESTALE) {

        if (wm_display_changed() < 0) {
            wm.running = false;
            return;
        }

        wm_composite();
    }
}


static int wm_listen(const char* path) {

    unlink(path);


    int fd;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        ulog_errorf("socket() failed: %s", strerror(errno));
        return -1;
    }


    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    if (strlen(path) >= sizeof(addr.sun_path)) {
        ulog_errorf("socket path is too long: %s", path);
        close(fd);
        return -1;
    }

    strcpy(addr.sun_path, path);


    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ulog_errorf("bind() failed on %s: %s", path, strerror(errno));
        close(fd);
        return -1;
    }

    if (listen(fd, 16) < 0) {
        ulog_errorf("listen() failed: %s", strerror(errno));
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
           "   -c, --control               control socket (default: " UI_CONTROL_SOCKET ")\n"
           "   -w, --wallpaper             desktop wallpaper, instead of the theme's\n"
           "       --help                  show this help\n");

    exit(0);
}


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);


    static struct option long_options[] = {
        {"device",    required_argument, NULL, 'd'},
        {"socket",    required_argument, NULL, 's'},
        {"control",   required_argument, NULL, 'c'},
        {"wallpaper", required_argument, NULL, 'w'},
        {"help",      no_argument,       NULL, 'h'},
        {NULL,        0,                 NULL, 0  }
    };

    const char* device  = "/dev/fb0";
    const char* path    = UI_DEFAULT_SOCKET;
    const char* control = UI_CONTROL_SOCKET;


    int c, idx;

    while ((c = getopt_long(argc, argv, "d:s:c:w:h", long_options, &idx)) != -1) {

        switch (c) {

            case 'd':
                device = optarg;
                break;
            case 's':
                path = optarg;
                break;
            case 'c':
                control = optarg;
                break;
            case 'w':
                wm.wallpaper = optarg;
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


    wm_theme_init();

    if (wm_display_open(&wm.display, device) < 0) {
        return 1;
    }

    wm.pointer.x = wm.display.width / 2;
    wm.pointer.y = wm.display.height / 2;

    wm_display_cursor_move(&wm.display, wm.pointer.x, wm.pointer.y);


    ui_font_face(wm.theme.font);

    if (wm_shadow_init(&wm.shadow_key, WM_SHADOW_KEY_BLUR, UI_WINDOW_RADIUS) < 0 || wm_shadow_init(&wm.shadow_ambient, WM_SHADOW_AMBIENT_BLUR, UI_WINDOW_RADIUS) < 0) {
        ulog_warnf("cannot render the window shadows, drawing none");
    }


    int listener = wm_listen(path);

    if (listener < 0) {
        wm_display_close(&wm.display);
        return 1;
    }

    int controller = wm_listen(control);

    if (controller < 0) {
        close(listener);
        wm_display_close(&wm.display);
        return 1;
    }


    int input = wm_input_open();

    if (input < 0) {
        close(controller);
        close(listener);
        wm_display_close(&wm.display);
        return 1;
    }

    const int clock = wm_taskbar_open();

    wm_pins_load();

    wm_launcher_copy();


    signal(SIGINT, wm_on_signal);
    signal(SIGTERM, wm_on_signal);
    signal(SIGCHLD, wm_keys_on_child);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, SIG_IGN);


    ulog_infof("listening on %s, control on %s", path, control);


    wm_rect_t everything = {0, 0, wm.display.width, wm.display.height};

    wm.frame_time = ui_anim_now();

    wm_damage(&everything);
    wm_composite();


    int timeout = -1;

    while (wm.running) {

        size_t clients = 4;

        for (wm_client_t* client = wm.clients; client; client = client->next) {
            clients++;
        }

        const size_t room = wm_poll_reserve(clients);

        if (room < 4) {
            ulog_errorf("cannot allocate the poll set");
            break;
        }

        struct pollfd* pfds  = wm_poll.pfds;
        wm_client_t** owners = wm_poll.owners;

        nfds_t count = 0;

        pfds[count].fd     = listener;
        pfds[count].events = POLLIN;
        owners[count]      = NULL;
        count++;

        pfds[count].fd     = controller;
        pfds[count].events = POLLIN;
        owners[count]      = NULL;
        count++;

        pfds[count].fd     = input;
        pfds[count].events = POLLIN;
        owners[count]      = NULL;
        count++;

        pfds[count].fd      = clock;
        pfds[count].events  = POLLIN;
        pfds[count].revents = 0;
        owners[count]       = NULL;
        count++;

        for (wm_client_t* client = wm.clients; client && count < room; client = client->next) {

            pfds[count].fd      = client->fd;
            pfds[count].events  = POLLIN | (wm_client_wants_write(client) ? POLLOUT : 0);
            pfds[count].revents = 0;
            owners[count]       = client;

            count++;
        }


        int e = poll(pfds, count, timeout);

        if (e < 0) {

            if (errno == EINTR) {
                continue;
            }

            ulog_errorf("poll() failed: %s", strerror(errno));
            break;
        }


        if (pfds[0].revents & POLLIN) {
            wm_client_accept(listener, false);
        }

        if (pfds[1].revents & POLLIN) {
            wm_client_accept(controller, true);
        }

        if (pfds[2].revents & POLLIN) {

            if (wm_input_dispatch(input) < 0) {
                ulog_errorf("the input pipe went away");
                break;
            }
        }


        if (pfds[3].revents & POLLIN) {
            wm_taskbar_tick(clock);
        }


        for (nfds_t i = 4; i < count; i++) {

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

        wm.frame_time = ui_anim_now();

        const int next = wm_animate(wm.frame_time);

        wm_composite();

        if (next < 0) {

            timeout = -1;

        } else {

            const uint64_t spent = ui_anim_now() - wm.frame_time;

            timeout = spent >= (uint64_t)next ? 0 : next - (int)spent;
        }
    }


    ulog_infof("shutting down");

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
    wm_ghost_fini();
    wm_drawer_fini();
    wm_taskbar_fini();
    wm_pins_fini();
    wm_menu_fini();
    wm_shadow_fini(&wm.shadow_key);
    wm_shadow_fini(&wm.shadow_ambient);
    wm_display_close(&wm.display);

    close(controller);
    unlink(control);

    close(listener);
    unlink(path);

    return 0;
}
