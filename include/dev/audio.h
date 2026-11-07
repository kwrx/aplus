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

#ifndef _DEV_AUDIO_H
#define _DEV_AUDIO_H

#ifndef __ASSEMBLY__


    #include <aplus.h>
    #include <aplus/debug.h>
    #include <aplus/soundcard.h>
    #include <aplus/syscall.h>
    #include <stdint.h>

    #include <dev/interface.h>


    #define AUDIO_MAX_CARDS 8

    #define AUDIO_MAJOR     14
    #define AUDIO_MINOR_DSP 3
    #define AUDIO_MINOR_MIX 0

    #define AUDIO_DEFAULT_RATE      48000
    #define AUDIO_DEFAULT_CHANNELS  2
    #define AUDIO_DEFAULT_FORMAT    AFMT_S16_LE
    #define AUDIO_DEFAULT_FRAGSIZE  4096
    #define AUDIO_DEFAULT_FRAGCOUNT 4

    #define AUDIO_IOC_NR(r)   ((uint32_t)((unsigned long)(r)) & 0xFF)
    #define AUDIO_IOC_TYPE(r) (((uint32_t)((unsigned long)(r)) >> 8) & 0xFF)
    #define AUDIO_IOC_DIR(r)  (((uint32_t)((unsigned long)(r)) >> 30) & 0x3)

    #define AUDIO_IOC_DIR_NONE  0
    #define AUDIO_IOC_DIR_WRITE 1
    #define AUDIO_IOC_DIR_READ  2

    #define AUDIO_IOC_TYPE_DSP   'P'
    #define AUDIO_IOC_TYPE_MIXER 'M'

    #define AUDIO_MIXER_LEVEL(l, r) (((uint32_t)((r) & 0xFF) << 8) | ((uint32_t)((l) & 0xFF)))
    #define AUDIO_MIXER_LEFT(v)     ((uint32_t)(v) & 0xFF)
    #define AUDIO_MIXER_RIGHT(v)    (((uint32_t)(v) >> 8) & 0xFF)


__BEGIN_DECLS

int audio_getattr(device_t*, struct stat*);
int audio_open(device_t*, int);
int audio_close(device_t*);
ssize_t audio_write(device_t*, const void*, size_t);
ssize_t audio_read(device_t*, void*, size_t);
int audio_ioctl(device_t*, long, void*);
int audio_poll(device_t*, int);
int audio_fsync(device_t*);
void audio_init(device_t*);
void audio_dnit(device_t*);

void audio_notify(device_t*);

int mixer_getattr(device_t*, struct stat*);
int mixer_ioctl(device_t*, long, void*);
void mixer_init(device_t*);
void mixer_dnit(device_t*);

int audio_format_bits(uint32_t);
size_t audio_frame_size(device_t*);

__END_DECLS


#endif
#endif
