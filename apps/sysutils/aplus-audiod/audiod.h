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

#ifndef _APLUS_AUDIOD_H
#define _APLUS_AUDIOD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <aplus/audio.h>


#define AUDIOD_NAME "aplus-audiod"

#define AUDIOD_CLIENTS_MAX 32

#define AUDIOD_OUTPUT_RATE     48000
#define AUDIOD_OUTPUT_CHANNELS 2

#define AUDIOD_FRAGMENT_FRAMES 1024
#define AUDIOD_LOW_FRAGMENTS   2
#define AUDIOD_HIGH_FRAGMENTS  4

#define AUDIOD_WINDOW_MS    200
#define AUDIOD_PREBUFFER_MS 20
#define AUDIOD_IDLE_MS      2000

#define AUDIOD_MARKS_MAX  64
#define AUDIOD_INBOX_SIZE (sizeof(audio_msg_header_t) + AUDIO_DATA_MAX)
#define AUDIOD_OUTBOX_SIZE 128


typedef struct {

    uint64_t position;
    uint64_t consumed;

} audiod_mark_t;


typedef struct {

    int fd;
    bool ready;

    char name[AUDIO_NAME_MAX];

    uint32_t rate;
    uint16_t channels;
    uint32_t format;
    size_t frame;

    uint32_t serial;

    uint8_t* ring;
    size_t ring_capacity;
    size_t ring_head;
    size_t ring_count;
    size_t prebuffer;

    uint8_t inbox[AUDIOD_INBOX_SIZE];
    size_t inbox_size;

    uint8_t outbox[AUDIOD_OUTBOX_SIZE];
    size_t outbox_size;

    bool started;
    bool primed;
    int64_t last_data;

    uint64_t step;
    uint64_t phase;
    int32_t current[2];
    int32_t next[2];

    uint64_t consumed;
    uint64_t heard;

    bool reported;
    uint32_t reported_serial;
    uint64_t reported_consumed;
    uint64_t reported_heard;

    audiod_mark_t marks[AUDIOD_MARKS_MAX];
    size_t marks_head;
    size_t marks_count;

} audiod_client_t;


typedef struct {

    const char* path;
    int fd;

    uint32_t rate;
    uint16_t channels;
    uint32_t format;
    size_t frame;
    size_t fragment;

    uint64_t written;
    int64_t idle_since;

    int32_t* mix;
    uint8_t* buffer;
    size_t pending;
    size_t pending_done;

} audiod_output_t;


typedef struct {

    bool running;

    int listener;

    audiod_client_t* clients[AUDIOD_CLIENTS_MAX];
    size_t count;

    audiod_output_t output;

} audiod_t;


extern audiod_t audiod;


int64_t audiod_now_ms(void);
void audiod_drop_all(int error);

audiod_client_t* audiod_client_create(int fd);
void audiod_client_destroy(audiod_client_t* client);
int audiod_client_receive(audiod_client_t* client);
int audiod_client_flush(audiod_client_t* client);
void audiod_client_report(audiod_client_t* client);
void audiod_client_fail(audiod_client_t* client, int error);
bool audiod_client_playable(audiod_client_t* client, int64_t now);
int audiod_client_wait(const audiod_client_t* client, int64_t now);
bool audiod_client_pop(audiod_client_t* client, int32_t frame[2]);
void audiod_client_mark(audiod_client_t* client, uint64_t position);
void audiod_client_played(audiod_client_t* client, uint64_t played);

int audiod_output_open(audiod_output_t* output);
void audiod_output_close(audiod_output_t* output);
int audiod_output_service(audiod_output_t* output);

#endif
