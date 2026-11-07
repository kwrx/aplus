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
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <aplus/soundcard.h>


#define DSP_PATH   "/dev/dsp"
#define MIXER_PATH "/dev/mixer"

#define WAVE_FORMAT_PCM        0x0001
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE

#define SRC_FRAMES 4096
#define DST_FRAMES 2048
#define MAX_CHANS  2

#define FX_ONE 65536u

#define MIXER_LEVEL(l, r) (((uint32_t)((r) & 0xFF) << 8) | ((uint32_t)((l) & 0xFF)))
#define MIXER_LEFT(v)     ((uint32_t)(v) & 0xFF)
#define MIXER_RIGHT(v)    (((uint32_t)(v) >> 8) & 0xFF)


static int failures = 0;
static int total    = 0;

static volatile sig_atomic_t stopping = 0;


#define CHECK(cond, name, fmt, ...)                                         \
    {                                                                       \
        total++;                                                            \
        if (cond) {                                                         \
            printf("audio-test: PASS  %s\n", (name));                       \
        } else {                                                            \
            failures++;                                                     \
            printf("audio-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                   \
    }

#define INFO(fmt, ...) printf("audio-test: INFO  " fmt "\n", __VA_ARGS__)


typedef struct {

    uint16_t channels;
    uint16_t bits;
    uint32_t rate;
    off_t offset;
    uint64_t bytes;

} wav_t;


typedef struct {

    int fd;
    uint32_t format;
    uint16_t channels;
    uint32_t rate;
    uint16_t bits;
    int fragsize;

} dsp_t;


static void on_signal(int sig) {

    (void)sig;

    stopping = 1;
}


static uint64_t now_ms(void) {

    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        return 0;
    }

    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)(ts.tv_nsec / 1000000);
}


static bool read_full(int fd, void* buf, size_t size, size_t* got) {

    size_t done = 0;

    while (done < size) {

        ssize_t r = read(fd, (uint8_t*)buf + done, size - done);

        if (r < 0) {

            if (errno == EINTR) {
                continue;
            }

            *got = done;
            return false;
        }

        if (r == 0) {
            break;
        }

        done += (size_t)r;
    }

    *got = done;
    return true;
}


static bool wav_open(const char* path, wav_t* wav, int* out_fd) {

    const int fd = open(path, O_RDONLY);

    if (fd < 0) {
        printf("audio-test: FAIL  open %s: %s\n", path, strerror(errno));
        return false;
    }

    struct stat st;

    if (fstat(fd, &st) < 0) {
        printf("audio-test: FAIL  stat %s: %s\n", path, strerror(errno));
        close(fd);
        return false;
    }

    uint8_t riff[12];
    size_t got = 0;

    if (!read_full(fd, riff, sizeof(riff), &got) || got != sizeof(riff)) {
        printf("audio-test: FAIL  %s is too short to be a RIFF file\n", path);
        close(fd);
        return false;
    }

    if (memcmp(&riff[0], "RIFF", 4) != 0 || memcmp(&riff[8], "WAVE", 4) != 0) {
        printf("audio-test: FAIL  %s is not a RIFF/WAVE file\n", path);
        close(fd);
        return false;
    }

    bool have_fmt  = false;
    bool have_data = false;
    uint16_t tag   = 0;

    memset(wav, 0, sizeof(*wav));

    off_t pos = 12;

    while (!have_data || !have_fmt) {

        uint8_t head[8];

        if (lseek(fd, pos, SEEK_SET) < 0) {
            break;
        }

        if (!read_full(fd, head, sizeof(head), &got) || got != sizeof(head)) {
            break;
        }

        const uint32_t size = (uint32_t)head[4] | ((uint32_t)head[5] << 8) | ((uint32_t)head[6] << 16) | ((uint32_t)head[7] << 24);

        if (memcmp(head, "fmt ", 4) == 0 && size >= 16) {

            uint8_t fmt[40];
            const size_t want = size > sizeof(fmt) ? sizeof(fmt) : size;

            if (!read_full(fd, fmt, want, &got) || got != want) {
                break;
            }

            tag           = (uint16_t)(fmt[0] | (fmt[1] << 8));
            wav->channels = (uint16_t)(fmt[2] | (fmt[3] << 8));
            wav->rate     = (uint32_t)fmt[4] | ((uint32_t)fmt[5] << 8) | ((uint32_t)fmt[6] << 16) | ((uint32_t)fmt[7] << 24);
            wav->bits     = (uint16_t)(fmt[14] | (fmt[15] << 8));

            if (tag == WAVE_FORMAT_EXTENSIBLE && want >= 26) {
                tag = (uint16_t)(fmt[24] | (fmt[25] << 8));
            }

            have_fmt = true;

        } else if (memcmp(head, "data", 4) == 0) {

            wav->offset = pos + 8;
            wav->bytes  = size;

            have_data = true;
        }

        pos += 8 + size + (size & 1);

        if (pos >= st.st_size) {
            break;
        }
    }

    if (!have_fmt || !have_data) {
        printf("audio-test: FAIL  %s has no fmt or data chunk\n", path);
        close(fd);
        return false;
    }

    if (wav->bytes == 0 || wav->offset + (off_t)wav->bytes > st.st_size) {
        wav->bytes = (uint64_t)(st.st_size - wav->offset);
    }

    CHECK(tag == WAVE_FORMAT_PCM, "the file holds uncompressed pcm", "format tag %#x", tag);
    CHECK(wav->bits == 8 || wav->bits == 16, "the sample width is supported", "%u bits", wav->bits);
    CHECK(wav->channels >= 1 && wav->channels <= MAX_CHANS, "the channel count is supported", "%u channels", wav->channels);
    CHECK(wav->rate >= 1000, "the sample rate is sane", "%u Hz", wav->rate);

    INFO("%s: %u Hz, %u bit, %u channel(s), %llu bytes of pcm at offset %lld", path, wav->rate, wav->bits, wav->channels, (unsigned long long)wav->bytes, (long long)wav->offset);

    if (tag != WAVE_FORMAT_PCM || (wav->bits != 8 && wav->bits != 16) || wav->channels < 1 || wav->channels > MAX_CHANS) {
        close(fd);
        return false;
    }

    if (lseek(fd, wav->offset, SEEK_SET) < 0) {
        printf("audio-test: FAIL  seek to the data chunk: %s\n", strerror(errno));
        close(fd);
        return false;
    }

    *out_fd = fd;
    return true;
}


static void dsp_report(int fd) {

    int caps = 0;

    errno = 0;
    int r = ioctl(fd, SNDCTL_DSP_GETCAPS, &caps);
    CHECK(r == 0 && (caps & DSP_CAP_OUTPUT), "the device reports playback support", "%d %s caps %#x", r, strerror(errno), caps);

    int fmts = 0;

    errno = 0;
    r     = ioctl(fd, SNDCTL_DSP_GETFMTS, &fmts);
    CHECK(r == 0 && (fmts & (AFMT_U8 | AFMT_S16_LE)), "the device lists at least one pcm format", "%d %s formats %#x", r, strerror(errno), fmts);

    INFO("caps %#x, formats %#x%s%s", caps, fmts, (caps & DSP_CAP_DUPLEX) ? ", duplex" : "", (caps & DSP_CAP_TRIGGER) ? ", trigger" : "");
}


static bool dsp_setup(dsp_t* dsp, const wav_t* wav) {

    int want = wav->bits == 8 ? AFMT_U8 : AFMT_S16_LE;
    int fmts = 0;

    ioctl(dsp->fd, SNDCTL_DSP_GETFMTS, &fmts);

    if (!(fmts & want)) {
        want = (fmts & AFMT_S16_LE) ? AFMT_S16_LE : AFMT_U8;
    }

    int fmt = want;

    errno = 0;
    int r = ioctl(dsp->fd, SNDCTL_DSP_SETFMT, &fmt);
    CHECK(r == 0 && fmt == want, "set the sample format", "%d %s asked %#x got %#x", r, strerror(errno), want, fmt);

    int channels = wav->channels;

    errno = 0;
    r     = ioctl(dsp->fd, SNDCTL_DSP_CHANNELS, &channels);
    CHECK(r == 0 && channels >= 1 && channels <= MAX_CHANS, "set the channel count", "%d %s got %d", r, strerror(errno), channels);

    int rate = (int)wav->rate;

    errno = 0;
    r     = ioctl(dsp->fd, SNDCTL_DSP_SPEED, &rate);
    CHECK(r == 0 && rate > 0, "set the sample rate", "%d %s got %d", r, strerror(errno), rate);

    int bits = 0;

    errno = 0;
    r     = ioctl(dsp->fd, SOUND_PCM_READ_BITS, &bits);
    CHECK(r == 0 && bits == (fmt == AFMT_U8 ? 8 : 16), "the device agrees on the sample width", "%d %s got %d for format %#x", r, strerror(errno), bits, fmt);

    int blk = 0;

    errno = 0;
    r     = ioctl(dsp->fd, SNDCTL_DSP_GETBLKSIZE, &blk);
    CHECK(r == 0 && blk > 0, "read the fragment size", "%d %s got %d", r, strerror(errno), blk);

    struct audio_buf_info bi;

    memset(&bi, 0, sizeof(bi));

    errno = 0;
    r     = ioctl(dsp->fd, SNDCTL_DSP_GETOSPACE, &bi);
    CHECK(r == 0 && bi.fragstotal > 0 && bi.fragsize == blk && bi.bytes == bi.fragments * bi.fragsize, "the output buffer starts empty", "%d %s %d/%d frags of %d, %d bytes", r, strerror(errno), bi.fragments, bi.fragstotal, bi.fragsize,
          bi.bytes);

    dsp->format   = (uint32_t)fmt;
    dsp->channels = (uint16_t)channels;
    dsp->rate     = (uint32_t)rate;
    dsp->bits     = fmt == AFMT_U8 ? 8 : 16;
    dsp->fragsize = blk > 0 ? blk : 4096;

    INFO("device: %u Hz, %u bit, %u channel(s), %d x %d byte fragments", dsp->rate, dsp->bits, dsp->channels, bi.fragstotal, dsp->fragsize);

    if (dsp->rate != wav->rate) {
        INFO("resampling %u Hz -> %u Hz (%s by %u.%03u)", wav->rate, dsp->rate, wav->rate > dsp->rate ? "down" : "up", wav->rate / dsp->rate, (unsigned)((uint64_t)(wav->rate % dsp->rate) * 1000 / dsp->rate));
    }

    if (dsp->channels != wav->channels) {
        INFO("remapping %u channel(s) -> %u channel(s)", wav->channels, dsp->channels);
    }

    return true;
}


static void dsp_errors(int fd) {

    const char junk[3] = {0, 0, 0};

    errno     = 0;
    ssize_t w = write(fd, junk, sizeof(junk));
    CHECK(w < 0 && errno == EINVAL, "a write shorter than one frame is refused", "%zd %s", w, strerror(errno));

    errno = 0;
    int r = ioctl(fd, _IOWR('Z', 0, int), &r);
    CHECK(r < 0 && errno == EINVAL, "an ioctl of another type is refused", "%d %s", r, strerror(errno));

    errno    = 0;
    int self = open(DSP_PATH, O_WRONLY);
    CHECK(self < 0 && errno == EBUSY, "the device is exclusive while open", "%d %s", self, strerror(errno));

    if (self >= 0) {
        close(self);
    }

    errno = 0;
    r     = ioctl(fd, SNDCTL_DSP_RESET, 0);
    CHECK(r == 0, "reset the device after the error cases", "%d %s", r, strerror(errno));
}


static void mixer_setup(int volume) {

    const int fd = open(MIXER_PATH, O_RDWR);

    if (fd < 0) {
        printf("audio-test: FAIL  open %s: %s\n", MIXER_PATH, strerror(errno));
        failures++;
        total++;
        return;
    }

    int devmask = 0;

    errno = 0;
    int r = ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devmask);
    CHECK(r == 0 && (devmask & SOUND_MASK_VOLUME), "the mixer reports a master volume", "%d %s devmask %#x", r, strerror(errno), devmask);

    int level = 0;

    errno = 0;
    r     = ioctl(fd, SOUND_MIXER_READ_VOLUME, &level);
    CHECK(r == 0, "read the master volume", "%d %s", r, strerror(errno));

    INFO("volume is %u/%u before playback", MIXER_LEFT(level), MIXER_RIGHT(level));

    if (volume >= 0) {

        int want = (int)MIXER_LEVEL(volume, volume);

        errno = 0;
        r     = ioctl(fd, SOUND_MIXER_WRITE_VOLUME, &want);
        CHECK(r == 0, "set the master volume", "%d %s", r, strerror(errno));

        int back = 0;

        ioctl(fd, SOUND_MIXER_READ_VOLUME, &back);
        CHECK((int)MIXER_LEFT(back) == volume && (int)MIXER_RIGHT(back) == volume, "the mixer keeps the volume it was given", "asked %d got %u/%u", volume, MIXER_LEFT(back), MIXER_RIGHT(back));
    }

    int bogus = (int)MIXER_LEVEL(101, 101);

    errno = 0;
    r     = ioctl(fd, SOUND_MIXER_WRITE_VOLUME, &bogus);
    CHECK(r < 0 && errno == EINVAL, "a level above 100 is refused", "%d %s", r, strerror(errno));

    close(fd);
}


static size_t convert(const uint8_t* in, size_t frames, const wav_t* wav, int16_t (*out)[MAX_CHANS]) {

    for (size_t i = 0; i < frames; i++) {

        for (unsigned c = 0; c < wav->channels; c++) {

            if (wav->bits == 8) {
                out[i][c] = (int16_t)(((int)in[i * wav->channels + c] - 128) << 8);
            } else {
                const size_t o = (i * wav->channels + c) * 2;
                out[i][c]      = (int16_t)((uint16_t)in[o] | ((uint16_t)in[o + 1] << 8));
            }
        }
    }

    return frames;
}


static void emit(const int16_t* frame, unsigned src_channels, const dsp_t* dsp, uint8_t** cursor) {

    int32_t v[MAX_CHANS];

    if (src_channels == dsp->channels) {

        for (unsigned c = 0; c < dsp->channels; c++) {
            v[c] = frame[c];
        }

    } else if (src_channels == 1) {

        for (unsigned c = 0; c < dsp->channels; c++) {
            v[c] = frame[0];
        }

    } else {

        int32_t mix = 0;

        for (unsigned c = 0; c < src_channels; c++) {
            mix += frame[c];
        }

        mix /= (int32_t)src_channels;

        for (unsigned c = 0; c < dsp->channels; c++) {
            v[c] = mix;
        }
    }

    for (unsigned c = 0; c < dsp->channels; c++) {

        if (dsp->bits == 8) {
            *(*cursor)++ = (uint8_t)(((v[c] >> 8) + 128) & 0xFF);
        } else {
            *(*cursor)++ = (uint8_t)(v[c] & 0xFF);
            *(*cursor)++ = (uint8_t)((v[c] >> 8) & 0xFF);
        }
    }
}


static bool push(const dsp_t* dsp, const uint8_t* buf, size_t size, uint64_t* written, uint64_t* stalls) {

    size_t done = 0;

    while (done < size) {

        if (stopping) {
            return false;
        }

        const ssize_t w = write(dsp->fd, buf + done, size - done);

        if (w > 0) {

            done += (size_t)w;
            *written += (uint64_t)w;

            continue;
        }

        if (w < 0 && (errno == EAGAIN || errno == EINTR)) {

            struct pollfd pfd = {.fd = dsp->fd, .events = POLLOUT, .revents = 0};

            (*stalls)++;

            if (poll(&pfd, 1, 1000) == 0) {
                printf("audio-test: FAIL  the device stopped asking for data\n");
                failures++;
                total++;
                return false;
            }

            continue;
        }

        printf("audio-test: FAIL  write to %s: %zd %s\n", DSP_PATH, w, strerror(errno));
        failures++;
        total++;
        return false;
    }

    return true;
}


static void play(int fd, const wav_t* wav, dsp_t* dsp, int seconds) {

    static uint8_t raw[SRC_FRAMES * MAX_CHANS * 2];
    static int16_t src[SRC_FRAMES + 16][MAX_CHANS];
    static uint8_t dst[DST_FRAMES * MAX_CHANS * 2];

    const size_t src_frame = (size_t)wav->channels * (wav->bits / 8);
    const size_t dst_frame = (size_t)dsp->channels * (dsp->bits / 8);

    const uint32_t step = (uint32_t)(((uint64_t)wav->rate * FX_ONE) / dsp->rate);

    uint64_t limit = wav->bytes;

    if (seconds > 0) {

        const uint64_t cap = (uint64_t)seconds * wav->rate * src_frame;

        if (cap < limit) {
            limit = cap;
        }
    }

    uint64_t consumed = 0;
    uint64_t written  = 0;
    uint64_t frames   = 0;
    uint64_t stalls   = 0;

    size_t navail = 0;
    uint32_t pos  = 0;

    const uint64_t start = now_ms();

    uint64_t mark = start;
    bool eof      = false;

    while (!stopping) {

        if (!eof && navail < SRC_FRAMES) {

            size_t want = (SRC_FRAMES - navail) * src_frame;

            if (consumed + want > limit) {
                want = (size_t)(limit - consumed);
            }

            want -= want % src_frame;

            if (want == 0) {

                eof = true;

            } else {

                size_t got = 0;

                if (!read_full(fd, raw, want, &got)) {
                    printf("audio-test: FAIL  read the pcm data: %s\n", strerror(errno));
                    failures++;
                    total++;
                    break;
                }

                consumed += got;

                if (got < want) {
                    eof = true;
                }

                navail += convert(raw, got / src_frame, wav, &src[navail]);
            }
        }

        if (navail == 0 && eof) {
            break;
        }

        uint8_t* cursor = dst;
        size_t produced = 0;

        while (produced < DST_FRAMES) {

            const size_t a = pos >> 16;
            size_t b       = (pos + step) >> 16;

            if (b <= a) {
                b = a + 1;
            }

            if (b > navail) {

                if (!eof) {
                    break;
                }

                if (a >= navail) {
                    break;
                }

                b = navail;
            }

            int16_t avg[MAX_CHANS];

            for (unsigned c = 0; c < wav->channels; c++) {

                int32_t acc = 0;

                for (size_t i = a; i < b; i++) {
                    acc += src[i][c];
                }

                avg[c] = (int16_t)(acc / (int32_t)(b - a));
            }

            emit(avg, wav->channels, dsp, &cursor);

            pos += step;
            produced++;
        }

        if (produced) {

            if (!push(dsp, dst, produced * dst_frame, &written, &stalls)) {
                break;
            }

            frames += produced;
        }

        const size_t eaten = pos >> 16;

        if (eaten) {

            if (eaten >= navail) {
                navail = 0;
            } else {
                memmove(src, &src[eaten], (navail - eaten) * sizeof(src[0]));
                navail -= eaten;
            }

            pos &= FX_ONE - 1;
        }

        if (produced == 0 && eof && navail == 0) {
            break;
        }

        const uint64_t elapsed = now_ms() - start;

        if (elapsed - (mark - start) >= 2000) {

            mark = start + elapsed;

            INFO("%llu KiB read, %llu KiB played, %llu.%01llu s of audio in %llu.%01llu s", (unsigned long long)(consumed / 1024), (unsigned long long)(written / 1024), (unsigned long long)(frames / dsp->rate),
                 (unsigned long long)((frames % dsp->rate) * 10 / dsp->rate), (unsigned long long)(elapsed / 1000), (unsigned long long)((elapsed % 1000) / 100));
        }
    }

    int delay = 0;

    errno = 0;
    int r = ioctl(dsp->fd, SNDCTL_DSP_GETODELAY, &delay);
    CHECK(r == 0 && delay >= 0, "read the output delay", "%d %s delay %d", r, strerror(errno), delay);

    const uint64_t before_sync = now_ms();

    errno = 0;
    r     = ioctl(dsp->fd, SNDCTL_DSP_SYNC, 0);
    CHECK(r == 0, "drain the device", "%d %s", r, strerror(errno));

    const uint64_t elapsed = now_ms() - start;

    int left = 0;

    errno = 0;
    r     = ioctl(dsp->fd, SNDCTL_DSP_GETODELAY, &left);
    CHECK(r == 0 && left == 0, "the buffer is empty once drained", "%d %s delay %d", r, strerror(errno), left);

    CHECK(written == frames * dst_frame, "every converted frame reached the device", "%llu bytes for %llu frames of %zu", (unsigned long long)written, (unsigned long long)frames, dst_frame);

    const uint64_t expected = dsp->rate ? (frames * 1000) / dsp->rate : 0;

    CHECK(stopping || elapsed * 100 >= expected * 85, "the device did not consume faster than real time", "%llu ms of audio took %llu ms", (unsigned long long)expected, (unsigned long long)elapsed);

    if (expected && elapsed * 100 > expected * 115) {
        INFO("the writer could not keep up: %llu ms of audio took %llu ms", (unsigned long long)expected, (unsigned long long)elapsed);
    }

    INFO("played %llu frames (%llu.%01llu s) in %llu.%01llu s, %llu stalls, drain took %llu ms", (unsigned long long)frames, (unsigned long long)(frames / dsp->rate), (unsigned long long)((frames % dsp->rate) * 10 / dsp->rate),
         (unsigned long long)(elapsed / 1000), (unsigned long long)((elapsed % 1000) / 100), (unsigned long long)stalls, (unsigned long long)(now_ms() - before_sync));
}


static void show_usage(void) {

    printf("Use: audio-test [OPTION]... FILE\n"
           "Play a RIFF/WAVE file through " DSP_PATH " and check the OSS interface.\n\n"
           "Options:\n"
           "  -t, --time SECONDS  Play at most SECONDS of audio, 0 for the whole file (default 10)\n"
           "  -v, --volume LEVEL  Set the master volume to LEVEL, 0 to 100\n"
           "  -i, --info          Report the device and the file, play nothing\n"
           "      --help          Show this help message and exit\n\n"
    );
}


int main(int argc, char** argv) {

    static const struct option long_options[] = {
        {"time",   required_argument, NULL, 't'},
        {"volume", required_argument, NULL, 'v'},
        {"info",   no_argument,       NULL, 'i'},
        {"help",   no_argument,       NULL, 'x'},
        {NULL,     0,                 NULL, 0  },
    };

    int seconds  = 10;
    int volume   = -1;
    bool infoend = false;

    int c;

    while ((c = getopt_long(argc, argv, "t:v:i", long_options, NULL)) != -1) {

        switch (c) {

            case 't':
                seconds = atoi(optarg);
                break;

            case 'v':
                volume = atoi(optarg);

                if (volume < 0 || volume > 100) {
                    fprintf(stderr, "audio-test: the volume must be between 0 and 100\n");
                    return 1;
                }

                break;

            case 'i':
                infoend = true;
                break;

            case 'x':
                show_usage();
                return 0;

            default:
                show_usage();
                return 1;
        }
    }


    if (optind >= argc) {
        printf("audio-test: no file given\n");
        return 1;
    }

    char* path = argv[optind];

    setvbuf(stdout, NULL, _IONBF, 0);

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    printf("audio-test: starting\n");

    wav_t wav;
    int wfd = -1;

    if (!wav_open(path, &wav, &wfd)) {
        printf("audio-test: %d/%d passed, %d failed\n", total - failures, total, failures);
        return 1;
    }

    dsp_t dsp;

    memset(&dsp, 0, sizeof(dsp));

    errno  = 0;
    dsp.fd = open(DSP_PATH, O_WRONLY);

    CHECK(dsp.fd >= 0, "open " DSP_PATH, "%d %s", dsp.fd, strerror(errno));

    if (dsp.fd < 0) {
        close(wfd);
        printf("audio-test: %d/%d passed, %d failed\n", total - failures, total, failures);
        return 1;
    }

    dsp_report(dsp.fd);
    dsp_errors(dsp.fd);
    dsp_setup(&dsp, &wav);
    mixer_setup(volume);

    if (!infoend) {
        play(wfd, &wav, &dsp, seconds);
    }

    close(dsp.fd);
    close(wfd);

    errno       = 0;
    const int r = ioctl(dsp.fd, SNDCTL_DSP_SYNC, 0);
    CHECK(r < 0 && errno == EBADF, "the closed descriptor is gone", "%d %s", r, strerror(errno));

    errno        = 0;
    const int re = open(DSP_PATH, O_WRONLY);
    CHECK(re >= 0, "the device can be reopened after close", "%d %s", re, strerror(errno));

    if (re >= 0) {
        close(re);
    }

    if (stopping) {
        printf("audio-test: interrupted\n");
    }

    printf("audio-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
