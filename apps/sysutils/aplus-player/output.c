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
#include <string.h>

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

    if ((output->stream = audio_stream_open("aplus-player", rate, 2, AUDIO_FORMAT_S16_LE)) == NULL) {
        *error = output_open_error(errno);
        return -1;
    }

    output->rate     = audio_stream_rate(output->stream);
    output->channels = audio_stream_channels(output->stream);
    output->bits     = audio_stream_format(output->stream) == AUDIO_FORMAT_U8 ? 8 : 16;
    output->frame    = audio_stream_frame(output->stream);
    output->fragment = audio_stream_fragment(output->stream);
    output->step     = ((uint64_t)rate << 32) / output->rate;

    output_clear(output);

    return 0;
}


void player_output_close(player_output_t* output) {

    if (!output->stream) {
        return;
    }

    audio_stream_close(output->stream);

    output->stream = NULL;
}


void player_output_reset(player_output_t* output) {

    if (output->stream) {
        audio_stream_reset(output->stream);
    }

    output_clear(output);
}


bool player_output_active(const player_output_t* output) {
    return output->stream != NULL;
}


int player_output_fd(const player_output_t* output) {
    return output->stream ? audio_stream_fd(output->stream) : -1;
}


short player_output_events(const player_output_t* output) {
    return output->stream ? audio_stream_events(output->stream) : 0;
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

    if (!output->stream) {
        errno = EBADF;
        return -1;
    }


    for (;;) {

        const size_t left = output->output_size - output->output_done;

        if (left >= output->frame) {

            const ssize_t w = audio_stream_write(output->stream, output->output + output->output_done, left);

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

    if (!output->stream || output->frame == 0) {
        return 0;
    }


    ssize_t delay = audio_stream_delay(output->stream);

    if (delay < 0) {
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
