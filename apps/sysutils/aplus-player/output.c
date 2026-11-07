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
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <aplus/soundcard.h>

#include "player.h"


static const char* output_open_error(int error) {

    switch (error) {

        case EBUSY:
            return "The audio device is busy";

        case ENOENT:
        case ENODEV:
        case ENXIO:
            return "No audio device found";

        default:
            return strerror(error);
    }
}


static void output_clear(player_output_t* output) {

    output->phase         = 0;
    output->input_count   = 0;
    output->output_size   = 0;
    output->output_done   = 0;
    output->output_silent = false;
    output->finished      = false;
    output->padding       = 0;
    output->written       = 0;
    output->silence       = 0;
}


int player_output_open(player_output_t* output, uint32_t rate, const char** error) {

    output->fd = open(PLAYER_DSP_PATH, O_WRONLY | O_NONBLOCK);

    if (output->fd < 0) {
        *error = output_open_error(errno);
        return -1;
    }


    int format = AFMT_S16_LE;

    if (ioctl(output->fd, SNDCTL_DSP_SETFMT, &format) < 0 || (format != AFMT_S16_LE && format != AFMT_U8)) {

        format = AFMT_U8;

        if (ioctl(output->fd, SNDCTL_DSP_SETFMT, &format) < 0 || format != AFMT_U8) {
            *error = "The audio device has no usable sample format";
            goto fail;
        }
    }


    int channels = 2;

    if (ioctl(output->fd, SNDCTL_DSP_CHANNELS, &channels) < 0 || channels < 1 || channels > 2) {
        *error = "The audio device has no usable channel layout";
        goto fail;
    }


    int speed = (int)rate;

    if (ioctl(output->fd, SNDCTL_DSP_SPEED, &speed) < 0 || speed <= 0) {
        *error = "The audio device cannot play at this sample rate";
        goto fail;
    }


    int query = AFMT_QUERY;

    if (ioctl(output->fd, SNDCTL_DSP_SETFMT, &query) == 0 && (query == AFMT_S16_LE || query == AFMT_U8)) {
        format = query;
    }


    int fragment = 0;

    if (ioctl(output->fd, SNDCTL_DSP_GETBLKSIZE, &fragment) < 0 || fragment <= 0) {
        fragment = PLAYER_OUTPUT_BYTES;
    }

    output->rate     = (uint32_t)speed;
    output->channels = (uint16_t)channels;
    output->bits     = format == AFMT_U8 ? 8 : 16;
    output->frame    = (size_t)output->channels * (output->bits / 8);
    output->fragment = (size_t)fragment - (size_t)fragment % output->frame;
    output->step     = ((uint64_t)rate << 32) / output->rate;

    output_clear(output);

    return 0;

fail:

    close(output->fd);
    output->fd = -1;

    return -1;
}


void player_output_close(player_output_t* output) {

    if (output->fd < 0) {
        return;
    }

    ioctl(output->fd, SNDCTL_DSP_RESET, 0);
    close(output->fd);

    output->fd = -1;
}


void player_output_reset(player_output_t* output) {

    if (output->fd >= 0) {
        ioctl(output->fd, SNDCTL_DSP_RESET, 0);
    }

    output_clear(output);
}


static uint8_t* output_emit(const player_output_t* output, uint8_t* cursor, int32_t left, int32_t right, uint32_t gain) {

    left  = (int32_t)(((int64_t)left * gain) >> 16);
    right = (int32_t)(((int64_t)right * gain) >> 16);

    int32_t values[2] = {left, right};

    if (output->channels == 1) {
        values[0] = (left + right) / 2;
    }

    for (uint16_t c = 0; c < output->channels; c++) {

        int32_t v = values[c];

        if (v > INT16_MAX) {
            v = INT16_MAX;
        }

        if (v < INT16_MIN) {
            v = INT16_MIN;
        }

        if (output->bits == 8) {

            *cursor++ = (uint8_t)((v >> 8) + 128);

        } else {

            *cursor++ = (uint8_t)(v & 0xFF);
            *cursor++ = (uint8_t)((v >> 8) & 0xFF);
        }
    }

    return cursor;
}


static bool output_refill(player_output_t* output, player_source_t* source) {

    size_t drop = (size_t)(output->phase >> 32);

    if (drop > output->input_count) {
        drop = output->input_count;
    }

    if (drop) {

        memmove(output->input, output->input + drop, (output->input_count - drop) * sizeof(output->input[0]));

        output->input_count -= drop;
        output->phase -= (uint64_t)drop << 32;
    }

    const size_t got = player_source_read(source, output->input + output->input_count, PLAYER_INPUT_FRAMES - output->input_count);

    output->input_count += got;

    return got > 0;
}


static size_t output_produce(player_output_t* output, player_source_t* source, uint32_t gain) {

    uint8_t* cursor = output->output;

    size_t produced = 0;

    while (produced < PLAYER_OUTPUT_FRAMES) {

        size_t index = (size_t)(output->phase >> 32);

        if (index + 1 >= output->input_count) {

            const bool more = output_refill(output, source);

            index = (size_t)(output->phase >> 32);

            if (index + 1 >= output->input_count) {

                if (more) {
                    continue;
                }

                if (index >= output->input_count) {
                    break;
                }

                cursor = output_emit(output, cursor, output->input[index][0], output->input[index][1], gain);

                output->phase += output->step;
                produced++;

                continue;
            }
        }


        const int64_t frac = (int64_t)(output->phase & 0xFFFFFFFFull);

        const int16_t* a = output->input[index];
        const int16_t* b = output->input[index + 1];

        const int32_t left  = a[0] + (int32_t)(((int64_t)(b[0] - a[0]) * frac) >> 32);
        const int32_t right = a[1] + (int32_t)(((int64_t)(b[1] - a[1]) * frac) >> 32);

        cursor = output_emit(output, cursor, left, right, gain);

        output->phase += output->step;
        produced++;
    }

    return produced;
}


int player_output_fill(player_output_t* output, player_source_t* source, uint32_t gain) {

    if (output->fd < 0) {
        errno = EBADF;
        return -1;
    }


    for (;;) {

        const size_t left = output->output_size - output->output_done;

        if (left >= output->frame) {

            const ssize_t w = write(output->fd, output->output + output->output_done, left);

            if (w < 0) {

                if (errno == EAGAIN || errno == EINTR) {
                    return 0;
                }

                return -1;
            }

            if (w == 0) {
                return 0;
            }

            output->output_done += (size_t)w;

            if (output->output_silent) {
                output->silence += (uint64_t)w;
            } else {
                output->written += (uint64_t)w;
            }

            continue;
        }


        output->output_size = 0;
        output->output_done = 0;

        if (output->finished) {

            if (output->padding == 0) {
                return 0;
            }


            size_t size = output->padding < sizeof(output->output) ? output->padding : sizeof(output->output);

            size -= size % output->frame;

            if (size == 0) {
                output->padding = 0;
                return 0;
            }

            memset(output->output, output->bits == 8 ? 0x80 : 0x00, size);

            output->output_size   = size;
            output->output_silent = true;
            output->padding -= size;

            continue;
        }


        const size_t produced = output_produce(output, source, gain);

        if (produced == 0) {

            output->finished = true;
            output->padding  = output->fragment;

            continue;
        }

        output->output_size   = produced * output->frame;
        output->output_silent = false;
    }
}


uint64_t player_output_heard(player_output_t* output) {

    if (output->fd < 0 || output->frame == 0) {
        return 0;
    }


    int delay = 0;

    if (ioctl(output->fd, SNDCTL_DSP_GETODELAY, &delay) < 0 || delay < 0) {
        delay = 0;
    }


    const uint64_t total = output->written + output->silence;

    uint64_t heard = total > (uint64_t)delay ? total - (uint64_t)delay : 0;

    if (heard > output->written) {
        heard = output->written;
    }

    return heard / output->frame;
}


bool player_output_wants(const player_output_t* output) {
    return !output->finished || output->padding > 0 || output->output_done < output->output_size;
}


bool player_output_done(player_output_t* output) {

    if (player_output_wants(output)) {
        return false;
    }

    return player_output_heard(output) * output->frame >= output->written;
}
