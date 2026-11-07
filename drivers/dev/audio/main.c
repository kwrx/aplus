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

#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysmacros.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <aplus/soundcard.h>
#include <aplus/vfs.h>

#include <dev/audio.h>
#include <dev/interface.h>



MODULE_NAME("dev/audio");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



static uint16_t num_cards = 0;



int audio_format_bits(uint32_t format) {

    switch (format) {

        case AFMT_MU_LAW:
        case AFMT_A_LAW:
        case AFMT_IMA_ADPCM:
        case AFMT_U8:
        case AFMT_S8:
            return 8;

        case AFMT_S16_LE:
        case AFMT_S16_BE:
        case AFMT_U16_LE:
        case AFMT_U16_BE:
            return 16;

        case AFMT_S32_LE:
        case AFMT_S32_BE:
            return 32;

        default:
            return 0;
    }
}


size_t audio_frame_size(device_t* device) {

    DEBUG_ASSERT(device);


    int bits = audio_format_bits(device->snd.params.format);

    if (unlikely(bits <= 0)) {
        bits = 8;
    }

    size_t channels = device->snd.params.channels ? device->snd.params.channels : 1;

    return (size_t)(bits >> 3) * channels;
}


void audio_notify(device_t* device) {

    DEBUG_ASSERT(device);

    vfs_notify(device->inode);
}



static inline size_t __audio_ilog2(size_t value) {

    size_t e = 0;

    while (value > 1) {
        value >>= 1;
        e++;
    }

    return e;
}


static int __audio_configure(device_t* device) {

    DEBUG_ASSERT(device);

    if (device->snd.configured) {
        return 0;
    }

    if (likely(device->snd.configure)) {

        int e;

        if ((e = device->snd.configure(device)) < 0) {
            return e;
        }
    }

    device->snd.configured = 1;

    return 0;
}



int audio_getattr(device_t* device, struct stat* st) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(st);

    st->st_dev     = makedev(device->major, device->minor);
    st->st_ino     = device->inode->ino;
    st->st_mode    = S_IFCHR | 0666;
    st->st_nlink   = 1;
    st->st_uid     = 0;
    st->st_gid     = 0;
    st->st_rdev    = makedev(device->major, device->minor);
    st->st_size    = 0;
    st->st_blksize = (blksize_t)device->snd.fragsize;
    st->st_blocks  = 0;
    st->st_atime   = arch_timer_gettime();
    st->st_mtime   = arch_timer_gettime();
    st->st_ctime   = arch_timer_gettime();

    return 0;
}


int audio_open(device_t* device, int flags) {

    DEBUG_ASSERT(device);

    if (device->snd.opened) {
        return -EBUSY;
    }


    int access = flags & O_ACCMODE;

    if (access == O_RDONLY && !device->snd.input) {
        return -EACCES;
    }

    if (access == O_WRONLY && !device->snd.output) {
        return -EACCES;
    }

    if (access == O_RDWR && !(device->snd.caps & DSP_CAP_DUPLEX)) {
        return -EACCES;
    }


    device->snd.params.rate     = MIN(AUDIO_DEFAULT_RATE, device->snd.maxrate);
    device->snd.params.channels = MIN(AUDIO_DEFAULT_CHANNELS, device->snd.maxchannels);
    device->snd.params.format   = (device->snd.formats & AUDIO_DEFAULT_FORMAT) ? AUDIO_DEFAULT_FORMAT : AFMT_U8;

    device->snd.trigger_mask = PCM_ENABLE_OUTPUT | PCM_ENABLE_INPUT;
    device->snd.configured   = 0;
    device->snd.opened       = 1;

    return 0;
}


int audio_close(device_t* device) {

    DEBUG_ASSERT(device);

    if (!device->snd.opened) {
        return 0;
    }


    if (likely(device->snd.drain)) {
        device->snd.drain(device);
    }

    if (likely(device->snd.trigger)) {
        device->snd.trigger(device, 0);
    }

    device->snd.trigger_mask = 0;
    device->snd.configured   = 0;
    device->snd.opened       = 0;

    return 0;
}


ssize_t audio_write(device_t* device, const void* buf, size_t size) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);

    if (unlikely(!device->snd.output)) {
        return -EACCES;
    }

    DEBUG_ASSERT(device->snd.bounce);
    DEBUG_ASSERT(device->snd.bouncesize);


    int e;

    if (unlikely((e = __audio_configure(device)) < 0)) {
        return e;
    }


    size_t frame = audio_frame_size(device);

    size -= size % frame;

    if (unlikely(!size)) {
        return -EINVAL;
    }


    size_t done = 0;

    while (done < size) {

        size_t chunk = MIN(size - done, device->snd.bouncesize);

        chunk -= chunk % frame;

        if (unlikely(!chunk)) {
            break;
        }


        memcpy(device->snd.bounce, (const uint8_t*)buf + done, chunk);

        ssize_t r = device->snd.output(device, device->snd.bounce, chunk);

        if (r < 0) {

            if (done) {
                break;
            }

            return r;
        }

        done += (size_t)r;

        if ((size_t)r < chunk) {
            break;
        }
    }


    if (unlikely(!done)) {
        return -EAGAIN;
    }

    return (ssize_t)done;
}


ssize_t audio_read(device_t* device, void* buf, size_t size) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);

    if (unlikely(!device->snd.input)) {
        return -EACCES;
    }

    DEBUG_ASSERT(device->snd.bounce);
    DEBUG_ASSERT(device->snd.bouncesize);


    int e;

    if (unlikely((e = __audio_configure(device)) < 0)) {
        return e;
    }


    size_t frame = audio_frame_size(device);

    size -= size % frame;

    if (unlikely(!size)) {
        return -EINVAL;
    }


    size_t done = 0;

    while (done < size) {

        size_t chunk = MIN(size - done, device->snd.bouncesize);

        chunk -= chunk % frame;

        if (unlikely(!chunk)) {
            break;
        }


        ssize_t r = device->snd.input(device, device->snd.bounce, chunk);

        if (r < 0) {

            if (done) {
                break;
            }

            return r;
        }

        if (r == 0) {
            break;
        }

        memcpy((uint8_t*)buf + done, device->snd.bounce, (size_t)r);

        done += (size_t)r;

        if ((size_t)r < chunk) {
            break;
        }
    }


    if (unlikely(!done)) {
        return -EAGAIN;
    }

    return (ssize_t)done;
}


int audio_poll(device_t* device, int events) {

    DEBUG_ASSERT(device);


    int revents = 0;

    ssize_t frame = (ssize_t)audio_frame_size(device);


    if (device->snd.output) {

        if (!device->snd.configured || !device->snd.ospace) {

            revents |= POLLOUT | POLLWRNORM;

        } else if (device->snd.ospace(device) >= frame) {

            revents |= POLLOUT | POLLWRNORM;
        }
    }

    if (device->snd.input) {

        if (device->snd.ispace && device->snd.ispace(device) >= frame) {
            revents |= POLLIN | POLLRDNORM;
        }
    }

    return revents & events;
}


int audio_fsync(device_t* device) {

    DEBUG_ASSERT(device);

    if (likely(device->snd.drain)) {

        int e;

        if ((e = device->snd.drain(device)) < 0) {
            return errno = -e, -1;
        }
    }

    return 0;
}


int audio_ioctl(device_t* device, long req, void* arg) {

    DEBUG_ASSERT(device);

    if (unlikely(AUDIO_IOC_TYPE(req) != AUDIO_IOC_TYPE_DSP)) {
        return errno = EINVAL, -1;
    }


    uint32_t nr  = AUDIO_IOC_NR(req);
    uint32_t dir = AUDIO_IOC_DIR(req);

    int e;


    switch (nr) {

        case 0: {

            if (likely(device->snd.flush)) {

                if ((e = device->snd.flush(device)) < 0) {
                    return errno = -e, -1;
                }
            }

            device->snd.configured = 0;
            return 0;
        }

        case 1: {

            if (likely(device->snd.drain)) {

                if ((e = device->snd.drain(device)) < 0) {
                    return errno = -e, -1;
                }
            }

            return 0;
        }

        case 8: {

            if (likely(device->snd.trigger)) {

                if ((e = device->snd.trigger(device, device->snd.trigger_mask)) < 0) {
                    return errno = -e, -1;
                }
            }

            return 0;
        }

        case 14:
            return 0;

        case 22: {

            if (!(device->snd.caps & DSP_CAP_DUPLEX)) {
                return errno = EINVAL, -1;
            }

            return 0;
        }

        default:
            break;
    }


    if (unlikely(!arg)) {
        return errno = EFAULT, -1;
    }

    if (unlikely(!uio_check(arg, R_OK | W_OK))) {
        return errno = EFAULT, -1;
    }


    switch (nr) {

        case 2: {

            if (dir & AUDIO_IOC_DIR_WRITE) {

                uint32_t rate = uio_r32((uint32_t*)arg);

                if (rate < device->snd.minrate) {
                    rate = device->snd.minrate;
                }

                if (rate > device->snd.maxrate) {
                    rate = device->snd.maxrate;
                }

                if (rate != device->snd.params.rate) {

                    device->snd.params.rate = rate;
                    device->snd.configured  = 0;
                }

                if ((e = __audio_configure(device)) < 0) {
                    return errno = -e, -1;
                }
            }

            uio_w32((uint32_t*)arg, device->snd.params.rate);
            return 0;
        }

        case 3: {

            if (dir & AUDIO_IOC_DIR_WRITE) {

                uint16_t channels = uio_r32((uint32_t*)arg) ? 2 : 1;

                if (channels > device->snd.maxchannels) {
                    channels = device->snd.maxchannels;
                }

                if (channels != device->snd.params.channels) {

                    device->snd.params.channels = channels;
                    device->snd.configured      = 0;
                }

                if ((e = __audio_configure(device)) < 0) {
                    return errno = -e, -1;
                }
            }

            uio_w32((uint32_t*)arg, device->snd.params.channels > 1 ? 1 : 0);
            return 0;
        }

        case 4: {

            uio_w32((uint32_t*)arg, (uint32_t)device->snd.fragsize);
            return 0;
        }

        case 5: {

            if (dir == AUDIO_IOC_DIR_READ) {

                uio_w32((uint32_t*)arg, (uint32_t)audio_format_bits(device->snd.params.format));
                return 0;
            }


            uint32_t format = uio_r32((uint32_t*)arg);

            if (format != AFMT_QUERY && (device->snd.formats & format)) {

                if (format != device->snd.params.format) {

                    device->snd.params.format = format;
                    device->snd.configured    = 0;
                }

                if ((e = __audio_configure(device)) < 0) {
                    return errno = -e, -1;
                }
            }

            uio_w32((uint32_t*)arg, device->snd.params.format);
            return 0;
        }

        case 6: {

            if (dir & AUDIO_IOC_DIR_WRITE) {

                uint32_t channels = uio_r32((uint32_t*)arg);

                if (channels == 0) {
                    channels = 1;
                }

                if (channels > device->snd.maxchannels) {
                    channels = device->snd.maxchannels;
                }

                if ((uint16_t)channels != device->snd.params.channels) {

                    device->snd.params.channels = (uint16_t)channels;
                    device->snd.configured      = 0;
                }

                if ((e = __audio_configure(device)) < 0) {
                    return errno = -e, -1;
                }
            }

            uio_w32((uint32_t*)arg, device->snd.params.channels);
            return 0;
        }

        case 9: {

            uio_w32((uint32_t*)arg, 1);
            return 0;
        }

        case 10: {

            uio_w32((uint32_t*)arg, (uint32_t)((device->snd.fragcount << 16) | __audio_ilog2(device->snd.fragsize)));
            return 0;
        }

        case 11: {

            uio_w32((uint32_t*)arg, device->snd.formats);
            return 0;
        }

        case 12:
        case 13: {

            ssize_t space = 0;

            if (nr == 12) {

                if (unlikely(!device->snd.output)) {
                    return errno = EINVAL, -1;
                }

                if (device->snd.ospace) {
                    space = device->snd.ospace(device);
                } else {
                    space = (ssize_t)(device->snd.fragsize * device->snd.fragcount);
                }

            } else {

                if (unlikely(!device->snd.input)) {
                    return errno = EINVAL, -1;
                }

                if (device->snd.ispace) {
                    space = device->snd.ispace(device);
                }
            }

            if (space < 0) {
                space = 0;
            }


            struct audio_buf_info info;

            info.fragsize   = (int)device->snd.fragsize;
            info.fragstotal = (int)device->snd.fragcount;
            info.bytes      = (int)space;
            info.fragments  = (int)((size_t)space / device->snd.fragsize);

            uio_memcpy_s2u(arg, &info, sizeof(info));
            return 0;
        }

        case 15: {

            uio_w32((uint32_t*)arg, device->snd.caps);
            return 0;
        }

        case 16: {

            if (dir & AUDIO_IOC_DIR_WRITE) {

                int trigger = (int)uio_r32((uint32_t*)arg);

                if (likely(device->snd.trigger)) {

                    if ((e = device->snd.trigger(device, trigger)) < 0) {
                        return errno = -e, -1;
                    }
                }

                device->snd.trigger_mask = trigger;
                return 0;
            }

            uio_w32((uint32_t*)arg, (uint32_t)device->snd.trigger_mask);
            return 0;
        }

        case 23: {

            ssize_t delay = 0;

            if (likely(device->snd.odelay)) {
                delay = device->snd.odelay(device);
            }

            if (delay < 0) {
                delay = 0;
            }

            uio_w32((uint32_t*)arg, (uint32_t)delay);
            return 0;
        }

        default:
            break;
    }

    return errno = EINVAL, -1;
}


void audio_init(device_t* device) {

    DEBUG_ASSERT(device);

    if (unlikely(!device->snd.output && !device->snd.input)) {
        kpanicf("audio_init: PANIC! no pcm endpoint declared for '%s'\n", device->name);
    }

    if (unlikely(num_cards >= AUDIO_MAX_CARDS)) {
        kpanicf("audio_init: PANIC! too many audio cards, max is %d\n", AUDIO_MAX_CARDS);
    }


    device->snd.index = num_cards++;

    if (!device->snd.formats) {
        device->snd.formats = AFMT_U8 | AFMT_S16_LE;
    }

    if (!device->snd.minrate) {
        device->snd.minrate = 8000;
    }

    if (!device->snd.maxrate) {
        device->snd.maxrate = AUDIO_DEFAULT_RATE;
    }

    if (!device->snd.maxchannels) {
        device->snd.maxchannels = AUDIO_DEFAULT_CHANNELS;
    }

    if (!device->snd.fragsize) {
        device->snd.fragsize = AUDIO_DEFAULT_FRAGSIZE;
    }

    if (!device->snd.fragcount) {
        device->snd.fragcount = AUDIO_DEFAULT_FRAGCOUNT;
    }

    if (device->snd.output) {
        device->snd.caps |= DSP_CAP_OUTPUT;
    }

    if (device->snd.input) {
        device->snd.caps |= DSP_CAP_INPUT;
    }

    if (device->snd.output && device->snd.input) {
        device->snd.caps |= DSP_CAP_DUPLEX;
    }

    if (device->snd.trigger) {
        device->snd.caps |= DSP_CAP_TRIGGER;
    }

    device->snd.params.rate     = MIN(AUDIO_DEFAULT_RATE, device->snd.maxrate);
    device->snd.params.channels = MIN(AUDIO_DEFAULT_CHANNELS, device->snd.maxchannels);
    device->snd.params.format   = (device->snd.formats & AUDIO_DEFAULT_FORMAT) ? AUDIO_DEFAULT_FORMAT : AFMT_U8;

    device->snd.bouncesize = MAX(device->snd.fragsize, (size_t)AUDIO_DEFAULT_FRAGSIZE);
    device->snd.bounce     = (uint8_t*)kmalloc(device->snd.bouncesize, GFP_KERNEL);

    if (unlikely(!device->snd.bounce)) {
        kpanicf("audio_init: PANIC! failed to allocate the pcm buffer for '%s'\n", device->name);
    }

    device->snd.opened     = 0;
    device->snd.configured = 0;


    if (device->snd.index == 0) {
        strncpy(device->name, "dsp", DEVICE_MAXNAMELEN - 1);
    } else {
        snprintf(device->name, DEVICE_MAXNAMELEN, "dsp%d", device->snd.index);
    }

    if (!device->major) {
        device->major = AUDIO_MAJOR;
        device->minor = (uint16_t)(device->snd.index * 16 + AUDIO_MINOR_DSP);
    }
}


void audio_dnit(device_t* device) {

    DEBUG_ASSERT(device);

    if (likely(device->snd.drain)) {
        device->snd.drain(device);
    }

    if (likely(device->snd.trigger)) {
        device->snd.trigger(device, 0);
    }

    if (likely(device->snd.bounce)) {

        kfree(device->snd.bounce);
        device->snd.bounce = NULL;
    }

    device->snd.opened     = 0;
    device->snd.configured = 0;
}


int mixer_getattr(device_t* device, struct stat* st) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(st);

    st->st_dev     = makedev(device->major, device->minor);
    st->st_ino     = device->inode->ino;
    st->st_mode    = S_IFCHR | 0666;
    st->st_nlink   = 1;
    st->st_uid     = 0;
    st->st_gid     = 0;
    st->st_rdev    = makedev(device->major, device->minor);
    st->st_size    = 0;
    st->st_blksize = 0;
    st->st_blocks  = 0;
    st->st_atime   = arch_timer_gettime();
    st->st_mtime   = arch_timer_gettime();
    st->st_ctime   = arch_timer_gettime();

    return 0;
}


int mixer_ioctl(device_t* device, long req, void* arg) {

    DEBUG_ASSERT(device);

    if (unlikely(AUDIO_IOC_TYPE(req) != AUDIO_IOC_TYPE_MIXER)) {
        return errno = EINVAL, -1;
    }

    if (unlikely(!arg)) {
        return errno = EFAULT, -1;
    }

    if (unlikely(!uio_check(arg, R_OK | W_OK))) {
        return errno = EFAULT, -1;
    }


    device_t* card = device->mix.card;

    if (unlikely(!card)) {
        return errno = ENODEV, -1;
    }


    int nr      = (int)AUDIO_IOC_NR(req);
    uint32_t v  = 0;
    int e       = 0;
    bool setter = !!(AUDIO_IOC_DIR(req) & AUDIO_IOC_DIR_WRITE);


    switch (nr) {

        case AUDIO_IOC_NR(SOUND_MIXER_INFO): {

            if (unlikely(!uio_check((uint8_t*)arg + sizeof(mixer_info) - 1, R_OK | W_OK))) {
                return errno = EFAULT, -1;
            }

            mixer_info info = {0};

            strncpy(info.id, card->name, sizeof(info.id) - 1);
            strncpy(info.name, device->description[0] ? device->description : card->description, sizeof(info.name) - 1);

            uio_memcpy_s2u(arg, &info, sizeof(info));
            return 0;
        }

        case SOUND_MIXER_DEVMASK:

            uio_w32((uint32_t*)arg, device->mix.devmask);
            return 0;

        case SOUND_MIXER_STEREODEVS:

            uio_w32((uint32_t*)arg, device->mix.stereomask);
            return 0;

        case SOUND_MIXER_RECMASK:

            uio_w32((uint32_t*)arg, device->mix.recmask);
            return 0;

        case SOUND_MIXER_CAPS:

            uio_w32((uint32_t*)arg, device->mix.recmask ? SOUND_CAP_EXCL_INPUT : 0);
            return 0;

        case SOUND_MIXER_RECSRC: {

            if (setter) {

                v = uio_r32((uint32_t*)arg) & device->mix.recmask;

                if (likely(device->mix.set)) {

                    if ((e = device->mix.set(card, nr, &v)) < 0) {
                        return errno = -e, -1;
                    }
                }

                device->mix.recsrc = v;
            }

            uio_w32((uint32_t*)arg, device->mix.recsrc);
            return 0;
        }

        default:
            break;
    }


    if (unlikely(nr >= SOUND_MIXER_NRDEVICES)) {
        return errno = EINVAL, -1;
    }

    if (unlikely(!(device->mix.devmask & (1u << nr)))) {
        return errno = EINVAL, -1;
    }


    if (setter) {

        if (unlikely(!device->mix.set)) {
            return errno = EINVAL, -1;
        }

        v = uio_r32((uint32_t*)arg);

        if (AUDIO_MIXER_LEFT(v) > 100 || AUDIO_MIXER_RIGHT(v) > 100) {
            return errno = EINVAL, -1;
        }

        if ((e = device->mix.set(card, nr, &v)) < 0) {
            return errno = -e, -1;
        }

        uio_w32((uint32_t*)arg, v);
        return 0;
    }


    if (unlikely(!device->mix.get)) {
        return errno = EINVAL, -1;
    }

    if ((e = device->mix.get(card, nr, &v)) < 0) {
        return errno = -e, -1;
    }

    uio_w32((uint32_t*)arg, v);
    return 0;
}


void mixer_init(device_t* device) {

    DEBUG_ASSERT(device);

    device_t* card = device->mix.card;

    if (unlikely(!card || card->type != DEVICE_TYPE_AUDIO)) {
        kpanicf("mixer_init: PANIC! no audio card bound to '%s'\n", device->name);
    }

    DEBUG_ASSERT(card->snd.index < AUDIO_MAX_CARDS);


    if (!device->name[0]) {

        if (card->snd.index == 0) {
            strncpy(device->name, "mixer", DEVICE_MAXNAMELEN - 1);
        } else {
            snprintf(device->name, DEVICE_MAXNAMELEN, "mixer%d", card->snd.index);
        }
    }

    if (!device->description[0]) {
        strncpy(device->description, card->description, DEVICE_MAXDESCLEN - 1);
    }

    if (!device->major) {
        device->major = AUDIO_MAJOR;
        device->minor = (uint16_t)(card->snd.index * 16 + AUDIO_MINOR_MIX);
    }

    device->mix.recsrc &= device->mix.recmask;
}


void mixer_dnit(device_t* device) {

    DEBUG_ASSERT(device);

    device->mix.card = NULL;
}



void init(const char* args) {
    (void)args;
}

void dnit(void) {
}
