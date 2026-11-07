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
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <aplus/soundcard.h>
#include <aplus/ulog.h>

#include "audiod.h"


#define AUDIOD_PHASE_ONE (1ull << 32)


int audiod_output_open(audiod_output_t* output) {

    if ((output->fd = open(output->path, O_WRONLY | O_NONBLOCK)) < 0) {
        ulog_warnf("cannot open %s: %s", output->path, strerror(errno));
        return -1;
    }


    int format = AFMT_S16_LE;

    if (ioctl(output->fd, SNDCTL_DSP_SETFMT, &format) < 0 || (format != AFMT_S16_LE && format != AFMT_U8)) {

        format = AFMT_U8;

        if (ioctl(output->fd, SNDCTL_DSP_SETFMT, &format) < 0 || format != AFMT_U8) {
            errno = ENODEV;
            goto fail;
        }
    }


    int channels = AUDIOD_OUTPUT_CHANNELS;

    if (ioctl(output->fd, SNDCTL_DSP_CHANNELS, &channels) < 0 || channels < 1 || channels > 2) {
        errno = ENODEV;
        goto fail;
    }


    int rate = AUDIOD_OUTPUT_RATE;

    if (ioctl(output->fd, SNDCTL_DSP_SPEED, &rate) < 0 || rate <= 0) {
        errno = ENODEV;
        goto fail;
    }


    int query = AFMT_QUERY;

    if (ioctl(output->fd, SNDCTL_DSP_SETFMT, &query) == 0 && (query == AFMT_S16_LE || query == AFMT_U8)) {
        format = query;
    }


    output->rate     = (uint32_t)rate;
    output->channels = (uint16_t)channels;
    output->format   = (uint32_t)format;
    output->frame    = (size_t)channels * (format == AFMT_U8 ? 1 : 2);


    int fragment = 0;

    if (ioctl(output->fd, SNDCTL_DSP_GETBLKSIZE, &fragment) < 0 || (size_t)fragment < output->frame) {
        fragment = (int)(AUDIOD_FRAGMENT_FRAMES * output->frame);
    }

    output->fragment = (size_t)fragment - (size_t)fragment % output->frame;


    const size_t frames = output->fragment / output->frame;

    output->mix    = malloc(frames * 2 * sizeof(int32_t));
    output->buffer = malloc(output->fragment);

    if (!output->mix || !output->buffer) {
        errno = ENOMEM;
        goto fail;
    }

    output->written      = 0;
    output->pending      = 0;
    output->pending_done = 0;
    output->idle_since   = audiod_now_ms();

    ulog_infof("opened %s: %u Hz, %u channel(s), %s, %zu-byte fragments", output->path, output->rate, output->channels, output->format == AFMT_U8 ? "u8" : "s16le", output->fragment);

    return 0;

fail:

    {
        const int error = errno;

        audiod_output_close(output);

        errno = error;
    }

    return -1;
}


void audiod_output_close(audiod_output_t* output) {

    if (output->fd >= 0) {
        close(output->fd);
        ulog_infof("closed %s", output->path);
    }

    if (output->mix) {
        free(output->mix);
    }

    if (output->buffer) {
        free(output->buffer);
    }

    output->fd     = -1;
    output->mix    = NULL;
    output->buffer = NULL;
}


static void audiod_output_stream(audiod_output_t* output, audiod_client_t* client, size_t frames) {

    if (!client->primed) {

        if (!audiod_client_pop(client, client->next)) {
            client->started = false;
            return;
        }

        client->current[0] = client->next[0];
        client->current[1] = client->next[1];
        client->phase      = 0;
        client->primed     = true;
    }


    int32_t* mix = output->mix;

    for (size_t i = 0; i < frames; i++) {

        while (client->phase >= AUDIOD_PHASE_ONE) {

            int32_t frame[2];

            if (!audiod_client_pop(client, frame)) {
                client->started = false;
                return;
            }

            client->current[0] = client->next[0];
            client->current[1] = client->next[1];
            client->next[0]    = frame[0];
            client->next[1]    = frame[1];
            client->phase -= AUDIOD_PHASE_ONE;
        }


        const int64_t frac = (int64_t)(client->phase & 0xFFFFFFFFull);

        mix[i * 2 + 0] += client->current[0] + (int32_t)(((int64_t)(client->next[0] - client->current[0]) * frac) >> 32);
        mix[i * 2 + 1] += client->current[1] + (int32_t)(((int64_t)(client->next[1] - client->current[1]) * frac) >> 32);

        client->phase += client->step;
    }
}


static void audiod_output_mix(audiod_output_t* output, int64_t now) {

    const size_t frames = output->fragment / output->frame;

    memset(output->mix, 0, frames * 2 * sizeof(int32_t));


    for (size_t i = 0; i < audiod.count; i++) {

        audiod_client_t* client = audiod.clients[i];

        if (!audiod_client_playable(client, now)) {
            continue;
        }

        const uint64_t consumed = client->consumed;

        audiod_output_stream(output, client, frames);

        if (client->consumed != consumed) {
            audiod_client_mark(client, output->written + frames);
        }
    }


    uint8_t* cursor = output->buffer;

    for (size_t i = 0; i < frames; i++) {

        int32_t values[2] = {output->mix[i * 2 + 0], output->mix[i * 2 + 1]};

        if (output->channels == 1) {
            values[0] = (values[0] + values[1]) / 2;
        }

        for (uint16_t c = 0; c < output->channels; c++) {

            int32_t v = values[c];

            if (v > INT16_MAX) {
                v = INT16_MAX;
            }

            if (v < INT16_MIN) {
                v = INT16_MIN;
            }

            if (output->format == AFMT_U8) {

                *cursor++ = (uint8_t)((v >> 8) + 128);

            } else {

                *cursor++ = (uint8_t)(v & 0xFF);
                *cursor++ = (uint8_t)((v >> 8) & 0xFF);
            }
        }
    }

    output->pending      = output->fragment;
    output->pending_done = 0;
}


static int audiod_output_flush(audiod_output_t* output) {

    while (output->pending_done < output->pending) {

        const ssize_t w = write(output->fd, output->buffer + output->pending_done, output->pending - output->pending_done);

        if (w < 0) {

            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN) {
                return 0;
            }

            return -1;
        }

        if (w == 0) {
            return 0;
        }

        output->pending_done += (size_t)w;
        output->written += (uint64_t)w / output->frame;
    }

    output->pending      = 0;
    output->pending_done = 0;

    return 0;
}


static size_t audiod_output_delay(audiod_output_t* output) {

    int delay = 0;

    if (ioctl(output->fd, SNDCTL_DSP_GETODELAY, &delay) < 0 || delay < 0) {
        return 0;
    }

    return (size_t)delay;
}


int audiod_output_service(audiod_output_t* output) {

    if (output->fd < 0) {
        return -1;
    }


    const int64_t now = audiod_now_ms();

    size_t ready = 0;

    for (size_t i = 0; i < audiod.count; i++) {

        if (audiod.clients[i]->ready) {
            ready++;
        }
    }


    if (ready == 0) {

        if (now - output->idle_since >= AUDIOD_IDLE_MS) {
            audiod_output_close(output);
            return -1;
        }

        return (int)(AUDIOD_IDLE_MS - (now - output->idle_since));
    }

    output->idle_since = now;


    if (audiod_output_flush(output) < 0) {
        goto lost;
    }


    size_t delay = audiod_output_delay(output);

    const uint64_t queued = delay / output->frame;
    const uint64_t played = output->written > queued ? output->written - queued : 0;

    for (size_t i = 0; i < audiod.count; i++) {
        audiod_client_played(audiod.clients[i], played);
    }


    const size_t low  = output->fragment * AUDIOD_LOW_FRAGMENTS;
    const size_t high = output->fragment * AUDIOD_HIGH_FRAGMENTS;

    while (output->pending == 0 && delay < high) {

        audiod_output_mix(output, now);

        if (audiod_output_flush(output) < 0) {
            goto lost;
        }

        delay += output->pending_done ? output->pending_done : output->fragment;
    }


    for (size_t i = 0; i < audiod.count; i++) {
        audiod_client_report(audiod.clients[i]);
    }


    const size_t bytes_per_second = (size_t)output->rate * output->frame;

    int timeout = delay > low ? (int)((delay - low) * 1000 / bytes_per_second) : 0;

    if (timeout < 1) {
        timeout = 1;
    }

    for (size_t i = 0; i < audiod.count; i++) {

        const int wait = audiod_client_wait(audiod.clients[i], now);

        if (wait >= 0 && wait < timeout) {
            timeout = wait > 0 ? wait : 1;
        }
    }

    return timeout;

lost:

    ulog_errorf("writing to %s failed: %s", output->path, strerror(errno));

    audiod_drop_all(EIO);
    audiod_output_close(output);

    return -1;
}
