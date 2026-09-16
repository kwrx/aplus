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
#include <sys/uio.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <aplus/ui.h>

#include "ui_internal.h"


/**
 * @brief Writes a whole buffer to a descriptor, looping over the short writes a socket may return.
 *
 * @param fd The descriptor to write to.
 * @param buf The bytes to send.
 * @param size The number of bytes to send.
 * @return The number of bytes sent, or -1 with errno set.
 */
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


/**
 * @brief Sends one message, header and payload together so that it costs a single write.
 *
 * @param fd The descriptor to send on.
 * @param type The message type.
 * @param payload The message body, or NULL when there is none.
 * @param size The length of the body.
 * @return 0 on success, or -1 with errno set.
 */
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

    struct iovec iov[2] = {
        {.iov_base = &hdr,                 .iov_len = sizeof(hdr)},
        {.iov_base = (void*)(uintptr_t)payload, .iov_len = size   },
    };

    const int parts = size ? 2 : 1;

    size_t left = sizeof(hdr) + size;
    int at      = 0;

    while (left > 0) {

        ssize_t e = writev(fd, &iov[at], parts - at);

        if (e < 0) {

            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (e == 0) {
            errno = EIO;
            return -1;
        }

        left -= (size_t)e;

        while (at < parts && (size_t)e >= iov[at].iov_len) {
            e -= (ssize_t)iov[at].iov_len;
            at++;
        }

        if (at < parts && e) {
            iov[at].iov_base = (uint8_t*)iov[at].iov_base + e;
            iov[at].iov_len -= (size_t)e;
        }
    }

    return 0;
}


/**
 * @brief Reads once off the socket onto the end of the connection's buffer.
 *
 * @param conn The connection to read from.
 * @return The number of bytes read, or -1 with errno set; zero only when the peer has gone.
 */
int ui_conn_fill(ui_connection_t* conn) {

    if (conn->rx.head && conn->rx.head == conn->rx.size) {

        conn->rx.head = 0;
        conn->rx.size = 0;
    }


    const size_t want = sizeof(ui_msg_header_t) + UI_MSG_PAYLOAD_MAX;

    if (conn->rx.size + want > conn->rx.capacity) {

        if (conn->rx.head) {

            memmove(conn->rx.data, conn->rx.data + conn->rx.head, conn->rx.size - conn->rx.head);

            conn->rx.size -= conn->rx.head;
            conn->rx.head = 0;
        }
    }

    if (conn->rx.size + want > conn->rx.capacity) {

        const size_t capacity = conn->rx.size + want;

        uint8_t* data = (uint8_t*)realloc(conn->rx.data, capacity);

        if (!data) {
            return -1;
        }

        conn->rx.data     = data;
        conn->rx.capacity = capacity;
    }


    for (;;) {

        ssize_t e = read(conn->fd, conn->rx.data + conn->rx.size, conn->rx.capacity - conn->rx.size);

        if (e > 0) {

            conn->rx.size += (size_t)e;

            return (int)e;
        }

        if (e == 0) {
            errno = ECONNRESET;
            return -1;
        }

        if (errno == EINTR) {
            continue;
        }

        return -1;
    }
}


/**
 * @brief Reports whether a whole message is already sitting in the connection's buffer.
 *
 * @param conn The connection to look at.
 * @return 1 when one is, 0 when more has to be read, or -1 when the peer named an impossible length.
 */
int ui_conn_message_ready(ui_connection_t* conn) {

    const size_t held = conn->rx.size - conn->rx.head;

    if (held < sizeof(ui_msg_header_t)) {
        return 0;
    }


    ui_msg_header_t hdr;

    memcpy(&hdr, conn->rx.data + conn->rx.head, sizeof(hdr));

    if (hdr.length > UI_MSG_PAYLOAD_MAX) {
        errno = EPROTO;
        return -1;
    }

    return held >= sizeof(hdr) + hdr.length;
}


/**
 * @brief Takes bytes off the connection, reading more only when the buffer runs short.
 *
 * @param conn The connection to read from.
 * @param buf Receives the bytes.
 * @param size How many to take.
 * @return 0 on success, or -1 with errno set.
 */
int ui_conn_read(ui_connection_t* conn, void* buf, size_t size) {

    uint8_t* at = (uint8_t*)buf;

    while (size) {

        const size_t held = conn->rx.size - conn->rx.head;

        if (!held) {

            if (ui_conn_fill(conn) < 0) {
                return -1;
            }

            continue;
        }

        const size_t take = held < size ? held : size;

        memcpy(at, conn->rx.data + conn->rx.head, take);

        conn->rx.head += take;

        at += take;
        size -= take;
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

    if (ui_conn_read(conn, &hdr, sizeof(hdr)) < 0) {
        ui_disconnect(conn);
        return NULL;
    }

    if (hdr.type != UI_EV_HELLO || hdr.length != sizeof(hello)) {
        ui_disconnect(conn);
        errno = EPROTO;
        return NULL;
    }

    if (ui_conn_read(conn, &hello, sizeof(hello)) < 0) {
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

    free(conn->rx.data);
    free(conn);
}


int ui_connection_fd(ui_connection_t* conn) {
    return conn ? conn->fd : -1;
}
