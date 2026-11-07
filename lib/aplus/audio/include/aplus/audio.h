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

#ifndef _APLUS_AUDIO_H
#define _APLUS_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif


#define AUDIO_SOCKET   "/var/run/aplus-audiod.sock"
#define AUDIO_DSP_PATH "/dev/dsp"

#define AUDIO_PROTOCOL_VERSION 1

#define AUDIO_FORMAT_U8     0x00000008
#define AUDIO_FORMAT_S16_LE 0x00000010

#define AUDIO_RATE_MIN     4000
#define AUDIO_RATE_MAX     192000
#define AUDIO_CHANNELS_MAX 2

#define AUDIO_NAME_MAX 32
#define AUDIO_DATA_MAX 16384


#define AUDIO_MSG_HELLO 0x0001
#define AUDIO_MSG_DATA  0x0002
#define AUDIO_MSG_RESET 0x0003

#define AUDIO_MSG_WELCOME 0x8001
#define AUDIO_MSG_STATUS  0x8002
#define AUDIO_MSG_ERROR   0x8003


typedef struct {

    uint32_t type;
    uint32_t size;

} audio_msg_header_t;


typedef struct {

    uint32_t version;
    uint32_t rate;
    uint32_t format;
    uint16_t channels;
    uint16_t reserved;

    char name[AUDIO_NAME_MAX];

} audio_msg_hello_t;


typedef struct {

    uint32_t serial;

} audio_msg_reset_t;


typedef struct {

    uint32_t version;
    uint32_t window;

} audio_msg_welcome_t;


typedef struct {

    uint32_t serial;
    uint32_t reserved;

    uint64_t consumed;
    uint64_t heard;

} audio_msg_status_t;


typedef struct {

    int32_t error;

} audio_msg_error_t;


typedef struct audio_stream audio_stream_t;


audio_stream_t* audio_stream_open(const char* name, uint32_t rate, uint16_t channels, uint32_t format);
void audio_stream_close(audio_stream_t* stream);

bool audio_stream_direct(const audio_stream_t* stream);
uint32_t audio_stream_rate(const audio_stream_t* stream);
uint16_t audio_stream_channels(const audio_stream_t* stream);
uint32_t audio_stream_format(const audio_stream_t* stream);
size_t audio_stream_frame(const audio_stream_t* stream);
size_t audio_stream_fragment(const audio_stream_t* stream);

int audio_stream_fd(const audio_stream_t* stream);
short audio_stream_events(const audio_stream_t* stream);

ssize_t audio_stream_write(audio_stream_t* stream, const void* buf, size_t size);
ssize_t audio_stream_delay(audio_stream_t* stream);
int audio_stream_reset(audio_stream_t* stream);
int audio_stream_drain(audio_stream_t* stream, int timeout);

#ifdef __cplusplus
}
#endif

#endif
