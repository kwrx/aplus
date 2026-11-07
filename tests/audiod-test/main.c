/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2026 Antonino Natale
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
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <aplus/audio.h>


#define WATCHDOG_SECONDS 40


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                          \
    {                                                                        \
        total++;                                                             \
        if (cond) {                                                          \
            printf("audiod-test: PASS  %s\n", (name));                       \
        } else {                                                             \
            failures++;                                                      \
            printf("audiod-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                    \
    }

#define INFO(fmt, ...) printf("audiod-test: INFO  " fmt "\n", __VA_ARGS__)


typedef struct {

    audio_stream_t* stream;

    double freq;
    double phase;

    uint32_t rate;
    uint16_t channels;
    uint32_t format;

    uint64_t sent;
    uint64_t limit;

} tone_t;


static int64_t now_ms(void) {

    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}


static size_t tone_render(tone_t* tone, uint8_t* out, size_t frames) {

    uint8_t* cursor = out;

    for (size_t i = 0; i < frames; i++) {

        const double v = tone->freq > 0.0 ? 0.3 * sin(tone->phase) : 0.0;

        tone->phase += 2.0 * M_PI * tone->freq / tone->rate;

        if (tone->phase > 2.0 * M_PI) {
            tone->phase -= 2.0 * M_PI;
        }

        for (uint16_t c = 0; c < tone->channels; c++) {

            if (tone->format == AUDIO_FORMAT_U8) {

                *cursor++ = (uint8_t)(128 + (int)(v * 127.0));

            } else {

                const int16_t s = (int16_t)(v * 32767.0);

                *cursor++ = (uint8_t)(s & 0xFF);
                *cursor++ = (uint8_t)((s >> 8) & 0xFF);
            }
        }
    }

    return (size_t)(cursor - out);
}


static int tone_feed(tone_t* tone) {

    static uint8_t buffer[8192];

    const size_t frame = audio_stream_frame(tone->stream);

    while (tone->sent < tone->limit) {

        size_t frames = sizeof(buffer) / frame;

        if (frames > tone->limit - tone->sent) {
            frames = (size_t)(tone->limit - tone->sent);
        }

        const double phase = tone->phase;
        const size_t size  = tone_render(tone, buffer, frames);

        const ssize_t w = audio_stream_write(tone->stream, buffer, size);

        if (w < 0) {

            tone->phase = phase;

            return errno == EAGAIN ? 0 : -1;
        }

        tone->phase = phase;
        tone_render(tone, buffer, (size_t)w / frame);

        tone->sent += (uint64_t)w / frame;
    }

    return 0;
}


static tone_t tone_open(const char* name, double freq, uint32_t rate, uint16_t channels, uint32_t format, double seconds) {

    tone_t tone = {

        .stream   = audio_stream_open(name, rate, channels, format),
        .freq     = freq,
        .rate     = rate,
        .channels = channels,
        .format   = format,
        .limit    = (uint64_t)(seconds * rate),
    };

    return tone;
}


static pid_t watchdog(void) {

    const pid_t parent = getpid();

    const pid_t child = fork();

    if (child == 0) {

        sleep(WATCHDOG_SECONDS);

        printf("audiod-test: FAIL  watchdog: still running after %d s\n", WATCHDOG_SECONDS);

        kill(parent, SIGKILL);
        _exit(0);
    }

    return child;
}


int main(void) {

    setvbuf(stdout, NULL, _IONBF, 0);

    const pid_t guard = watchdog();


    tone_t a = tone_open("test-a", 440.0, 44100, 2, AUDIO_FORMAT_S16_LE, 2.0);

    CHECK(a.stream != NULL, "open 44100 Hz s16 stereo", "%s", strerror(errno));

    if (!a.stream) {
        goto done;
    }

    CHECK(!audio_stream_direct(a.stream), "served by aplus-audiod", "%s", "fell back to /dev/dsp");


    int dsp = open(AUDIO_DSP_PATH, O_WRONLY | O_NONBLOCK);

    CHECK(dsp < 0 && errno == EBUSY, "/dev/dsp held by the server", "open returned %d (%s)", dsp, dsp < 0 ? strerror(errno) : "ok");

    if (dsp >= 0) {
        close(dsp);
    }


    tone_t b = {0};

    const int64_t start = now_ms();

    int64_t a_heard_at = -1;
    bool feed_ok       = true;

    while (a.sent < a.limit || !b.stream || b.sent < b.limit) {

        if (!b.stream && a.sent >= a.rate) {

            b = tone_open("test-b", 1000.0, 22050, 1, AUDIO_FORMAT_U8, 2.0);

            CHECK(b.stream != NULL, "open a second stream at 22050 Hz u8 mono", "%s", strerror(errno));

            if (!b.stream) {
                goto done;
            }

            CHECK(!audio_stream_direct(b.stream), "second stream shares the server", "%s", "fell back to /dev/dsp");
        }

        if (tone_feed(&a) < 0 || (b.stream && tone_feed(&b) < 0)) {
            feed_ok = false;
            break;
        }

        if (a_heard_at < 0 && a.sent > 0 && audio_stream_delay(a.stream) < (ssize_t)(a.sent * audio_stream_frame(a.stream))) {
            a_heard_at = now_ms() - start;
        }

        struct pollfd fds[2] = {

            {.fd = audio_stream_fd(a.stream), .events = audio_stream_events(a.stream), .revents = 0},
            {.fd = b.stream ? audio_stream_fd(b.stream) : -1, .events = b.stream ? audio_stream_events(b.stream) : 0, .revents = 0},
        };

        poll(fds, b.stream ? 2 : 1, 100);
    }

    CHECK(feed_ok, "feed both streams", "%s", strerror(errno));
    CHECK(a_heard_at >= 0 && a_heard_at < 500, "first samples heard promptly", "after %lld ms", (long long)a_heard_at);


    const int ra = audio_stream_drain(a.stream, 3000);
    const int rb = audio_stream_drain(b.stream, 3000);

    const int64_t elapsed = now_ms() - start;

    CHECK(ra == 0 && rb == 0, "drain both streams", "a=%d b=%d (%s)", ra, rb, strerror(errno));
    CHECK(audio_stream_delay(a.stream) == 0 && audio_stream_delay(b.stream) == 0, "nothing left after drain", "a=%zd b=%zd", audio_stream_delay(a.stream), audio_stream_delay(b.stream));
    CHECK(elapsed >= 2800 && elapsed <= 3800, "3 s of audio take about 3 s", "took %lld ms", (long long)elapsed);

    INFO("timeline took %lld ms", (long long)elapsed);


    tone_t c = tone_open("test-c", 0.0, 48000, 2, AUDIO_FORMAT_S16_LE, 0.15);

    CHECK(c.stream != NULL, "open a third stream", "%s", strerror(errno));

    if (c.stream) {

        tone_feed(&c);

        const ssize_t before = audio_stream_delay(c.stream);

        CHECK(before > 0, "queued data shows as delay", "delay %zd", before);
        CHECK(audio_stream_reset(c.stream) == 0, "reset", "%s", strerror(errno));
        CHECK(audio_stream_delay(c.stream) == 0, "reset clears the delay", "delay %zd", audio_stream_delay(c.stream));

        c.sent  = 0;
        c.limit = 4800;

        tone_feed(&c);

        const int64_t t0 = now_ms();
        const int rc     = audio_stream_drain(c.stream, 2000);

        CHECK(rc == 0 && now_ms() - t0 < 1000, "data after a reset plays out", "drain %d after %lld ms", rc, (long long)(now_ms() - t0));
    }


    audio_stream_close(a.stream);
    audio_stream_close(b.stream);

    if (c.stream) {
        audio_stream_close(c.stream);
    }


    usleep(2600 * 1000);

    dsp = open(AUDIO_DSP_PATH, O_WRONLY | O_NONBLOCK);

    CHECK(dsp >= 0, "server releases /dev/dsp when idle", "%s", strerror(errno));

    if (dsp >= 0) {
        close(dsp);
    }


done:

    printf("audiod-test: %d/%d passed\n", total - failures, total);

    if (guard > 0) {
        kill(guard, SIGKILL);
        waitpid(guard, NULL, 0);
    }

    return failures ? 1 : 0;
}
