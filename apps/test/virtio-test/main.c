/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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

/*
 * Tests for the four virtio devices from the far side of the syscall boundary.
 *
 * There is one device node per driver -- /dev/hwrng, /dev/hvc0, /dev/tablet and /dev/fb0 --
 * and what is checked here is the contract each one offers a process, not the virtqueue
 * plumbing under it. That boundary is where the interesting defects have been: a transfer
 * larger than a driver's window is split across descriptors and a mistake in the split shows
 * up as a short read, torn data or a queue that runs dry after a few hundred requests, none
 * of which the driver notices itself.
 *
 * Every group skips itself when its device is missing, so this is safe to run on a machine
 * configured without virtio. `scripts/run-qemu` attaches all four by default.
 *
 * With no argument every group runs. Naming one -- random, console, input, gpu -- runs just
 * that group. The input group needs a pointer to actually move, so it consumes what is
 * already buffered and skips when that is nothing; `virtio-test input 5` waits five seconds
 * for a human instead.
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <aplus/events.h>
#include <aplus/fb.h>


static int failures = 0;
static int skipped  = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                          \
    {                                                                        \
        total++;                                                             \
        if (cond) {                                                          \
            printf("virtio-test: PASS  %s\n", (name));                       \
        } else {                                                             \
            failures++;                                                      \
            printf("virtio-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                    \
    }

#define SKIP(name, fmt, ...)                                             \
    {                                                                    \
        skipped++;                                                       \
        printf("virtio-test: SKIP  %s: " fmt "\n", (name), __VA_ARGS__); \
    }


/* The windows the drivers split a transfer at. Neither is visible to a process, which is the
   point: a read or a write larger than one has to come back whole anyway, and the cases below
   pick their sizes to land several windows past the boundary rather than just over it. */

#define VIRTRANDOM_RECV_WINDOW  8192
#define VIRTCONSOLE_SEND_WINDOW 4096


static void sleep_ms(unsigned ms) {

    struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (long)(ms % 1000) * 1000000L};

    nanosleep(&ts, NULL);
}


static int is_zeroed(const uint8_t* p, size_t size) {

    for (size_t i = 0; i < size; i++) {
        if (p[i])
            return 0;
    }

    return 1;
}


/*
 * /dev/hwrng, the entropy source.
 *
 * The device hands over one window of bytes per request and the driver loops until the read is
 * satisfied, so the cases that matter are the ones that do not fit in a single window: a loop
 * that miscounts leaves the tail of the buffer untouched, and a caller has no way to tell that
 * from entropy that happens to be zero -- except in bulk, which is what is checked here.
 */

static void test_random(void) {

    int fd = open("/dev/hwrng", O_RDONLY);

    if (fd < 0) {
        SKIP("random", "cannot open /dev/hwrng: %s", strerror(errno));
        return;
    }


    uint8_t a[32];
    uint8_t b[32];

    memset(a, 0, sizeof(a));
    memset(b, 0, sizeof(b));

    ssize_t ra = read(fd, a, sizeof(a));
    ssize_t rb = read(fd, b, sizeof(b));

    CHECK(ra == (ssize_t)sizeof(a), "random-read", "read() returned %d, expected %d (%s)", (int)ra, (int)sizeof(a), strerror(errno));
    CHECK(!is_zeroed(a, sizeof(a)), "random-nonzero", "%s", "32 bytes of entropy came back all zero");

    /* Two reads of the same size colliding is a 1-in-2^256 event, so this failing means the
       device is handing back the same buffer twice rather than that luck ran out. */
    if (ra == (ssize_t)sizeof(a) && rb == (ssize_t)sizeof(b)) {
        CHECK(memcmp(a, b, sizeof(a)) != 0, "random-varies", "%s", "two reads returned identical bytes");
    }


    uint8_t one = 0;

    ssize_t r1 = read(fd, &one, 1);

    CHECK(r1 == 1, "random-single", "a 1-byte read returned %d (%s)", (int)r1, strerror(errno));


    const size_t big = VIRTRANDOM_RECV_WINDOW * 4;

    uint8_t* buf = calloc(1, big);

    if (buf) {

        ssize_t rbig = read(fd, buf, big);

        CHECK(rbig == (ssize_t)big, "random-large", "a %d-byte read returned %d (%s)", (int)big, (int)rbig, strerror(errno));

        if (rbig == (ssize_t)big) {

            /* Per block rather than over the whole buffer: a loop that stops after the first
               window leaves everything past it zero, and a single "is any byte set" test
               passes on that. A legitimately zero 256-byte block is a 1-in-2^2048 event. */
            size_t empty = 0;

            for (size_t off = 0; off < big; off += 256) {
                if (is_zeroed(buf + off, 256))
                    empty++;
            }

            CHECK(empty == 0, "random-large-entropy", "%d of %d blocks past the window boundary came back zero", (int)empty, (int)(big / 256));
        }

        free(buf);

    } else {
        SKIP("random-large", "%s", "out of memory");
    }


    /* An entropy source is read-only, and says so rather than quietly accepting the bytes.
     *
     * On a descriptor of its own, opened for writing. Writing to the O_RDONLY descriptor above
     * would be refused by sys_write() before the driver was reached at all -- as EPERM, which
     * is exactly what the device itself would look like if it reported its refusal the wrong
     * way, so reusing that descriptor here would make this case pass or fail for a reason that
     * has nothing to do with the device. */

    int wfd = open("/dev/hwrng", O_WRONLY);

    if (wfd < 0) {

        SKIP("random-write", "cannot open /dev/hwrng for writing: %s", strerror(errno));

    } else {

        errno      = 0;
        ssize_t rw = write(wfd, "x", 1);

        CHECK(rw < 0 && errno == ENOSPC, "random-write", "write() returned %d, errno %d (expected -1, ENOSPC)", (int)rw, errno);

        close(wfd);
    }


    errno  = 0;
    int io = ioctl(fd, 0x1234, NULL);

    CHECK(io < 0 && errno == ENOSYS, "random-ioctl", "ioctl() returned %d, errno %d (expected -1, ENOSYS)", io, errno);

    close(fd);
}


/*
 * /dev/hvc0, the console port.
 *
 * Writes are the half with a host on the other end -- under run-qemu they land in console.log
 * -- and each one is split at the send window and handed to the transmit queue a descriptor at
 * a time. A descriptor that is not returned to the pool leaves the queue a little smaller than
 * it was, which one write never shows: it takes a few hundred before the pool runs dry and the
 * port stops talking. That is what the burst case is for.
 *
 * Reads are here because the port used to have none at all: its receive queue was never given
 * a buffer, so the device had nowhere to put input and dropped it. A port with a stocked queue
 * and nothing waiting reports zero bytes, which is also what a port with no queue reports --
 * the difference is only visible with a host that sends something, so what this can check is
 * the weaker property that a read is answered at all rather than refused.
 */

static void test_console(void) {

    int fd = open("/dev/hvc0", O_RDWR | O_NONBLOCK);

    if (fd < 0) {
        SKIP("console", "cannot open /dev/hvc0: %s", strerror(errno));
        return;
    }


    static const char msg[] = "virtio-test: hello from /dev/hvc0\n";

    ssize_t w = write(fd, msg, sizeof(msg) - 1);

    CHECK(w == (ssize_t)(sizeof(msg) - 1), "console-write", "write() returned %d, expected %d (%s)", (int)w, (int)(sizeof(msg) - 1), strerror(errno));


    ssize_t w0 = write(fd, msg, 0);

    CHECK(w0 == 0, "console-write-zero", "a 0-byte write returned %d (%s)", (int)w0, strerror(errno));


    const size_t big = VIRTCONSOLE_SEND_WINDOW * 4;

    char* buf = malloc(big);

    if (buf) {

        memset(buf, '.', big);
        buf[big - 1] = '\n';

        ssize_t wbig = write(fd, buf, big);

        CHECK(wbig == (ssize_t)big, "console-write-large", "a %d-byte write returned %d (%s)", (int)big, (int)wbig, strerror(errno));

        free(buf);

    } else {
        SKIP("console-write-large", "%s", "out of memory");
    }


    /* Deep enough to outlast the queue, which is the whole point.
     *
     * The queue holds 128 descriptors and the host drains it from a thread of its own, so a
     * process in a tight write() loop gets ahead of it within a few hundred sends and finds
     * every descriptor still in flight. That is an ordinary full queue and the write has to
     * wait for room; what it used to do was fail, and because a fire and forget descriptor
     * only ever returns to the pool when the next caller collects it, the caller being turned
     * away was the one that would have collected it. The port stopped transmitting for the
     * rest of the boot -- not just for this process -- partway through the burst.
     *
     * 512 is chosen to be comfortably past 128 so the case has to survive the queue filling
     * and draining several times over rather than merely fitting. */

    int burst_failed = 0;

    for (int i = 0; i < 512; i++) {

        if (write(fd, "virtio-test\n", 12) != 12) {
            burst_failed = i + 1;
            break;
        }
    }

    CHECK(burst_failed == 0, "console-write-burst", "write %d of 512 failed: %s", burst_failed, strerror(errno));


    /* Non-blocking on purpose. A port whose queue is stocked and empty reports zero bytes
       rather than EAGAIN, so this does not actually sleep -- but a regression that turned the
       zero into an EAGAIN would hang the whole run on a blocking descriptor instead of
       failing here. */
    char in[64];

    errno     = 0;
    ssize_t r = read(fd, in, sizeof(in));

    CHECK(r >= 0 || errno == EAGAIN, "console-read", "read() returned %d, errno %d (expected a byte count, or EAGAIN)", (int)r, errno);

    close(fd);
}


/*
 * /dev/tablet, the absolute pointer.
 *
 * Events arrive as event_t structures through a ring buffer the driver fills from interrupt
 * context, and the one thing a reader is promised about them is the scale: a driver normalizes
 * whatever range its device reports onto EV_ABS_MIN..EV_ABS_MAX, so a reader can map a
 * coordinate onto the screen knowing only the screen size. A driver that forwards raw device
 * coordinates instead puts the pointer in the wrong place on every machine but its own, and
 * that is what the bounds check below is for.
 *
 * Waiting for a pointer to move is not something an automated run can do, so by default this
 * validates whatever is already buffered and skips when there is none.
 */

static void test_input(int wait_seconds) {

    int fd = open("/dev/tablet", O_RDONLY | O_NONBLOCK);

    if (fd < 0) {
        SKIP("input", "cannot open /dev/tablet: %s", strerror(errno));
        return;
    }


    /* A device inode has no poll operation, so vfs_poll() answers "ready" to everything it is
       asked about -- including a pointer that has not moved since boot. This is not a bug
       being reported, it is the reason the display server reads these devices from a thread
       of its own rather than folding them into a select() loop, and it is worth a case so
       that giving them a real poll operation one day is a deliberate change rather than a
       silent one that breaks nothing visibly. */

    struct pollfd pfd = {.fd = fd, .events = POLLIN};

    int pr = poll(&pfd, 1, 0);

    CHECK(pr == 1 && (pfd.revents & POLLIN), "input-poll-always-ready", "poll() returned %d, revents 0x%x (expected 1, POLLIN: device inodes have no poll op)", pr, pfd.revents);


    event_t events[32];

    size_t count = 0;

    unsigned deadline = (wait_seconds > 0) ? (unsigned)wait_seconds * 1000u : 0u;

    for (unsigned waited = 0;;) {

        errno     = 0;
        ssize_t e = read(fd, &events[count], sizeof(event_t));

        if (e == (ssize_t)sizeof(event_t)) {

            if (++count == sizeof(events) / sizeof(events[0]))
                break;

            continue;
        }

        if (e < 0 && errno != EAGAIN) {
            CHECK(0, "input-read", "read() failed with errno %d (%s)", errno, strerror(errno));
            close(fd);
            return;
        }

        if (e >= 0 && e != (ssize_t)sizeof(event_t)) {
            CHECK(0, "input-read", "read() returned %d, expected %d or EAGAIN", (int)e, (int)sizeof(event_t));
            close(fd);
            return;
        }

        if (count || waited >= deadline)
            break;

        sleep_ms(100);
        waited += 100;
    }


    if (count == 0) {
        SKIP("input-events", "%s", "no pointer events buffered; run `virtio-test input 5` and move the pointer");
        close(fd);
        return;
    }

    CHECK(1, "input-read", "%s", "");


    size_t bad_type = 0;
    size_t bad_axis = 0;
    size_t abs_seen = 0;

    for (size_t i = 0; i < count; i++) {

        switch (events[i].ev_type) {

            case EV_ABS:

                abs_seen++;

                /* Only the lower bound is worth testing: vaxis_t is int16_t and EV_ABS_MAX is
                   exactly what it holds, so the scale's upper end is enforced by the type and
                   the one way out of range is negative. That is also the shape the bug takes
                   -- a driver that forwards a raw coordinate, or scales with signed
                   arithmetic that wraps, lands below zero rather than above 32767. */

                if (events[i].ev_abs.x < EV_ABS_MIN || events[i].ev_abs.y < EV_ABS_MIN)
                    bad_axis++;

                break;

            case EV_SYN:
            case EV_KEY:
            case EV_REL:
            case EV_MSC:
                break;

            default:
                bad_type++;
                break;
        }
    }

    CHECK(bad_type == 0, "input-event-type", "%d of %d events carried an unknown ev_type", (int)bad_type, (int)count);

    if (abs_seen) {
        CHECK(bad_axis == 0, "input-abs-range", "%d of %d absolute events carried a coordinate below EV_ABS_MIN (%d)", (int)bad_axis, (int)abs_seen, EV_ABS_MIN);
    } else {
        SKIP("input-abs-range", "%s", "none of the buffered events were absolute; move the pointer rather than clicking");
    }

    close(fd);
}


/*
 * /dev/fb0, the display adapter.
 *
 * The framebuffer is reached by dereferencing fb_fix_screeninfo.smem_start directly: it is a
 * physical address the driver identity-mapped with the user bit set, and there is no mapping
 * for a process to make. read(), write() and mmap() all refuse the node, so a process that
 * went looking for the usual framebuffer interface would find nothing and has to be told that
 * this is the arrangement -- hence the three cases that assert the refusals.
 *
 * The rest is FBIO_FLUSH and the hardware cursor. Both take a rectangle or an image from
 * userspace and both clamp what they are given against the current mode, which is the only
 * thing standing between a bad argument and the adapter reading out of memory that is not the
 * framebuffer. Those guards are cheap to break and invisible when broken, so each one gets a
 * case that hands it something out of range and expects a refusal rather than a crash.
 */

static void test_gpu(void) {

    int fd = open("/dev/fb0", O_RDWR);

    if (fd < 0) {
        SKIP("gpu", "cannot open /dev/fb0: %s", strerror(errno));
        return;
    }


    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;

    memset(&var, 0, sizeof(var));
    memset(&fix, 0, sizeof(fix));

    int gv = ioctl(fd, FBIOGET_VSCREENINFO, &var);
    int gf = ioctl(fd, FBIOGET_FSCREENINFO, &fix);

    CHECK(gv == 0, "gpu-vscreeninfo", "FBIOGET_VSCREENINFO returned %d (%s)", gv, strerror(errno));
    CHECK(gf == 0, "gpu-fscreeninfo", "FBIOGET_FSCREENINFO returned %d (%s)", gf, strerror(errno));

    if (gv < 0 || gf < 0) {
        close(fd);
        return;
    }


    /* Everything below this point is the framebuffer contract that any adapter owes; the id
       only decides whether a failure here is a virtio-gpu problem or somebody else's. */
    int is_virtio = (strncmp(fix.id, "VIRTIO-GPU", sizeof(fix.id)) == 0);

    printf("virtio-test: /dev/fb0 is '%.16s', %ux%u at %u bpp, pitch %u\n", fix.id, var.xres, var.yres, var.bits_per_pixel, fix.line_length);

    if (!is_virtio)
        SKIP("gpu-is-virtio", "%s", "/dev/fb0 is not the virtio adapter; the cases below still apply to whatever is there");


    CHECK(var.xres && var.yres, "gpu-mode", "mode is %ux%u, expected both nonzero", var.xres, var.yres);
    CHECK(fix.smem_start != 0, "gpu-smem-start", "%s", "framebuffer address is NULL");

    CHECK(fix.line_length >= var.xres * (var.bits_per_pixel / 8), "gpu-pitch", "pitch %u is shorter than a %u-pixel row at %u bpp", fix.line_length, var.xres, var.bits_per_pixel);

    CHECK((uint64_t)fix.smem_len >= (uint64_t)fix.line_length * var.yres, "gpu-smem-len", "framebuffer is %u bytes, a %u-row screen of pitch %u needs %llu", fix.smem_len, var.yres, fix.line_length,
          (unsigned long long)fix.line_length * var.yres);


    char probe[4];

    errno     = 0;
    ssize_t r = read(fd, probe, sizeof(probe));

    CHECK(r < 0 && errno == ENOSYS, "gpu-no-read", "read() returned %d, errno %d (expected -1, ENOSYS)", (int)r, errno);

    errno     = 0;
    ssize_t w = write(fd, probe, sizeof(probe));

    CHECK(w < 0 && errno == ENOSYS, "gpu-no-write", "write() returned %d, errno %d (expected -1, ENOSYS)", (int)w, errno);


    errno     = 0;
    void* map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    CHECK(map == MAP_FAILED && errno == ENOTSUP, "gpu-no-mmap", "mmap(MAP_SHARED) returned %p, errno %d (expected MAP_FAILED, ENOTSUP)", map, errno);

    if (map != MAP_FAILED)
        munmap(map, 4096);


    /* In a child: if smem_start ever stops being reachable from userspace this faults, and a
       fault here would otherwise take the rest of the run with it. */
    if (var.bits_per_pixel == 32 && fix.smem_start && fix.line_length) {

        pid_t pid = fork();

        if (pid == 0) {

            volatile uint32_t* fb = (volatile uint32_t*)(uintptr_t)fix.smem_start;

            /* Bottom right corner, put back straight away: the display server owns this
               screen and is very likely painting it while this runs. */
            size_t off = ((size_t)(var.yres - 1) * fix.line_length) / sizeof(uint32_t) + (var.xres - 1);

            uint32_t saved = fb[off];

            fb[off] = 0x00A5A5A5;

            uint32_t got = fb[off];

            fb[off] = saved;

            _exit(got == 0x00A5A5A5 ? 0 : 1);
        }

        if (pid < 0) {

            SKIP("gpu-framebuffer", "fork() failed: %s", strerror(errno));

        } else {

            int status = 0;

            waitpid(pid, &status, 0);

            CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "gpu-framebuffer", "writing a pixel through smem_start %s", WIFEXITED(status) ? "read back wrong" : "faulted");
        }

    } else {
        SKIP("gpu-framebuffer", "%s", "the pixel probe assumes 32 bpp");
    }


    struct fb_rect rect = {.x = 0, .y = 0, .width = (var.xres < 64) ? var.xres : 64, .height = (var.yres < 64) ? var.yres : 64};

    int f1 = ioctl(fd, FBIO_FLUSH, &rect);

    CHECK(f1 == 0, "gpu-flush", "FBIO_FLUSH of %ux%u at the origin returned %d (%s)", rect.width, rect.height, f1, strerror(errno));


    struct fb_rect empty = {.x = 0, .y = 0, .width = 0, .height = 0};

    int f2 = ioctl(fd, FBIO_FLUSH, &empty);

    CHECK(f2 == 0, "gpu-flush-empty", "FBIO_FLUSH of an empty rectangle returned %d (%s)", f2, strerror(errno));


    /* Runs off both edges: the driver is expected to clip it back to the mode, not to transfer
       a rectangle that walks off the end of the framebuffer. */
    struct fb_rect over = {.x = var.xres / 2, .y = var.yres / 2, .width = var.xres, .height = var.yres};

    int f3 = ioctl(fd, FBIO_FLUSH, &over);

    CHECK(f3 == 0, "gpu-flush-clip", "FBIO_FLUSH of a rectangle overhanging the mode returned %d (%s)", f3, strerror(errno));


    struct fb_rect off = {.x = var.xres, .y = var.yres, .width = 16, .height = 16};

    int f4 = ioctl(fd, FBIO_FLUSH, &off);

    CHECK(f4 == 0, "gpu-flush-offscreen", "FBIO_FLUSH of a rectangle starting past the mode returned %d (%s)", f4, strerror(errno));


    uint32_t crtc = 0;

    int vs = ioctl(fd, FBIO_WAITFORVSYNC, &crtc);

    CHECK(vs == 0, "gpu-vsync", "FBIO_WAITFORVSYNC returned %d (%s)", vs, strerror(errno));


    struct fb_hwcinfo hwc;

    memset(&hwc, 0, sizeof(hwc));

    errno  = 0;
    int gh = ioctl(fd, FBIOGET_HWCINFO, &hwc);

    CHECK(gh == 0 || errno == ENOTSUP, "gpu-hwcinfo", "FBIOGET_HWCINFO returned %d, errno %d (expected 0, or ENOTSUP on an adapter with no cursor plane)", gh, errno);

    if (gh == 0) {

        CHECK(hwc.max_width && hwc.max_height, "gpu-hwcinfo-size", "cursor plane reports a maximum of %ux%u, expected both nonzero", hwc.max_width, hwc.max_height);

        /* Only the rejections. A cursor that was actually set would replace the pointer the
           display server put there, and none of these get far enough to touch the plane. */

        struct fb_hwcursor cursor;

        memset(&cursor, 0, sizeof(cursor));

        cursor.flags  = FB_HWCURSOR_ENABLE;
        cursor.width  = 16;
        cursor.height = 16;
        cursor.image  = NULL;

        errno  = 0;
        int c1 = ioctl(fd, FBIOPUT_HWCURSOR, &cursor);

        CHECK(c1 < 0 && errno == EINVAL, "gpu-hwcursor-no-image", "a visible cursor with no image returned %d, errno %d (expected -1, EINVAL)", c1, errno);


        uint32_t image[16 * 16];

        memset(image, 0, sizeof(image));

        cursor.image = image;
        cursor.hot_x = cursor.width;
        cursor.hot_y = 0;

        errno  = 0;
        int c2 = ioctl(fd, FBIOPUT_HWCURSOR, &cursor);

        CHECK(c2 < 0 && errno == EINVAL, "gpu-hwcursor-hotspot", "a hotspot outside the image returned %d, errno %d (expected -1, EINVAL)", c2, errno);


        cursor.hot_x  = 0;
        cursor.width  = hwc.max_width + 1;
        cursor.height = hwc.max_height + 1;

        errno  = 0;
        int c3 = ioctl(fd, FBIOPUT_HWCURSOR, &cursor);

        CHECK(c3 < 0 && errno == EINVAL, "gpu-hwcursor-too-large", "a %ux%u cursor on a plane that tops out at %ux%u returned %d, errno %d (expected -1, EINVAL)", cursor.width, cursor.height, hwc.max_width, hwc.max_height, c3, errno);


        /* Moving it is harmless: the display server puts it back on the next pointer event. */
        struct fb_hwcursor_pos pos = {.x = 0, .y = 0};

        errno  = 0;
        int c4 = ioctl(fd, FBIOPUT_HWCURSOR_POS, &pos);

        CHECK(c4 == 0 || errno == ENOTSUP, "gpu-hwcursor-move", "FBIOPUT_HWCURSOR_POS returned %d, errno %d (expected 0, or ENOTSUP)", c4, errno);

    } else {
        SKIP("gpu-hwcursor", "%s", "this adapter has no hardware cursor plane");
    }


    errno   = 0;
    int bad = ioctl(fd, 0x7FFF, NULL);

    CHECK(bad < 0 && errno == ENOSYS, "gpu-bad-ioctl", "an unknown ioctl returned %d, errno %d (expected -1, ENOSYS)", bad, errno);

    close(fd);
}


static int input_wait_seconds = 0;

static void run_input(void) {
    test_input(input_wait_seconds);
}


static struct {

    const char* name;
    void (*fn)(void);

} groups[] = {

    {"random",  test_random },
    {"console", test_console},
    {"input",   run_input   },
    {"gpu",     test_gpu    },
};


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc > 2)
        input_wait_seconds = atoi(argv[2]);

    printf("virtio-test: starting\n");

    int matched = 0;

    for (size_t i = 0; i < sizeof(groups) / sizeof(groups[0]); i++) {

        if (argc > 1 && strcmp(argv[1], groups[i].name) != 0)
            continue;

        matched++;

        groups[i].fn();
    }

    if (!matched) {
        fprintf(stderr, "virtio-test: no such group '%s' (try: random, console, input, gpu)\n", argv[1]);
        return 2;
    }

    printf("virtio-test: %d/%d passed, %d failed, %d skipped\n", total - failures, total, failures, skipped);

    return failures ? 1 : 0;
}
