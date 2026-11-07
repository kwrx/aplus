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
#include <unistd.h>

#include <aplus/ulog.h>

#include "audiod.h"


audiod_client_t* audiod_client_create(int fd) {

    audiod_client_t* client = calloc(1, sizeof(*client));

    if (!client) {
        return NULL;
    }

    client->fd = fd;

    return client;
}


void audiod_client_destroy(audiod_client_t* client) {

    if (client->ready) {
        ulog_infof("stream '%s' closed", client->name);
    }

    close(client->fd);

    if (client->ring) {
        free(client->ring);
    }

    free(client);
}


static int audiod_client_queue(audiod_client_t* client, uint32_t type, const void* payload, size_t size) {

    if (client->outbox_size + sizeof(audio_msg_header_t) + size > sizeof(client->outbox)) {
        return -1;
    }


    audio_msg_header_t header = {

        .type = type,
        .size = (uint32_t)size,
    };

    memcpy(client->outbox + client->outbox_size, &header, sizeof(header));
    memcpy(client->outbox + client->outbox_size + sizeof(header), payload, size);

    client->outbox_size += sizeof(header) + size;

    return 0;
}


int audiod_client_flush(audiod_client_t* client) {

    size_t done = 0;

    while (done < client->outbox_size) {

        const ssize_t w = write(client->fd, client->outbox + done, client->outbox_size - done);

        if (w < 0) {

            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN) {
                break;
            }

            return -1;
        }

        done += (size_t)w;
    }

    memmove(client->outbox, client->outbox + done, client->outbox_size - done);

    client->outbox_size -= done;

    return 0;
}


void audiod_client_fail(audiod_client_t* client, int error) {

    audio_msg_error_t message = {

        .error = error,
    };

    audiod_client_queue(client, AUDIO_MSG_ERROR, &message, sizeof(message));
    audiod_client_flush(client);
}


void audiod_client_report(audiod_client_t* client) {

    if (!client->ready) {
        return;
    }

    if (client->reported && client->reported_serial == client->serial && client->reported_consumed == client->consumed && client->reported_heard == client->heard) {
        return;
    }


    audio_msg_status_t status = {

        .serial   = client->serial,
        .consumed = client->consumed,
        .heard    = client->heard,
    };

    if (audiod_client_queue(client, AUDIO_MSG_STATUS, &status, sizeof(status)) < 0) {
        return;
    }

    client->reported          = true;
    client->reported_serial   = client->serial;
    client->reported_consumed = client->consumed;
    client->reported_heard    = client->heard;

    audiod_client_flush(client);
}


static void audiod_client_clear(audiod_client_t* client) {

    client->ring_head   = 0;
    client->ring_count  = 0;
    client->started     = false;
    client->primed      = false;
    client->phase       = 0;
    client->consumed    = 0;
    client->heard       = 0;
    client->marks_head  = 0;
    client->marks_count = 0;
    client->reported    = false;
}


static int audiod_client_hello(audiod_client_t* client, const uint8_t* payload, size_t size) {

    if (client->ready || size < sizeof(audio_msg_hello_t)) {
        errno = EPROTO;
        return -1;
    }


    audio_msg_hello_t hello;

    memcpy(&hello, payload, sizeof(hello));

    hello.name[sizeof(hello.name) - 1] = '\0';


    if (hello.version != AUDIO_PROTOCOL_VERSION) {
        audiod_client_fail(client, EPROTONOSUPPORT);
        errno = EPROTONOSUPPORT;
        return -1;
    }

    if (hello.rate < AUDIO_RATE_MIN || hello.rate > AUDIO_RATE_MAX || hello.channels < 1 || hello.channels > AUDIO_CHANNELS_MAX || (hello.format != AUDIO_FORMAT_U8 && hello.format != AUDIO_FORMAT_S16_LE)) {
        audiod_client_fail(client, EINVAL);
        errno = EINVAL;
        return -1;
    }


    if (audiod.output.fd < 0 && audiod_output_open(&audiod.output) < 0) {

        const int error = errno;

        audiod_client_fail(client, error);

        errno = error;
        return -1;
    }


    const size_t frame = (size_t)hello.channels * (hello.format == AUDIO_FORMAT_U8 ? 1 : 2);
    const size_t rate  = (size_t)hello.rate * frame;

    size_t window = rate * AUDIOD_WINDOW_MS / 1000;
    window -= window % frame;

    size_t prebuffer = rate * AUDIOD_PREBUFFER_MS / 1000;
    prebuffer -= prebuffer % frame;


    if ((client->ring = malloc(window)) == NULL) {
        audiod_client_fail(client, ENOMEM);
        errno = ENOMEM;
        return -1;
    }

    memcpy(client->name, hello.name, sizeof(client->name));

    client->rate          = hello.rate;
    client->channels      = hello.channels;
    client->format        = hello.format;
    client->frame         = frame;
    client->ring_capacity = window;
    client->prebuffer     = prebuffer > frame ? prebuffer : frame;
    client->step          = ((uint64_t)hello.rate << 32) / audiod.output.rate;

    audiod_client_clear(client);


    audio_msg_welcome_t welcome = {

        .version = AUDIO_PROTOCOL_VERSION,
        .window  = (uint32_t)window,
    };

    if (audiod_client_queue(client, AUDIO_MSG_WELCOME, &welcome, sizeof(welcome)) < 0) {
        errno = ENOBUFS;
        return -1;
    }

    client->ready = true;

    ulog_infof("stream '%s' opened: %u Hz, %u channel(s), %s", client->name, client->rate, client->channels, client->format == AUDIO_FORMAT_U8 ? "u8" : "s16le");

    return 0;
}


static int audiod_client_data(audiod_client_t* client, const uint8_t* payload, size_t size) {

    if (!client->ready || size % client->frame || size > client->ring_capacity - client->ring_count) {
        errno = EPROTO;
        return -1;
    }


    size_t tail = (client->ring_head + client->ring_count) % client->ring_capacity;

    size_t first = client->ring_capacity - tail;

    if (first > size) {
        first = size;
    }

    memcpy(client->ring + tail, payload, first);
    memcpy(client->ring, payload + first, size - first);

    client->ring_count += size;
    client->last_data = audiod_now_ms();

    return 0;
}


static int audiod_client_reset(audiod_client_t* client, const uint8_t* payload, size_t size) {

    if (!client->ready || size < sizeof(audio_msg_reset_t)) {
        errno = EPROTO;
        return -1;
    }


    audio_msg_reset_t reset;

    memcpy(&reset, payload, sizeof(reset));

    audiod_client_clear(client);

    client->serial = reset.serial;

    return 0;
}


static int audiod_client_dispatch(audiod_client_t* client, const audio_msg_header_t* header, const uint8_t* payload) {

    switch (header->type) {

        case AUDIO_MSG_HELLO:
            return audiod_client_hello(client, payload, header->size);

        case AUDIO_MSG_DATA:
            return audiod_client_data(client, payload, header->size);

        case AUDIO_MSG_RESET:
            return audiod_client_reset(client, payload, header->size);

        default:
            errno = EPROTO;
            return -1;
    }
}


int audiod_client_receive(audiod_client_t* client) {

    for (;;) {

        const ssize_t r = read(client->fd, client->inbox + client->inbox_size, sizeof(client->inbox) - client->inbox_size);

        if (r < 0) {

            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN) {
                return 0;
            }

            return -1;
        }

        if (r == 0) {
            errno = EPIPE;
            return -1;
        }

        client->inbox_size += (size_t)r;


        size_t at = 0;

        while (client->inbox_size - at >= sizeof(audio_msg_header_t)) {

            audio_msg_header_t header;

            memcpy(&header, client->inbox + at, sizeof(header));

            if (header.size > AUDIO_DATA_MAX) {
                errno = EPROTO;
                return -1;
            }

            if (client->inbox_size - at < sizeof(header) + header.size) {
                break;
            }

            if (audiod_client_dispatch(client, &header, client->inbox + at + sizeof(header)) < 0) {
                return -1;
            }

            at += sizeof(header) + header.size;
        }

        memmove(client->inbox, client->inbox + at, client->inbox_size - at);

        client->inbox_size -= at;
    }
}


bool audiod_client_playable(audiod_client_t* client, int64_t now) {

    if (!client->ready || client->ring_count == 0) {
        return false;
    }

    if (!client->started && (client->ring_count >= client->prebuffer || now - client->last_data >= AUDIOD_PREBUFFER_MS)) {
        client->started = true;
    }

    return client->started;
}


int audiod_client_wait(const audiod_client_t* client, int64_t now) {

    if (!client->ready || client->started || client->ring_count == 0) {
        return -1;
    }

    const int64_t left = AUDIOD_PREBUFFER_MS - (now - client->last_data);

    return left > 0 ? (int)left : 0;
}


bool audiod_client_pop(audiod_client_t* client, int32_t frame[2]) {

    if (client->ring_count < client->frame) {
        return false;
    }


    uint8_t raw[4];

    for (size_t i = 0; i < client->frame; i++) {
        raw[i] = client->ring[(client->ring_head + i) % client->ring_capacity];
    }

    client->ring_head = (client->ring_head + client->frame) % client->ring_capacity;
    client->ring_count -= client->frame;
    client->consumed += client->frame;


    for (uint16_t c = 0; c < client->channels; c++) {

        if (client->format == AUDIO_FORMAT_U8) {
            frame[c] = ((int32_t)raw[c] - 128) << 8;
        } else {
            frame[c] = (int16_t)((uint16_t)raw[c * 2] | ((uint16_t)raw[c * 2 + 1] << 8));
        }
    }

    if (client->channels == 1) {
        frame[1] = frame[0];
    }

    return true;
}


void audiod_client_mark(audiod_client_t* client, uint64_t position) {

    if (client->marks_count == AUDIOD_MARKS_MAX) {

        client->heard      = client->marks[client->marks_head].consumed;
        client->marks_head = (client->marks_head + 1) % AUDIOD_MARKS_MAX;
        client->marks_count--;
    }

    const size_t at = (client->marks_head + client->marks_count) % AUDIOD_MARKS_MAX;

    client->marks[at].position = position;
    client->marks[at].consumed = client->consumed;
    client->marks_count++;
}


void audiod_client_played(audiod_client_t* client, uint64_t played) {

    while (client->marks_count > 0 && client->marks[client->marks_head].position <= played) {

        client->heard      = client->marks[client->marks_head].consumed;
        client->marks_head = (client->marks_head + 1) % AUDIOD_MARKS_MAX;
        client->marks_count--;
    }
}
