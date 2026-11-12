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
           "       --bench                 measure compositing instead, see --bench --help\n"
           "       --help                  show this help\n");

    exit(0);
}


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    if (argc > 1 && !strcmp(argv[1], "--bench")) {
        return wm_bench_main(argc - 1, argv + 1);
    }


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
    wm_stroke_fini();
    wm_display_close(&wm.display);

    close(controller);
    unlink(control);

    close(listener);
    unlink(path);

    return 0;
}
