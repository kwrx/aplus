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

#ifndef _APLUS_SOUNDCARD_H
#define _APLUS_SOUNDCARD_H

#ifndef __ASSEMBLY__

    #include <stdint.h>
    #include <sys/ioctl.h>

    #define AFMT_QUERY     0x00000000
    #define AFMT_MU_LAW    0x00000001
    #define AFMT_A_LAW     0x00000002
    #define AFMT_IMA_ADPCM 0x00000004
    #define AFMT_U8        0x00000008
    #define AFMT_S16_LE    0x00000010
    #define AFMT_S16_BE    0x00000020
    #define AFMT_S8        0x00000040
    #define AFMT_U16_LE    0x00000080
    #define AFMT_U16_BE    0x00000100
    #define AFMT_MPEG      0x00000200
    #define AFMT_AC3       0x00000400
    #define AFMT_S32_LE    0x00001000
    #define AFMT_S32_BE    0x00002000

    #define SNDCTL_DSP_RESET       _IO('P', 0)
    #define SNDCTL_DSP_SYNC        _IO('P', 1)
    #define SNDCTL_DSP_SPEED       _IOWR('P', 2, int)
    #define SNDCTL_DSP_STEREO      _IOWR('P', 3, int)
    #define SNDCTL_DSP_GETBLKSIZE  _IOWR('P', 4, int)
    #define SNDCTL_DSP_SETFMT      _IOWR('P', 5, int)
    #define SNDCTL_DSP_CHANNELS    _IOWR('P', 6, int)
    #define SNDCTL_DSP_POST        _IO('P', 8)
    #define SNDCTL_DSP_SUBDIVIDE   _IOWR('P', 9, int)
    #define SNDCTL_DSP_SETFRAGMENT _IOWR('P', 10, int)
    #define SNDCTL_DSP_GETFMTS     _IOR('P', 11, int)
    #define SNDCTL_DSP_GETOSPACE   _IOR('P', 12, struct audio_buf_info)
    #define SNDCTL_DSP_GETISPACE   _IOR('P', 13, struct audio_buf_info)
    #define SNDCTL_DSP_NONBLOCK    _IO('P', 14)
    #define SNDCTL_DSP_GETCAPS     _IOR('P', 15, int)
    #define SNDCTL_DSP_GETTRIGGER  _IOR('P', 16, int)
    #define SNDCTL_DSP_SETTRIGGER  _IOW('P', 16, int)
    #define SNDCTL_DSP_GETODELAY   _IOR('P', 23, int)
    #define SNDCTL_DSP_SETDUPLEX   _IO('P', 22)

    #define SOUND_PCM_READ_RATE     _IOR('P', 2, int)
    #define SOUND_PCM_READ_CHANNELS _IOR('P', 6, int)
    #define SOUND_PCM_READ_BITS     _IOR('P', 5, int)

    #define DSP_CAP_REVISION 0x000000ff
    #define DSP_CAP_DUPLEX   0x00000100
    #define DSP_CAP_REALTIME 0x00000200
    #define DSP_CAP_BATCH    0x00000400
    #define DSP_CAP_COPROC   0x00000800
    #define DSP_CAP_TRIGGER  0x00001000
    #define DSP_CAP_MMAP     0x00002000
    #define DSP_CAP_INPUT    0x00010000
    #define DSP_CAP_OUTPUT   0x00020000

    #define PCM_ENABLE_INPUT  0x00000001
    #define PCM_ENABLE_OUTPUT 0x00000002

    #define SOUND_MIXER_NRDEVICES 25

    #define SOUND_MIXER_VOLUME  0
    #define SOUND_MIXER_BASS    1
    #define SOUND_MIXER_TREBLE  2
    #define SOUND_MIXER_SYNTH   3
    #define SOUND_MIXER_PCM     4
    #define SOUND_MIXER_SPEAKER 5
    #define SOUND_MIXER_LINE    6
    #define SOUND_MIXER_MIC     7
    #define SOUND_MIXER_CD      8
    #define SOUND_MIXER_IMIX    9
    #define SOUND_MIXER_ALTPCM  10
    #define SOUND_MIXER_RECLEV  11
    #define SOUND_MIXER_IGAIN   12
    #define SOUND_MIXER_OGAIN   13
    #define SOUND_MIXER_LINE1   14
    #define SOUND_MIXER_LINE2   15
    #define SOUND_MIXER_LINE3   16
    #define SOUND_MIXER_PHONEIN 17
    #define SOUND_MIXER_VIDEO   18
    #define SOUND_MIXER_RADIO   19
    #define SOUND_MIXER_MONITOR 20

    #define SOUND_MIXER_NONE       31
    #define SOUND_MIXER_STEREODEVS 0xfb
    #define SOUND_MIXER_CAPS       0xfc
    #define SOUND_MIXER_RECMASK    0xfd
    #define SOUND_MIXER_DEVMASK    0xfe
    #define SOUND_MIXER_RECSRC     0xff

    #define SOUND_CAP_EXCL_INPUT 0x00000001

    #define SOUND_MIXER_READ(dev)  _IOR('M', dev, int)
    #define SOUND_MIXER_WRITE(dev) _IOWR('M', dev, int)

    #define SOUND_MIXER_READ_VOLUME     SOUND_MIXER_READ(SOUND_MIXER_VOLUME)
    #define SOUND_MIXER_READ_PCM        SOUND_MIXER_READ(SOUND_MIXER_PCM)
    #define SOUND_MIXER_READ_LINE       SOUND_MIXER_READ(SOUND_MIXER_LINE)
    #define SOUND_MIXER_READ_MIC        SOUND_MIXER_READ(SOUND_MIXER_MIC)
    #define SOUND_MIXER_READ_IGAIN      SOUND_MIXER_READ(SOUND_MIXER_IGAIN)
    #define SOUND_MIXER_READ_DEVMASK    SOUND_MIXER_READ(SOUND_MIXER_DEVMASK)
    #define SOUND_MIXER_READ_STEREODEVS SOUND_MIXER_READ(SOUND_MIXER_STEREODEVS)
    #define SOUND_MIXER_READ_RECMASK    SOUND_MIXER_READ(SOUND_MIXER_RECMASK)
    #define SOUND_MIXER_READ_RECSRC     SOUND_MIXER_READ(SOUND_MIXER_RECSRC)
    #define SOUND_MIXER_READ_CAPS       SOUND_MIXER_READ(SOUND_MIXER_CAPS)

    #define SOUND_MIXER_INFO _IOR('M', 101, mixer_info)

    #define SOUND_MIXER_WRITE_VOLUME SOUND_MIXER_WRITE(SOUND_MIXER_VOLUME)
    #define SOUND_MIXER_WRITE_PCM    SOUND_MIXER_WRITE(SOUND_MIXER_PCM)
    #define SOUND_MIXER_WRITE_LINE   SOUND_MIXER_WRITE(SOUND_MIXER_LINE)
    #define SOUND_MIXER_WRITE_MIC    SOUND_MIXER_WRITE(SOUND_MIXER_MIC)
    #define SOUND_MIXER_WRITE_IGAIN  SOUND_MIXER_WRITE(SOUND_MIXER_IGAIN)
    #define SOUND_MIXER_WRITE_RECSRC SOUND_MIXER_WRITE(SOUND_MIXER_RECSRC)

    #define SOUND_MASK_VOLUME  (1 << SOUND_MIXER_VOLUME)
    #define SOUND_MASK_PCM     (1 << SOUND_MIXER_PCM)
    #define SOUND_MASK_LINE    (1 << SOUND_MIXER_LINE)
    #define SOUND_MASK_MIC     (1 << SOUND_MIXER_MIC)
    #define SOUND_MASK_CD      (1 << SOUND_MIXER_CD)
    #define SOUND_MASK_IGAIN   (1 << SOUND_MIXER_IGAIN)
    #define SOUND_MASK_OGAIN   (1 << SOUND_MIXER_OGAIN)
    #define SOUND_MASK_SPEAKER (1 << SOUND_MIXER_SPEAKER)

struct audio_buf_info {

    int fragments;
    int fragstotal;
    int fragsize;
    int bytes;
};

struct count_info {

    int bytes;
    int blocks;
    int ptr;
};

typedef struct mixer_info {

    char id[16];
    char name[32];
    int modify_counter;
    int fillers[10];

} mixer_info;

#endif
#endif
