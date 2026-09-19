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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <wm.h>


/**
 * @brief How much may queue for a client before it is dropped as misbehaving rather than slow.
 */
#define WM_CLIENT_TX_MAX (1 * 1024 * 1024)

/**
 * @brief One read of the client socket.
 */
#define WM_CLIENT_CHUNK (16 * 1024)

/**
 * @brief Everything the receive buffer ever holds: one maximum-sized frame plus the chunk completing it.
 */
#define WM_CLIENT_RX_MAX (UI_MSG_PAYLOAD_MAX + sizeof(ui_msg_header_t) + WM_CLIENT_CHUNK)


static int wm_buffer_reserve(uint8_t** data, size_t* capacity, size_t needed, size_t limit) {

    if (needed <= *capacity) {
        return 0;
    }

    if (needed > limit) {
        errno = EMSGSIZE;
        return -1;
    }


    size_t capacity_new = *capacity ? *capacity : 4096;

    while (capacity_new < needed) {
        capacity_new *= 2;
    }

    if (capacity_new > limit) {
        capacity_new = limit;
    }


    uint8_t* data_new = (uint8_t*)realloc(*data, capacity_new);

    if (!data_new) {
        return -1;
    }

    *data     = data_new;
    *capacity = capacity_new;

    return 0;
}


wm_client_t* wm_client_accept(int listener) {

    int fd = accept4(listener, NULL, NULL, O_NONBLOCK);

    if (fd < 0) {
        return NULL;
    }


    wm_client_t* client = (wm_client_t*)calloc(1, sizeof(wm_client_t));

    if (!client) {
        close(fd);
        return NULL;
    }

    client->fd = fd;

    client->next = wm.clients;
    wm.clients   = client;

    return client;
}


void wm_client_destroy(wm_client_t* client) {

    if (!client) {
        return;
    }


    wm_window_t* win = wm.windows;

    while (win) {

        wm_window_t* next = win->next;

        if (win->client == client) {
            wm_window_destroy(win);
        }

        win = next;
    }


    wm_client_t** it = &wm.clients;

    while (*it) {

        if (*it == client) {
            *it = client->next;
            break;
        }

        it = &(*it)->next;
    }


    if (client->fd >= 0) {
        close(client->fd);
    }

    free(client->rx.data);
    free(client->tx.data);
    free(client);
}


int wm_client_queue(wm_client_t* client, uint16_t type, const void* payload, size_t size) {

    if (!client || client->dead) {
        return -1;
    }


    const size_t needed = sizeof(ui_msg_header_t) + size;


    if (client->tx.head && client->tx.head == client->tx.size) {
        client->tx.head = 0;
        client->tx.size = 0;
    } else if (client->tx.head > 0 && client->tx.size + needed > client->tx.capacity) {
        memmove(client->tx.data, client->tx.data + client->tx.head, client->tx.size - client->tx.head);
        client->tx.size -= client->tx.head;
        client->tx.head = 0;
    }


    if (wm_buffer_reserve(&client->tx.data, &client->tx.capacity, client->tx.size + needed, WM_CLIENT_TX_MAX) < 0) {
        client->dead = true;
        return -1;
    }


    ui_msg_header_t hdr = {

        .type   = type,
        .flags  = 0,
        .length = (uint32_t)size,
    };

    memcpy(client->tx.data + client->tx.size, &hdr, sizeof(hdr));
    client->tx.size += sizeof(hdr);

    if (size) {
        memcpy(client->tx.data + client->tx.size, payload, size);
        client->tx.size += size;
    }

    return 0;
}


bool wm_client_wants_write(const wm_client_t* client) {
    return client->tx.size > client->tx.head;
}


int wm_client_flush(wm_client_t* client) {

    while (client->tx.size > client->tx.head) {

        ssize_t e = write(client->fd, client->tx.data + client->tx.head, client->tx.size - client->tx.head);

        if (e > 0) {
            client->tx.head += (size_t)e;
            continue;
        }

        if (e < 0 && errno == EINTR) {
            continue;
        }

        if (e < 0 && errno == EAGAIN) {
            return 0;
        }

        return -1;
    }

    client->tx.head = 0;
    client->tx.size = 0;

    return 0;
}


static int wm_client_handle_hello(wm_client_t* client, const uint8_t* payload, size_t size) {

    ui_msg_hello_t hello;

    if (size != sizeof(hello)) {
        return -1;
    }

    memcpy(&hello, payload, sizeof(hello));

    if (hello.version != UI_PROTOCOL_VERSION) {
        fprintf(stderr, "aplus-wm: client speaks protocol %u, expected %u\n", hello.version, UI_PROTOCOL_VERSION);
        return -1;
    }

    client->hello = true;

    hello.version = UI_PROTOCOL_VERSION;

    return wm_client_queue(client, UI_EV_HELLO, &hello, sizeof(hello));
}


static int wm_client_handle_create_window(wm_client_t* client, const uint8_t* payload, size_t size) {

    ui_msg_create_window_t req;

    if (size != sizeof(req)) {
        return -1;
    }

    memcpy(&req, payload, sizeof(req));

    req.title[UI_TITLE_MAX - 1] = '\0';


    if (req.flags & ~(uint32_t)UI_WINDOW_BORDERLESS) {
        fprintf(stderr, "aplus-wm: client asked for window flags this server does not know (%#x)\n", req.flags);
        return -1;
    }


    wm_window_t* win = wm_window_create(client, req.width, req.height, req.title, req.flags);

    if (!win) {
        return -1;
    }

    if (wm_window_notify_configure(win) < 0) {
        return -1;
    }

    wm_window_focus(win);

    return 0;
}


/**
 * @brief Acts on a commit, which names the rectangle the client changed and carries no pixels.
 *
 * @param client The client that committed.
 * @param payload The message body.
 * @param size The length of the message body.
 * @return 0 on success, or -1 with errno set.
 */
static int wm_client_handle_commit(wm_client_t* client, const uint8_t* payload, size_t size) {

    ui_msg_commit_t req;

    if (size != sizeof(req)) {
        return -1;
    }

    memcpy(&req, payload, sizeof(req));


    wm_window_t* win = wm_window_from_id(req.window_id);

    if (!win || win->client != client) {
        return -1;
    }

    if (req.serial != win->serial) {
        return 0;
    }

    if (wm_window_damage_content(win, req.x, req.y, req.width, req.height) < 0) {
        return -1;
    }


    wm_rect_t damage = {

        .x      = win->x + req.x,
        .y      = win->y + req.y,
        .width  = req.width,
        .height = req.height,
    };

    wm_damage(&damage);

    return 0;
}


static int wm_client_handle_set_title(wm_client_t* client, const uint8_t* payload, size_t size) {

    ui_msg_set_title_t req;

    if (size != sizeof(req)) {
        return -1;
    }

    memcpy(&req, payload, sizeof(req));

    req.title[UI_TITLE_MAX - 1] = '\0';


    wm_window_t* win = wm_window_from_id(req.window_id);

    if (!win || win->client != client) {
        return -1;
    }

    strncpy(win->title, req.title, UI_TITLE_MAX - 1);
    win->title[UI_TITLE_MAX - 1] = '\0';

    wm_damage_window(win);

    return 0;
}


static int wm_client_handle_destroy_window(wm_client_t* client, const uint8_t* payload, size_t size) {

    ui_msg_window_t req;

    if (size != sizeof(req)) {
        return -1;
    }

    memcpy(&req, payload, sizeof(req));


    wm_window_t* win = wm_window_from_id(req.window_id);

    if (!win || win->client != client) {
        return -1;
    }

    wm_window_destroy(win);

    return 0;
}


static int wm_client_handle(wm_client_t* client, uint16_t type, const uint8_t* payload, size_t size) {

    if (!client->hello && type != UI_REQ_HELLO) {
        return -1;
    }

    switch (type) {

        case UI_REQ_HELLO:
            return wm_client_handle_hello(client, payload, size);

        case UI_REQ_CREATE_WINDOW:
            return wm_client_handle_create_window(client, payload, size);

        case UI_REQ_COMMIT:
            return wm_client_handle_commit(client, payload, size);

        case UI_REQ_SET_TITLE:
            return wm_client_handle_set_title(client, payload, size);

        case UI_REQ_DESTROY_WINDOW:
            return wm_client_handle_destroy_window(client, payload, size);

        default:
            return 0;
    }
}


/**
 * @brief Acts on every whole frame the receive buffer holds, shifting the partial one back to the front.
 *
 * @param client The client to dispatch for.
 * @return 0 on success, or -1 with errno set.
 */

static int wm_client_dispatch(wm_client_t* client) {

    size_t offset = 0;

    for (;;) {

        if (client->rx.size - offset < sizeof(ui_msg_header_t)) {
            break;
        }


        ui_msg_header_t hdr;

        memcpy(&hdr, client->rx.data + offset, sizeof(hdr));

        if (hdr.length > UI_MSG_PAYLOAD_MAX) {
            fprintf(stderr, "aplus-wm: client sent an oversized frame (%u bytes)\n", hdr.length);
            return -1;
        }

        if (client->rx.size - offset < sizeof(hdr) + hdr.length) {
            break;
        }


        if (wm_client_handle(client, hdr.type, client->rx.data + offset + sizeof(hdr), hdr.length) < 0) {
            fprintf(stderr, "aplus-wm: client sent a bad message (type %#x, %u bytes)\n", hdr.type, hdr.length);
            return -1;
        }

        offset += sizeof(hdr) + hdr.length;
    }


    if (offset) {

        memmove(client->rx.data, client->rx.data + offset, client->rx.size - offset);
        client->rx.size -= offset;
    }

    return 0;
}


/**
 * @brief Reads from a client, accumulating bytes and acting on whole frames, draining to EAGAIN.
 *
 * @param client The client to read from.
 * @return 0 on success, or -1 with errno set.
 */
int wm_client_read(wm_client_t* client) {

    uint8_t chunk[WM_CLIENT_CHUNK];

    bool drained = false;

    while (!drained) {

        ssize_t e = read(client->fd, chunk, sizeof(chunk));

        if (e == 0) {
            return -1;
        }

        if (e < 0) {

            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN) {
                break;
            }

            return -1;
        }

        if (e < (ssize_t)sizeof(chunk)) {
            drained = true;
        }

        if (wm_buffer_reserve(&client->rx.data, &client->rx.capacity, client->rx.size + (size_t)e, WM_CLIENT_RX_MAX) < 0) {
            fprintf(stderr, "aplus-wm: cannot hold %zu bytes from a client: %s\n", client->rx.size + (size_t)e, strerror(errno));
            return -1;
        }

        memcpy(client->rx.data + client->rx.size, chunk, (size_t)e);
        client->rx.size += (size_t)e;

        if (wm_client_dispatch(client) < 0) {
            return -1;
        }
    }

    return 0;
}
