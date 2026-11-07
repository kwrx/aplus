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
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <aplus/ulog.h>

#include "audiod.h"


audiod_t audiod = {

    .listener = -1,

    .output = {
        .path = AUDIO_DSP_PATH,
        .fd   = -1,
    },
};


int64_t audiod_now_ms(void) {

    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}


static void audiod_remove(size_t index) {

    audiod_client_destroy(audiod.clients[index]);

    memmove(&audiod.clients[index], &audiod.clients[index + 1], (audiod.count - index - 1) * sizeof(audiod.clients[0]));

    audiod.count--;
}


void audiod_drop_all(int error) {

    while (audiod.count > 0) {
        audiod_client_fail(audiod.clients[audiod.count - 1], error);
        audiod_remove(audiod.count - 1);
    }
}


static int audiod_listen(const char* path) {

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

    if (chmod(path, 0666) < 0) {
        ulog_warnf("chmod() failed on %s: %s", path, strerror(errno));
    }

    if (listen(fd, 16) < 0) {
        ulog_errorf("listen() failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) < 0) {
        ulog_errorf("fcntl() failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}


static void audiod_accept(void) {

    for (;;) {

        const int fd = accept(audiod.listener, NULL, NULL);

        if (fd < 0) {

            if (errno != EAGAIN && errno != EINTR) {
                ulog_warnf("accept() failed: %s", strerror(errno));
            }

            return;
        }

        if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) < 0) {
            close(fd);
            continue;
        }


        audiod_client_t* client = NULL;

        if (audiod.count == AUDIOD_CLIENTS_MAX || (client = audiod_client_create(fd)) == NULL) {

            audio_msg_header_t header = {.type = AUDIO_MSG_ERROR, .size = sizeof(audio_msg_error_t)};
            audio_msg_error_t error   = {.error = audiod.count == AUDIOD_CLIENTS_MAX ? EBUSY : ENOMEM};

            if (write(fd, &header, sizeof(header)) == sizeof(header)) {
                write(fd, &error, sizeof(error));
            }

            close(fd);
            continue;
        }

        audiod.clients[audiod.count++] = client;
    }
}


static void audiod_on_signal(int signum) {

    (void)signum;

    audiod.running = false;
}


static void show_usage(void) {

    printf("Use: " AUDIOD_NAME " [options]...\n"
           "Mix every client's audio stream onto the sound card.\n\n"
           "   -d, --device                output device (default: " AUDIO_DSP_PATH ")\n"
           "   -s, --socket                listening socket (default: " AUDIO_SOCKET ")\n"
           "   -h, --help                  show this help\n");
}


int main(int argc, char** argv) {

    static struct option long_options[] = {

        {"device", required_argument, NULL, 'd'},
        {"socket", required_argument, NULL, 's'},
        {"help",   no_argument,       NULL, 'h'},
        {NULL,     0,                 NULL, 0  },
    };


    const char* path = AUDIO_SOCKET;

    int c;

    while ((c = getopt_long(argc, argv, "d:s:h", long_options, NULL)) != -1) {

        switch (c) {

            case 'd':
                audiod.output.path = optarg;
                break;

            case 's':
                path = optarg;
                break;

            case 'h':
                show_usage();
                return 0;

            default:
                show_usage();
                return 1;
        }
    }


    signal(SIGINT, audiod_on_signal);
    signal(SIGTERM, audiod_on_signal);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, SIG_IGN);


    if ((audiod.listener = audiod_listen(path)) < 0) {
        return 1;
    }

    ulog_infof("listening on %s", path);


    audiod.running = true;

    while (audiod.running) {

        const int timeout = audiod_output_service(&audiod.output);


        struct pollfd fds[AUDIOD_CLIENTS_MAX + 1];

        fds[0].fd      = audiod.listener;
        fds[0].events  = POLLIN;
        fds[0].revents = 0;

        for (size_t i = 0; i < audiod.count; i++) {

            fds[i + 1].fd      = audiod.clients[i]->fd;
            fds[i + 1].events  = POLLIN | (audiod.clients[i]->outbox_size ? POLLOUT : 0);
            fds[i + 1].revents = 0;
        }

        const size_t count = audiod.count;

        if (poll(fds, count + 1, timeout) < 0) {

            if (errno == EINTR) {
                continue;
            }

            ulog_errorf("poll() failed: %s", strerror(errno));
            break;
        }


        for (size_t i = count; i > 0; i--) {

            audiod_client_t* client = audiod.clients[i - 1];

            const short revents = fds[i].revents;

            if (revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) {

                if (audiod_client_receive(client) < 0) {

                    if (errno != EPIPE) {
                        ulog_warnf("dropping stream '%s': %s", client->name, strerror(errno));
                    }

                    audiod_client_flush(client);
                    audiod_remove(i - 1);
                    continue;
                }
            }

            if (audiod_client_flush(client) < 0) {
                audiod_remove(i - 1);
            }
        }


        if (fds[0].revents & POLLIN) {
            audiod_accept();
        }
    }


    audiod_drop_all(ESHUTDOWN);
    audiod_output_close(&audiod.output);

    close(audiod.listener);
    unlink(path);

    return 0;
}
