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

#ifndef _APLUS_PLAYER_H
#define _APLUS_PLAYER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>


#define PLAYER_DSP_PATH "/dev/dsp"

#define PLAYER_PCM_RATE     44100
#define PLAYER_PCM_CHANNELS 2
#define PLAYER_PCM_BITS     16

#define PLAYER_INPUT_FRAMES  2048
#define PLAYER_OUTPUT_FRAMES 1024
#define PLAYER_OUTPUT_BYTES  (PLAYER_OUTPUT_FRAMES * 2 * 2)

#define PLAYER_GAIN_UNITY 65536


typedef enum {

    PLAYER_FORMAT_WAV = 0,
    PLAYER_FORMAT_PCM,

} player_format_t;


typedef struct {

    int fd;

    player_format_t format;

    uint32_t rate;
    uint16_t channels;
    uint16_t bits;
    uint16_t block;

    off_t offset;

    uint64_t frames;
    uint64_t position;

} player_source_t;


typedef struct {

    int fd;

    uint32_t rate;
    uint16_t channels;
    uint16_t bits;

    size_t frame;
    size_t fragment;

    uint64_t step;
    uint64_t phase;

    int16_t input[PLAYER_INPUT_FRAMES][2];
    size_t input_count;

    uint8_t output[PLAYER_OUTPUT_BYTES];
    size_t output_size;
    size_t output_done;
    bool output_silent;

    bool finished;
    size_t padding;

    uint64_t written;
    uint64_t silence;

} player_output_t;


int player_source_open(player_source_t* source, const char* path, const char** error);
void player_source_close(player_source_t* source);
int player_source_seek(player_source_t* source, uint64_t frame);
size_t player_source_read(player_source_t* source, int16_t (*out)[2], size_t frames);

int player_output_open(player_output_t* output, uint32_t rate, const char** error);
void player_output_close(player_output_t* output);
void player_output_reset(player_output_t* output);
int player_output_fill(player_output_t* output, player_source_t* source, uint32_t gain);
uint64_t player_output_heard(player_output_t* output);
bool player_output_done(player_output_t* output);
bool player_output_wants(const player_output_t* output);

#endif
