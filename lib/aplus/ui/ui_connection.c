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
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <aplus/ui.h>

#include "ui_internal.h"


/* A local socket is an ordinary descriptor here, so write() can come back having moved
   fewer bytes than asked: ringbuffer_write() stops at the end of the peer's buffer and
   reports what it managed. Every send in this protocol therefore has to loop, or a frame
   would be silently truncated and desynchronise the stream for good. */
ssize_t ui_send_all(int fd, const void* buf, size_t size) {

    const uint8_t* p = (const uint8_t*)buf;
    size_t done      = 0;

    while (done < size) {

        ssize_t e = write(fd, p + done, size - done);

        if (e > 0) {
            done += (size_t)e;
            continue;
        }

        if (e < 0 && errno == EINTR) {
            continue;
        }

        /* A zero-length write is not something a stream socket should do, but treating it
           as success would spin here forever. */
        if (e == 0) {
            errno = EIO;
        }

        return -1;
    }

    return (ssize_t)done;
}


ssize_t ui_recv_all(int fd, void* buf, size_t size) {

    uint8_t* p  = (uint8_t*)buf;
    size_t done = 0;

    while (done < size) {

        ssize_t e = read(fd, p + done, size - done);

        if (e > 0) {
            done += (size_t)e;
            continue;
        }

        /* A zero-length read is the peer closing, which for a half-received frame is an
           error rather than a short result: there is no way to resynchronise. */
        if (e == 0) {
            errno = ECONNRESET;
            return -1;
        }

        if (errno == EINTR) {
            continue;
        }

        return -1;
    }

    return (ssize_t)done;
}


int ui_send_msg(int fd, uint16_t type, const void* payload, size_t size) {

    if (size > UI_MSG_PAYLOAD_MAX) {
        errno = EMSGSIZE;
        return -1;
    }


    ui_msg_header_t hdr = {

        .type   = type,
        .flags  = 0,
        .length = (uint32_t)size,
    };

    if (ui_send_all(fd, &hdr, sizeof(hdr)) < 0) {
        return -1;
    }

    if (size && ui_send_all(fd, payload, size) < 0) {
        return -1;
    }

    return 0;
}


ui_connection_t* ui_connect(const char* path, int retry_ms) {

    if (!path) {
        path = UI_DEFAULT_SOCKET;
    }


    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    if (strlen(path) >= sizeof(addr.sun_path)) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    strcpy(addr.sun_path, path);


    int fd      = -1;
    int waited  = 0;
    int backoff = 50;

    for (;;) {

        if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
            return NULL;
        }

        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            break;
        }

        close(fd);
        fd = -1;

        /* The server is usually started from the same script, a moment earlier, so the
           socket may not exist yet. Retrying here is what removes the need for a sleep
           between the two lines of init.sh. */
        if (waited >= retry_ms) {
            return NULL;
        }

        struct timespec ts = {

            .tv_sec  = backoff / 1000,
            .tv_nsec = (long)(backoff % 1000) * 1000000L,
        };

        nanosleep(&ts, NULL);

        waited += backoff;
    }


    ui_connection_t* conn = (ui_connection_t*)calloc(1, sizeof(ui_connection_t));

    if (!conn) {
        close(fd);
        return NULL;
    }

    conn->fd      = fd;
    conn->windows = NULL;


    ui_msg_hello_t hello = {.version = UI_PROTOCOL_VERSION};

    if (ui_send_msg(fd, UI_REQ_HELLO, &hello, sizeof(hello)) < 0) {
        ui_disconnect(conn);
        return NULL;
    }


    ui_msg_header_t hdr;

    if (ui_recv_all(fd, &hdr, sizeof(hdr)) < 0) {
        ui_disconnect(conn);
        return NULL;
    }

    if (hdr.type != UI_EV_HELLO || hdr.length != sizeof(hello)) {
        ui_disconnect(conn);
        errno = EPROTO;
        return NULL;
    }

    if (ui_recv_all(fd, &hello, sizeof(hello)) < 0) {
        ui_disconnect(conn);
        return NULL;
    }

    if (hello.version != UI_PROTOCOL_VERSION) {
        ui_disconnect(conn);
        errno = EPROTONOSUPPORT;
        return NULL;
    }

    return conn;
}


void ui_disconnect(ui_connection_t* conn) {

    if (!conn) {
        return;
    }

    while (conn->windows) {

        ui_window_t* next = conn->windows->next;

        ui_window_drop_surface(conn->windows);

        free(conn->windows);

        conn->windows = next;
    }

    if (conn->fd >= 0) {
        close(conn->fd);
    }

    free(conn);
}


int ui_connection_fd(ui_connection_t* conn) {
    return conn ? conn->fd : -1;
}
