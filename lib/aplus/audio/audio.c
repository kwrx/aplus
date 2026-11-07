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
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <aplus/audio.h>
#include <aplus/soundcard.h>


#define AUDIO_HANDSHAKE_MS 2000
#define AUDIO_INBOX_SIZE   256


struct audio_stream {

    int fd;
    bool direct;
    int error;

    uint32_t rate;
    uint16_t channels;
    uint32_t format;
    size_t frame;
    size_t fragment;

    uint32_t window;
    uint32_t serial;

    uint64_t sent;
    uint64_t consumed;
    uint64_t heard;

    uint8_t inbox[AUDIO_INBOX_SIZE];
    size_t inbox_size;
};


static size_t audio_frame_bytes(uint32_t format, uint16_t channels) {
    return (size_t)channels * (format == AUDIO_FORMAT_U8 ? 1 : 2);
}


static int64_t audio_now_ms(void) {

    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}


static int audio_send_all(int fd, const void* buf, size_t size) {

    const uint8_t* at = buf;

    while (size > 0) {

        const ssize_t w = write(fd, at, size);

        if (w < 0) {

            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN) {

                struct pollfd pfd = {.fd = fd, .events = POLLOUT, .revents = 0};

                if (poll(&pfd, 1, -1) < 0 && errno != EINTR) {
                    return -1;
                }

                continue;
            }

            return -1;
        }

        at += w;
        size -= (size_t)w;
    }

    return 0;
}


static int audio_send(audio_stream_t* stream, uint32_t type, const void* payload, size_t size) {

    audio_msg_header_t header = {

        .type = type,
        .size = (uint32_t)size,
    };

    if (audio_send_all(stream->fd, &header, sizeof(header)) < 0) {
        return -1;
    }

    if (size > 0 && audio_send_all(stream->fd, payload, size) < 0) {
        return -1;
    }

    return 0;
}


static int audio_recv_all(int fd, void* buf, size_t size, int timeout) {

    uint8_t* at = buf;

    const int64_t deadline = audio_now_ms() + timeout;

    while (size > 0) {

        const int64_t left = deadline - audio_now_ms();

        if (left <= 0) {
            errno = ETIMEDOUT;
            return -1;
        }

        struct pollfd pfd = {.fd = fd, .events = POLLIN, .revents = 0};

        if (poll(&pfd, 1, (int)left) < 0) {

            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (!(pfd.revents & (POLLIN | POLLHUP | POLLERR))) {
            continue;
        }

        const ssize_t r = read(fd, at, size);

        if (r < 0) {

            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }

            return -1;
        }

        if (r == 0) {
            errno = EPIPE;
            return -1;
        }

        at += r;
        size -= (size_t)r;
    }

    return 0;
}


static void audio_dispatch(audio_stream_t* stream, const audio_msg_header_t* header, const uint8_t* payload) {

    switch (header->type) {

        case AUDIO_MSG_STATUS: {

            if (header->size < sizeof(audio_msg_status_t)) {
                break;
            }

            audio_msg_status_t status;

            memcpy(&status, payload, sizeof(status));

            if (status.serial != stream->serial) {
                break;
            }

            stream->consumed = status.consumed;
            stream->heard    = status.heard;

            break;
        }

        case AUDIO_MSG_ERROR: {

            if (header->size < sizeof(audio_msg_error_t)) {
                stream->error = EIO;
                break;
            }

            audio_msg_error_t error;

            memcpy(&error, payload, sizeof(error));

            stream->error = error.error > 0 ? error.error : EIO;

            break;
        }

        default:
            break;
    }
}


static int audio_pump(audio_stream_t* stream) {

    if (stream->error) {
        errno = stream->error;
        return -1;
    }


    for (;;) {

        const ssize_t r = read(stream->fd, stream->inbox + stream->inbox_size, sizeof(stream->inbox) - stream->inbox_size);

        if (r < 0) {

            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN) {
                break;
            }

            stream->error = errno;
            return -1;
        }

        if (r == 0) {
            stream->error = EPIPE;
            errno         = EPIPE;
            return -1;
        }

        stream->inbox_size += (size_t)r;


        size_t at = 0;

        while (stream->inbox_size - at >= sizeof(audio_msg_header_t)) {

            audio_msg_header_t header;

            memcpy(&header, stream->inbox + at, sizeof(header));

            if (header.size > sizeof(stream->inbox) - sizeof(header)) {
                stream->error = EPROTO;
                errno         = EPROTO;
                return -1;
            }

            if (stream->inbox_size - at < sizeof(header) + header.size) {
                break;
            }

            audio_dispatch(stream, &header, stream->inbox + at + sizeof(header));

            at += sizeof(header) + header.size;
        }

        memmove(stream->inbox, stream->inbox + at, stream->inbox_size - at);

        stream->inbox_size -= at;
    }


    if (stream->error) {
        errno = stream->error;
        return -1;
    }

    return 0;
}


static audio_stream_t* audio_open_direct(audio_stream_t* stream, uint32_t rate, uint16_t channels, uint32_t format) {

    if ((stream->fd = open(AUDIO_DSP_PATH, O_WRONLY | O_NONBLOCK)) < 0) {
        return NULL;
    }


    int value = (int)format;

    if (ioctl(stream->fd, SNDCTL_DSP_SETFMT, &value) < 0 || (value != AUDIO_FORMAT_U8 && value != AUDIO_FORMAT_S16_LE)) {
        errno = EINVAL;
        return NULL;
    }

    stream->format = (uint32_t)value;


    value = channels;

    if (ioctl(stream->fd, SNDCTL_DSP_CHANNELS, &value) < 0 || value < 1 || value > AUDIO_CHANNELS_MAX) {
        errno = EINVAL;
        return NULL;
    }

    stream->channels = (uint16_t)value;


    value = (int)rate;

    if (ioctl(stream->fd, SNDCTL_DSP_SPEED, &value) < 0 || value <= 0) {
        errno = EINVAL;
        return NULL;
    }

    stream->rate = (uint32_t)value;


    value = 0;

    if (ioctl(stream->fd, SNDCTL_DSP_GETBLKSIZE, &value) < 0 || value < 0) {
        value = 0;
    }

    stream->direct   = true;
    stream->frame    = audio_frame_bytes(stream->format, stream->channels);
    stream->fragment = (size_t)value - (size_t)value % stream->frame;

    return stream;
}


static int audio_connect(audio_stream_t* stream) {

    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    strcpy(addr.sun_path, AUDIO_SOCKET);


    if ((stream->fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    if (connect(stream->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return -1;
    }

    return 0;
}


static audio_stream_t* audio_open_server(audio_stream_t* stream, const char* name, uint32_t rate, uint16_t channels, uint32_t format) {

    audio_msg_hello_t hello = {

        .version  = AUDIO_PROTOCOL_VERSION,
        .rate     = rate,
        .format   = format,
        .channels = channels,
    };

    strncpy(hello.name, name ? name : "", sizeof(hello.name) - 1);

    if (audio_send(stream, AUDIO_MSG_HELLO, &hello, sizeof(hello)) < 0) {
        return NULL;
    }


    audio_msg_header_t header;

    if (audio_recv_all(stream->fd, &header, sizeof(header), AUDIO_HANDSHAKE_MS) < 0) {
        return NULL;
    }

    if (header.size > sizeof(stream->inbox)) {
        errno = EPROTO;
        return NULL;
    }

    if (audio_recv_all(stream->fd, stream->inbox, header.size, AUDIO_HANDSHAKE_MS) < 0) {
        return NULL;
    }


    if (header.type == AUDIO_MSG_ERROR) {

        audio_msg_error_t error = {.error = EIO};

        if (header.size >= sizeof(error)) {
            memcpy(&error, stream->inbox, sizeof(error));
        }

        errno = error.error > 0 ? error.error : EIO;
        return NULL;
    }

    if (header.type != AUDIO_MSG_WELCOME || header.size < sizeof(audio_msg_welcome_t)) {
        errno = EPROTO;
        return NULL;
    }


    audio_msg_welcome_t welcome;

    memcpy(&welcome, stream->inbox, sizeof(welcome));

    if (welcome.version != AUDIO_PROTOCOL_VERSION) {
        errno = EPROTONOSUPPORT;
        return NULL;
    }


    if (fcntl(stream->fd, F_SETFL, fcntl(stream->fd, F_GETFL) | O_NONBLOCK) < 0) {
        return NULL;
    }

    stream->rate     = rate;
    stream->channels = channels;
    stream->format   = format;
    stream->frame    = audio_frame_bytes(format, channels);
    stream->fragment = 0;
    stream->window   = welcome.window - welcome.window % (uint32_t)stream->frame;

    if (stream->window == 0) {
        errno = EPROTO;
        return NULL;
    }

    return stream;
}


audio_stream_t* audio_stream_open(const char* name, uint32_t rate, uint16_t channels, uint32_t format) {

    if (rate < AUDIO_RATE_MIN || rate > AUDIO_RATE_MAX || channels < 1 || channels > AUDIO_CHANNELS_MAX) {
        errno = EINVAL;
        return NULL;
    }

    if (format != AUDIO_FORMAT_U8 && format != AUDIO_FORMAT_S16_LE) {
        errno = EINVAL;
        return NULL;
    }


    audio_stream_t* stream = calloc(1, sizeof(*stream));

    if (!stream) {
        return NULL;
    }

    stream->fd = -1;


    audio_stream_t* opened;

    if (audio_connect(stream) == 0) {

        opened = audio_open_server(stream, name, rate, channels, format);

    } else if (errno == ENOENT || errno == ECONNREFUSED) {

        if (stream->fd >= 0) {
            close(stream->fd);
            stream->fd = -1;
        }

        opened = audio_open_direct(stream, rate, channels, format);

    } else {

        opened = NULL;
    }


    if (!opened) {

        const int error = errno;

        if (stream->fd >= 0) {
            close(stream->fd);
        }

        free(stream);

        errno = error;
        return NULL;
    }

    return stream;
}


void audio_stream_close(audio_stream_t* stream) {

    if (!stream) {
        return;
    }

    if (stream->direct) {
        ioctl(stream->fd, SNDCTL_DSP_RESET, 0);
    }

    close(stream->fd);
    free(stream);
}


bool audio_stream_direct(const audio_stream_t* stream) {
    return stream->direct;
}


uint32_t audio_stream_rate(const audio_stream_t* stream) {
    return stream->rate;
}


uint16_t audio_stream_channels(const audio_stream_t* stream) {
    return stream->channels;
}


uint32_t audio_stream_format(const audio_stream_t* stream) {
    return stream->format;
}


size_t audio_stream_frame(const audio_stream_t* stream) {
    return stream->frame;
}


size_t audio_stream_fragment(const audio_stream_t* stream) {
    return stream->fragment;
}


int audio_stream_fd(const audio_stream_t* stream) {
    return stream->fd;
}


short audio_stream_events(const audio_stream_t* stream) {
    return stream->direct ? POLLOUT : POLLIN;
}


ssize_t audio_stream_write(audio_stream_t* stream, const void* buf, size_t size) {

    size -= size % stream->frame;

    if (size == 0) {
        return 0;
    }


    if (stream->direct) {
        return write(stream->fd, buf, size);
    }


    if (audio_pump(stream) < 0) {
        return -1;
    }


    const uint8_t* at = buf;

    size_t done = 0;

    while (done < size) {

        const uint64_t inflight = stream->sent - stream->consumed;

        if (inflight >= stream->window) {
            break;
        }


        size_t chunk = size - done;

        if (chunk > stream->window - inflight) {
            chunk = (size_t)(stream->window - inflight);
        }

        if (chunk > AUDIO_DATA_MAX) {
            chunk = AUDIO_DATA_MAX;
        }

        chunk -= chunk % stream->frame;

        if (chunk == 0) {
            break;
        }


        if (audio_send(stream, AUDIO_MSG_DATA, at + done, chunk) < 0) {
            stream->error = errno;
            return done ? (ssize_t)done : -1;
        }

        stream->sent += chunk;
        done += chunk;
    }


    if (done == 0) {
        errno = EAGAIN;
        return -1;
    }

    return (ssize_t)done;
}


ssize_t audio_stream_delay(audio_stream_t* stream) {

    if (stream->direct) {

        int delay = 0;

        if (ioctl(stream->fd, SNDCTL_DSP_GETODELAY, &delay) < 0) {
            return -1;
        }

        return delay < 0 ? 0 : delay;
    }


    if (audio_pump(stream) < 0) {
        return -1;
    }

    return stream->sent > stream->heard ? (ssize_t)(stream->sent - stream->heard) : 0;
}


int audio_stream_reset(audio_stream_t* stream) {

    if (stream->direct) {
        return ioctl(stream->fd, SNDCTL_DSP_RESET, 0);
    }


    if (audio_pump(stream) < 0) {
        return -1;
    }


    audio_msg_reset_t reset = {

        .serial = stream->serial + 1,
    };

    if (audio_send(stream, AUDIO_MSG_RESET, &reset, sizeof(reset)) < 0) {
        stream->error = errno;
        return -1;
    }

    stream->serial   = reset.serial;
    stream->sent     = 0;
    stream->consumed = 0;
    stream->heard    = 0;

    return 0;
}


int audio_stream_drain(audio_stream_t* stream, int timeout) {

    if (stream->direct) {
        return ioctl(stream->fd, SNDCTL_DSP_SYNC, 0);
    }


    const int64_t deadline = audio_now_ms() + timeout;

    for (;;) {

        if (audio_pump(stream) < 0) {
            return -1;
        }

        if (stream->heard >= stream->sent) {
            return 0;
        }


        int wait = -1;

        if (timeout >= 0) {

            const int64_t left = deadline - audio_now_ms();

            if (left <= 0) {
                errno = ETIMEDOUT;
                return -1;
            }

            wait = (int)left;
        }


        struct pollfd pfd = {.fd = stream->fd, .events = POLLIN, .revents = 0};

        if (poll(&pfd, 1, wait) < 0 && errno != EINTR) {
            return -1;
        }
    }
}
