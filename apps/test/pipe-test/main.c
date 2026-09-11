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
 * Regression tests for pipes, readiness notification and poll().
 *
 * Every case here corresponds to a defect that was live in the kernel. Several of them used
 * to HANG rather than fail -- a reader that could never learn its writer was gone, a poll()
 * that slept on top of a buffer that was already full, a timeout that restarted from the top
 * on every wakeup. Run this under a timeout: a hang is a failure too.
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                       \
    {                                                                     \
        total++;                                                          \
        if (cond) {                                                       \
            printf("pipe-test: PASS  %s\n", (name));                      \
        } else {                                                          \
            failures++;                                                   \
            printf("pipe-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                 \
    }


/* CONFIG_PIPESIZ. A write larger than this is the case that used to be unsatisfiable:
   ringbuffer_write() was all-or-nothing, so the predicate "does it all fit" could never
   become true and sys_write() blocked and restarted forever. */
#define PIPE_CAPACITY 65535


static uint64_t now_ms(void) {

    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
        return 0;

    return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}


/*
 * A write bigger than the pipe buffer has to make partial progress. The reader drains
 * concurrently so the whole transfer can complete.
 */
static void test_large_write(void) {

    const size_t size = PIPE_CAPACITY * 4;

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "large write", "pipe() failed: %s", strerror(errno));
        return;
    }

    char* out = malloc(size);
    char* in  = malloc(size);

    if (!out || !in) {
        CHECK(0, "large write", "out of memory%s", "");
        return;
    }

    for (size_t i = 0; i < size; i++)
        out[i] = (char)(i & 0xFF);


    pid_t pid = fork();

    if (pid == 0) {

        close(fds[0]);

        size_t done = 0;

        while (done < size) {

            ssize_t e = write(fds[1], out + done, size - done);

            if (e <= 0)
                _exit(1);

            done += (size_t)e;
        }

        close(fds[1]);
        _exit(0);
    }


    close(fds[1]);

    size_t got = 0;

    while (got < size) {

        ssize_t e = read(fds[0], in + got, size - got);

        if (e < 0)
            break;

        if (e == 0)
            break;

        got += (size_t)e;
    }

    close(fds[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    CHECK(got == size && memcmp(out, in, size) == 0, "large write", "transferred %zu of %zu bytes", got, size);

    free(out);
    free(in);
}


/*
 * The canonical pipe idiom. Closing the write end has to surface as end of file on the read
 * end; it used to tear the shared buffer down and leave the survivor with -ENOSYS.
 */
static void test_eof_on_writer_close(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "eof on writer close", "pipe() failed: %s", strerror(errno));
        return;
    }

    if (write(fds[1], "hello", 5) != 5) {
        CHECK(0, "eof on writer close", "write() failed: %s", strerror(errno));
        return;
    }

    close(fds[1]);


    char buf[16];

    ssize_t first = read(fds[0], buf, sizeof(buf));

    /* Data written before the close still has to come out ... */
    CHECK(first == 5 && memcmp(buf, "hello", 5) == 0, "drain after writer close", "read() returned %zd (errno %d)", first, errno);

    /* ... and only then does the reader see the end of the stream. */
    errno         = 0;
    ssize_t empty = read(fds[0], buf, sizeof(buf));

    CHECK(empty == 0, "eof on writer close", "read() returned %zd, errno %d (%s)", empty, errno, strerror(errno));

    close(fds[0]);
}


/*
 * Writing into a pipe with no reader left is EPIPE. There was previously no reader count at
 * all, so this was indistinguishable from a full buffer and blocked forever.
 */
static void test_epipe(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "epipe", "pipe() failed: %s", strerror(errno));
        return;
    }

    close(fds[0]);

    errno     = 0;
    ssize_t e = write(fds[1], "x", 1);

    CHECK(e < 0 && errno == EPIPE, "epipe on reader close", "write() returned %zd, errno %d (%s)", e, errno, strerror(errno));

    close(fds[1]);
}


/*
 * Readiness has to be a property of the pipe, not of who was watching when the data landed.
 * Data written before poll() is called used to be invisible: the notify was gated on an
 * interest mask that the poller only set on its way to sleep.
 */
static void test_poll_sees_buffered_data(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "poll sees buffered data", "pipe() failed: %s", strerror(errno));
        return;
    }

    /* Written first, polled afterwards -- nobody was watching at the time. */
    if (write(fds[1], "a", 1) != 1) {
        CHECK(0, "poll sees buffered data", "write() failed: %s", strerror(errno));
        return;
    }

    struct pollfd pfd = {.fd = fds[0], .events = POLLIN, .revents = 0};

    int e = poll(&pfd, 1, 2000);

    CHECK(e == 1 && (pfd.revents & POLLIN), "poll sees buffered data", "poll() returned %d, revents 0x%x", e, pfd.revents);


    /* Readiness must not be consumed by observing it: a second poll has to say the same
       thing while the byte is still sitting there. */
    pfd.revents = 0;

    int again = poll(&pfd, 1, 2000);

    CHECK(again == 1 && (pfd.revents & POLLIN), "poll readiness is not consumed", "second poll() returned %d, revents 0x%x", again, pfd.revents);

    close(fds[0]);
    close(fds[1]);
}


/*
 * A zero timeout is a readiness probe. It used to pass a NULL deadline to the futex layer
 * and block forever.
 */
static void test_poll_zero_timeout(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "poll zero timeout", "pipe() failed: %s", strerror(errno));
        return;
    }

    struct pollfd pfd = {.fd = fds[0], .events = POLLIN, .revents = 0};

    uint64_t start = now_ms();
    int e          = poll(&pfd, 1, 0);
    uint64_t took  = now_ms() - start;

    CHECK(e == 0 && took < 500, "poll zero timeout", "poll() returned %d after %llu ms", e, (unsigned long long)took);

    close(fds[0]);
    close(fds[1]);
}


/*
 * A positive timeout has to actually come due. The deadline was rebuilt from scratch on every
 * syscall restart, so poll() re-armed a full timeout each time it woke and never returned 0.
 */
static void test_poll_timeout_expires(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "poll timeout expires", "pipe() failed: %s", strerror(errno));
        return;
    }

    struct pollfd pfd = {.fd = fds[0], .events = POLLIN, .revents = 0};

    uint64_t start = now_ms();
    int e          = poll(&pfd, 1, 300);
    uint64_t took  = now_ms() - start;

    CHECK(e == 0 && took >= 250 && took < 5000, "poll timeout expires", "poll() returned %d after %llu ms", e, (unsigned long long)took);

    close(fds[0]);
    close(fds[1]);
}


/*
 * A hung-up peer has to be reportable. POLLHUP is never in the caller's events mask, and the
 * scan used to test revents against that mask, so a hangup could never match and the poller
 * slept on a dead pipe forever.
 */
static void test_poll_hup(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "poll hup", "pipe() failed: %s", strerror(errno));
        return;
    }

    close(fds[1]);

    struct pollfd pfd = {.fd = fds[0], .events = POLLIN, .revents = 0};

    int e = poll(&pfd, 1, 2000);

    CHECK(e == 1 && (pfd.revents & POLLHUP), "poll reports hangup", "poll() returned %d, revents 0x%x", e, pfd.revents);

    close(fds[0]);
}


/*
 * A negative fd is how callers park a slot they are not interested in. POSIX says skip it
 * with revents cleared; it used to be rejected outright with EBADF.
 */
static void test_poll_negative_fd(void) {

    struct pollfd pfd[2];

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "poll negative fd", "pipe() failed: %s", strerror(errno));
        return;
    }

    if (write(fds[1], "z", 1) != 1) {
        CHECK(0, "poll negative fd", "write() failed: %s", strerror(errno));
        return;
    }

    pfd[0].fd      = -1;
    pfd[0].events  = POLLIN;
    pfd[0].revents = 0xFFFF;

    pfd[1].fd      = fds[0];
    pfd[1].events  = POLLIN;
    pfd[1].revents = 0;

    int e = poll(pfd, 2, 2000);

    CHECK(e == 1 && pfd[0].revents == 0 && (pfd[1].revents & POLLIN), "poll skips negative fd", "poll() returned %d, revents 0x%x / 0x%x", e, pfd[0].revents, pfd[1].revents);

    close(fds[0]);
    close(fds[1]);
}


/*
 * O_CLOEXEC was written into the descriptor's open flags, but execve() consults a separate
 * close_on_exec bit that only fcntl(F_SETFD) ever set -- so the flag was silently ignored.
 */
static void test_cloexec(void) {

    int fds[2];

    if (pipe2(fds, O_CLOEXEC) < 0) {
        CHECK(0, "pipe2 O_CLOEXEC", "pipe2() failed: %s", strerror(errno));
        return;
    }

    int flags = fcntl(fds[0], F_GETFD);

    CHECK(flags >= 0 && (flags & FD_CLOEXEC), "pipe2 O_CLOEXEC recorded", "F_GETFD returned %d", flags);

    close(fds[0]);
    close(fds[1]);
}


/*
 * mknod() tested the file type bits with & instead of comparing them against S_IFMT, so
 * S_IFREG (0100000) matched the S_IFSOCK (0140000) arm and creating an ordinary file failed.
 */
static void test_mknod_regular_file(void) {

    const char* path = "/tmp/pipe-test-mknod";

    unlink(path);

    errno = 0;
    int e = mknod(path, S_IFREG | 0644, 0);

    CHECK(e == 0, "mknod regular file", "mknod() returned %d, errno %d (%s)", e, errno, strerror(errno));

    if (e == 0)
        unlink(path);
}


static const struct {
    const char* name;
    void (*fn)(void);
} cases[] = {
    {"large-write", test_large_write},
    {"eof", test_eof_on_writer_close},
    {"epipe", test_epipe},
    {"poll-buffered", test_poll_sees_buffered_data},
    {"poll-zero", test_poll_zero_timeout},
    {"poll-timeout", test_poll_timeout_expires},
    {"poll-hup", test_poll_hup},
    {"poll-negative", test_poll_negative_fd},
    {"cloexec", test_cloexec},
    {"mknod", test_mknod_regular_file},
};


/*
 * With no argument every case runs. Naming one runs just that case, which is how these get
 * checked against an unfixed kernel: several of the defects hang instead of failing, and a
 * hang in one case would otherwise take the whole run down with it.
 */
int main(int argc, char** argv) {

    /* Unbuffered: a case that hangs would otherwise take its own output down with it, and
       several of these defects hang rather than fail. */
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("pipe-test: starting\n");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {

        if (argc > 1 && strcmp(argv[1], cases[i].name) != 0)
            continue;

        cases[i].fn();
    }

    printf("pipe-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
