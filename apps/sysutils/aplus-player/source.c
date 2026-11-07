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
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "player.h"


#define WAVE_FORMAT_PCM        0x0001
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE

#define SOURCE_CHUNK_BYTES 8192


static uint16_t source_le16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}


static uint32_t source_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}


static ssize_t source_read_full(int fd, void* buf, size_t size) {

    size_t done = 0;

    while (done < size) {

        const ssize_t r = read(fd, (uint8_t*)buf + done, size - done);

        if (r < 0) {

            if (errno == EINTR) {
                continue;
            }

            return done ? (ssize_t)done : -1;
        }

        if (r == 0) {
            break;
        }

        done += (size_t)r;
    }

    return (ssize_t)done;
}


static bool source_pread_full(int fd, off_t offset, void* buf, size_t size) {

    if (lseek(fd, offset, SEEK_SET) < 0) {
        return false;
    }

    return source_read_full(fd, buf, size) == (ssize_t)size;
}


static int source_open_wav(player_source_t* source, off_t size, const char** error) {

    uint8_t riff[12];

    if (!source_pread_full(source->fd, 0, riff, sizeof(riff)) || memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
        *error = "Not a WAV file";
        return -1;
    }


    bool have_fmt  = false;
    bool have_data = false;

    uint16_t tag  = 0;
    uint64_t data = 0;

    off_t pos = sizeof(riff);

    while (pos + 8 <= size && !(have_fmt && have_data)) {

        uint8_t head[8];

        if (!source_pread_full(source->fd, pos, head, sizeof(head))) {
            break;
        }

        const uint32_t length = source_le32(head + 4);

        if (memcmp(head, "fmt ", 4) == 0 && length >= 16) {

            uint8_t fmt[40] = {0};

            const size_t want = length < sizeof(fmt) ? length : sizeof(fmt);

            if (source_read_full(source->fd, fmt, want) != (ssize_t)want) {
                break;
            }

            tag              = source_le16(fmt);
            source->channels = source_le16(fmt + 2);
            source->rate     = source_le32(fmt + 4);
            source->block    = source_le16(fmt + 12);
            source->bits     = source_le16(fmt + 14);

            if (tag == WAVE_FORMAT_EXTENSIBLE && want >= 26) {
                tag = source_le16(fmt + 24);
            }

            have_fmt = true;

        } else if (memcmp(head, "data", 4) == 0) {

            source->offset = pos + 8;
            data           = length;
            have_data      = true;
        }

        pos += 8 + (off_t)length + (length & 1);
    }


    if (!have_fmt || !have_data) {
        *error = "The WAV file is damaged";
        return -1;
    }

    if (tag != WAVE_FORMAT_PCM) {
        *error = "Only PCM WAV files are supported";
        return -1;
    }

    if (source->bits != 8 && source->bits != 16 && source->bits != 24 && source->bits != 32) {
        *error = "Unsupported sample size";
        return -1;
    }

    if (source->channels == 0 || source->rate == 0 || source->block < source->channels * (source->bits / 8)) {
        *error = "The WAV file is damaged";
        return -1;
    }

    if (source->offset >= size) {
        data = 0;
    } else if (data == 0 || source->offset + (off_t)data > size) {
        data = (uint64_t)(size - source->offset);
    }

    source->frames = data / source->block;

    return 0;
}


int player_source_open(player_source_t* source, const char* path, const char** error) {

    memset(source, 0, sizeof(*source));

    source->fd = open(path, O_RDONLY);

    if (source->fd < 0) {
        *error = strerror(errno);
        return -1;
    }


    struct stat st;

    if (fstat(source->fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        *error = "Not a regular file";
        player_source_close(source);
        return -1;
    }


    const char* dot = strrchr(path, '.');

    if (dot && strcasecmp(dot, ".pcm") == 0) {

        source->format   = PLAYER_FORMAT_PCM;
        source->rate     = PLAYER_PCM_RATE;
        source->channels = PLAYER_PCM_CHANNELS;
        source->bits     = PLAYER_PCM_BITS;
        source->block    = PLAYER_PCM_CHANNELS * (PLAYER_PCM_BITS / 8);
        source->offset   = 0;
        source->frames   = (uint64_t)st.st_size / source->block;

    } else {

        source->format = PLAYER_FORMAT_WAV;

        if (source_open_wav(source, st.st_size, error) < 0) {
            player_source_close(source);
            return -1;
        }
    }

    if (player_source_seek(source, 0) < 0) {
        *error = strerror(errno);
        player_source_close(source);
        return -1;
    }

    return 0;
}


void player_source_close(player_source_t* source) {

    if (source->fd >= 0) {
        close(source->fd);
    }

    source->fd = -1;
}


int player_source_seek(player_source_t* source, uint64_t frame) {

    if (frame > source->frames) {
        frame = source->frames;
    }

    if (lseek(source->fd, source->offset + (off_t)(frame * source->block), SEEK_SET) < 0) {
        return -1;
    }

    source->position = frame;

    return 0;
}


static int32_t source_sample(const uint8_t* p, uint16_t bits) {

    switch (bits) {

        case 8:
            return ((int32_t)p[0] - 128) << 8;

        case 16:
            return (int16_t)source_le16(p);

        case 24:
            return (int32_t)(((uint32_t)p[0] << 8) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 24)) >> 16;

        default:
            return (int32_t)source_le32(p) >> 16;
    }
}


size_t player_source_read(player_source_t* source, int16_t (*out)[2], size_t frames) {

    static uint8_t raw[SOURCE_CHUNK_BYTES];

    const size_t width = source->bits / 8;

    size_t total = 0;

    while (total < frames && source->position < source->frames) {

        size_t want = frames - total;

        if (want > sizeof(raw) / source->block) {
            want = sizeof(raw) / source->block;
        }

        if (want > source->frames - source->position) {
            want = (size_t)(source->frames - source->position);
        }


        const ssize_t got = source_read_full(source->fd, raw, want * source->block);

        if (got <= 0) {
            source->frames = source->position;
            break;
        }


        const size_t count = (size_t)got / source->block;

        for (size_t i = 0; i < count; i++) {

            const uint8_t* frame = raw + i * source->block;

            const int32_t left  = source_sample(frame, source->bits);
            const int32_t right = source->channels > 1 ? source_sample(frame + width, source->bits) : left;

            out[total + i][0] = (int16_t)left;
            out[total + i][1] = (int16_t)right;
        }

        total += count;
        source->position += count;

        if (count < want) {
            source->frames = source->position;
            break;
        }
    }

    return total;
}
