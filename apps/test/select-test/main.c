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
 * Regression tests for select(), pselect6() and ppoll().
 *
 * All three sit on the same readiness core as poll(), so most of what is checked here is the
 * part that is not shared: select()'s three in-out descriptor sets. A syscall that sleeps in
 * this kernel is restarted from the top with its original arguments, which makes those sets
 * treacherous -- they are the question and the answer in the same memory. An implementation
 * that writes the answer before going to sleep destroys the question, and the restarted call
 * then watches nothing at all. That failure HANGS rather than returning wrong, so run this
 * under a timeout: a hang is a failure too.
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                         \
    {                                                                       \
        total++;                                                            \
        if (cond) {                                                         \
            printf("select-test: PASS  %s\n", (name));                      \
        } else {                                                            \
            failures++;                                                     \
            printf("select-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                   \
    }


/* How much of a sigset_t either side actually means. The type is 128 bytes, but every syscall
   that carries one also carries a length, and both the kernel and the libc only ever fill in
   that many -- sigemptyset() does not clear the rest either. Comparing whole sigset_t objects
   compares stack litter. */
#define SIGMASK_BYTES (_NSIG / 8)


static uint64_t now_ms(void) {

    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
        return 0;

    return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}


static void sleep_ms(unsigned ms) {

    struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (long)(ms % 1000) * 1000000L};

    nanosleep(&ts, NULL);
}


/*
 * Data already in the pipe has to be reported without sleeping: readiness is a question about
 * now, not a subscription to the next change.
 */
static void test_readable(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "readable", "pipe() failed: %s", strerror(errno));
        return;
    }

    write(fds[1], "x", 1);


    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fds[0], &rfds);

    struct timeval tv = {0, 0};

    int r = select(fds[0] + 1, &rfds, NULL, NULL, &tv);

    CHECK(r == 1, "readable", "select() returned %d (%s), expected 1", r, strerror(errno));
    CHECK(r == 1 && FD_ISSET(fds[0], &rfds), "readable-bit", "%s", "the ready descriptor is not set in the result");

    close(fds[0]);
    close(fds[1]);
}


/*
 * An empty pipe has a writable write end and an unreadable read end. Both answers come from
 * the same scan, so this also pins down that the sets are kept apart.
 */
static void test_writable(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "writable", "pipe() failed: %s", strerror(errno));
        return;
    }


    fd_set rfds, wfds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_SET(fds[0], &rfds);
    FD_SET(fds[1], &wfds);

    struct timeval tv = {0, 0};

    int n = (fds[0] > fds[1] ? fds[0] : fds[1]) + 1;
    int r = select(n, &rfds, &wfds, NULL, &tv);

    CHECK(r == 1, "writable", "select() returned %d (%s), expected 1", r, strerror(errno));
    CHECK(r >= 0 && FD_ISSET(fds[1], &wfds), "writable-bit", "%s", "the write end is not reported writable");
    CHECK(r >= 0 && !FD_ISSET(fds[0], &rfds), "writable-not-readable", "%s", "an empty pipe is reported readable");

    close(fds[0]);
    close(fds[1]);
}


/*
 * A zero timeout is a probe. Nothing is ready, so the answer is zero and all three sets come
 * back empty rather than untouched.
 */
static void test_zero_timeout(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "zero-timeout", "pipe() failed: %s", strerror(errno));
        return;
    }


    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fds[0], &rfds);

    struct timeval tv = {0, 0};

    uint64_t t0 = now_ms();
    int r       = select(fds[0] + 1, &rfds, NULL, NULL, &tv);
    uint64_t dt = now_ms() - t0;

    CHECK(r == 0, "zero-timeout", "select() returned %d (%s), expected 0", r, strerror(errno));
    CHECK(dt < 100, "zero-timeout-immediate", "a probe took %llu ms", (unsigned long long)dt);
    CHECK(!FD_ISSET(fds[0], &rfds), "zero-timeout-cleared", "%s", "the set was not cleared on timeout");

    close(fds[0]);
    close(fds[1]);
}


/*
 * A timeout has to actually come due. The deadline is stamped once and counted down across
 * restarts; one recomputed from the top on every wakeup would never expire, and this would
 * hang instead of failing.
 */
static void test_timeout_expires(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "timeout", "pipe() failed: %s", strerror(errno));
        return;
    }


    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fds[0], &rfds);

    struct timeval tv = {0, 300000};

    uint64_t t0 = now_ms();
    int r       = select(fds[0] + 1, &rfds, NULL, NULL, &tv);
    uint64_t dt = now_ms() - t0;

    CHECK(r == 0, "timeout", "select() returned %d (%s), expected 0", r, strerror(errno));
    CHECK(dt >= 250, "timeout-waited", "select() returned after %llu ms, expected about 300", (unsigned long long)dt);
    CHECK(dt < 3000, "timeout-not-overslept", "select() returned after %llu ms, expected about 300", (unsigned long long)dt);

    close(fds[0]);
    close(fds[1]);
}


/*
 * The case the whole design is arranged around. Nothing is ready, so select() sleeps and is
 * restarted when the writer arrives -- and the restart re-reads the very sets it would have
 * written its answer into. If they were written early, the second attempt watches an empty
 * set and this hangs until the timeout.
 */
static void test_blocks_until_data(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "blocks", "pipe() failed: %s", strerror(errno));
        return;
    }


    pid_t pid = fork();

    if (pid < 0) {
        CHECK(0, "blocks", "fork() failed: %s", strerror(errno));
        close(fds[0]);
        close(fds[1]);
        return;
    }

    if (pid == 0) {

        close(fds[0]);
        sleep_ms(250);
        write(fds[1], "x", 1);
        close(fds[1]);

        _exit(0);
    }

    close(fds[1]);


    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fds[0], &rfds);

    struct timeval tv = {5, 0};

    uint64_t t0 = now_ms();
    int r       = select(fds[0] + 1, &rfds, NULL, NULL, &tv);
    uint64_t dt = now_ms() - t0;

    CHECK(r == 1, "blocks", "select() returned %d (%s), expected 1", r, strerror(errno));
    CHECK(r == 1 && FD_ISSET(fds[0], &rfds), "blocks-bit", "%s", "woke up without reporting the descriptor");
    CHECK(dt < 4000, "blocks-woken-by-data", "select() took %llu ms, so it waited out the timeout instead of being woken", (unsigned long long)dt);

    int status;
    waitpid(pid, &status, 0);

    close(fds[0]);
}


/*
 * Only the descriptors that are actually ready come back set. A result built by clearing bits
 * in place rather than from an empty set tends to pass the single-descriptor cases above and
 * fail this one.
 */
static void test_reports_only_ready(void) {

    int a[2], b[2];

    if (pipe(a) < 0) {
        CHECK(0, "only-ready", "pipe() failed: %s", strerror(errno));
        return;
    }

    if (pipe(b) < 0) {
        CHECK(0, "only-ready", "pipe() failed: %s", strerror(errno));
        close(a[0]);
        close(a[1]);
        return;
    }

    write(a[1], "x", 1);


    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(a[0], &rfds);
    FD_SET(b[0], &rfds);

    struct timeval tv = {0, 0};

    int n = (a[0] > b[0] ? a[0] : b[0]) + 1;
    int r = select(n, &rfds, NULL, NULL, &tv);

    CHECK(r == 1, "only-ready", "select() returned %d (%s), expected 1", r, strerror(errno));
    CHECK(r >= 0 && FD_ISSET(a[0], &rfds), "only-ready-set", "%s", "the pipe with data is not reported");
    CHECK(r >= 0 && !FD_ISSET(b[0], &rfds), "only-ready-clear", "%s", "the empty pipe is reported ready");

    close(a[0]);
    close(a[1]);
    close(b[0]);
    close(b[1]);
}


/*
 * The return value counts ready bits, not descriptors: one that is both readable and writable
 * is worth two.
 */
static void test_counts_bits_not_fds(void) {

    int sv[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        CHECK(0, "count", "socketpair() failed: %s", strerror(errno));
        return;
    }

    write(sv[1], "x", 1);
    sleep_ms(50);


    fd_set rfds, wfds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_SET(sv[0], &rfds);
    FD_SET(sv[0], &wfds);

    struct timeval tv = {0, 0};

    int r = select(sv[0] + 1, &rfds, &wfds, NULL, &tv);

    CHECK(r == 2, "count", "select() returned %d (%s), expected 2 for one descriptor ready both ways", r, strerror(errno));

    close(sv[0]);
    close(sv[1]);
}


/*
 * select() has nowhere to report a bad descriptor per entry the way poll() does with POLLNVAL,
 * so a closed descriptor fails the whole call.
 */
static void test_bad_fd(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "bad-fd", "pipe() failed: %s", strerror(errno));
        return;
    }

    int fd = fds[0];

    close(fds[0]);
    close(fds[1]);


    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    struct timeval tv = {0, 0};

    errno = 0;

    int r = select(fd + 1, &rfds, NULL, NULL, &tv);

    CHECK(r < 0 && errno == EBADF, "bad-fd", "select() returned %d errno %d (%s), expected -1 EBADF", r, errno, strerror(errno));
}


/*
 * select() with no descriptors at all is a sleep, and has to come back on its own.
 */
static void test_sleep(void) {

    struct timeval tv = {0, 200000};

    uint64_t t0 = now_ms();
    int r       = select(0, NULL, NULL, NULL, &tv);
    uint64_t dt = now_ms() - t0;

    CHECK(r == 0, "sleep", "select() returned %d (%s), expected 0", r, strerror(errno));
    CHECK(dt >= 150, "sleep-waited", "select(0, ...) returned after %llu ms, expected about 200", (unsigned long long)dt);
    CHECK(dt < 3000, "sleep-not-overslept", "select(0, ...) returned after %llu ms, expected about 200", (unsigned long long)dt);
}


/*
 * nanosleep() goes through the scheduler's own deadline rather than the one the poll family
 * uses, so it needs checking on its own. The scheduler read the current time into a kernel
 * buffer through sys_clock_gettime(), which rejects a kernel pointer coming from a userspace
 * task and returned -EFAULT having written nothing: the deadline was then compared against
 * whatever the stack happened to hold, and sleeps ended after an arbitrary fraction of what
 * was asked for. `watch -n 2` ran its command several times a second.
 */
static void test_nanosleep(void) {

    struct timespec ts = {.tv_sec = 0, .tv_nsec = 400000000L};

    uint64_t t0 = now_ms();
    int r       = nanosleep(&ts, NULL);
    uint64_t dt = now_ms() - t0;

    CHECK(r == 0, "nanosleep", "nanosleep() returned %d (%s), expected 0", r, strerror(errno));
    CHECK(dt >= 350, "nanosleep-waited", "nanosleep(400ms) returned after %llu ms", (unsigned long long)dt);
    CHECK(dt < 3000, "nanosleep-not-overslept", "nanosleep(400ms) returned after %llu ms", (unsigned long long)dt);
}


/*
 * A descriptor above CONFIG_OPEN_MAX is a socket, and readiness for one is answered by the
 * network stack rather than by the VFS. Whether it calls an idle socket writable is its own
 * business; what has to hold is that select() and poll() come back with the same answer, since
 * select() reaches that path by a different route. Skipped where there is no socket to make.
 */
static void test_high_fd_socket(void) {

    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd < 0) {
        printf("select-test: SKIP  high-fd (no socket: %s)\n", strerror(errno));
        return;
    }


    struct pollfd pfd = {.fd = fd, .events = POLLIN | POLLOUT, .revents = 0};

    int pr = poll(&pfd, 1, 0);


    fd_set rfds, wfds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_SET(fd, &rfds);
    FD_SET(fd, &wfds);

    struct timeval tv = {0, 0};

    int sr = select(fd + 1, &rfds, &wfds, NULL, &tv);

    CHECK(pr >= 0, "high-fd-poll", "poll() on socket fd %d failed: %s", fd, strerror(errno));
    CHECK(sr >= 0, "high-fd", "select() on socket fd %d failed: %s", fd, strerror(errno));

    if (pr >= 0 && sr >= 0) {

        int in_poll = (pfd.revents & POLLIN) ? 1 : 0;
        int in_sel  = FD_ISSET(fd, &rfds) ? 1 : 0;

        int out_poll = (pfd.revents & POLLOUT) ? 1 : 0;
        int out_sel  = FD_ISSET(fd, &wfds) ? 1 : 0;

        CHECK(in_poll == in_sel, "high-fd-in", "poll() says readable=%d but select() says %d", in_poll, in_sel);
        CHECK(out_poll == out_sel, "high-fd-out", "poll() says writable=%d but select() says %d", out_poll, out_sel);
        CHECK(sr == in_sel + out_sel, "high-fd-count", "select() returned %d, expected %d ready bits", sr, in_sel + out_sel);
    }

    close(fd);
}


/*
 * select(FD_SETSIZE, ...) is a common shorthand for "everything". Descriptors that high cannot
 * exist here, so it has to be bounded rather than refused.
 */
static void test_fd_setsize(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "fd-setsize", "pipe() failed: %s", strerror(errno));
        return;
    }

    write(fds[1], "x", 1);


    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fds[0], &rfds);

    struct timeval tv = {0, 0};

    int r = select(FD_SETSIZE, &rfds, NULL, NULL, &tv);

    CHECK(r == 1, "fd-setsize", "select(FD_SETSIZE, ...) returned %d (%s), expected 1", r, strerror(errno));

    close(fds[0]);
    close(fds[1]);
}


/*
 * pselect() takes its timeout as a timespec and a signal mask it blocks for the duration. With
 * no mask it is select() with finer units.
 */
static void test_pselect(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "pselect", "pipe() failed: %s", strerror(errno));
        return;
    }

    write(fds[1], "x", 1);


    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fds[0], &rfds);

    struct timespec ts = {0, 0};

    int r = pselect(fds[0] + 1, &rfds, NULL, NULL, &ts, NULL);

    CHECK(r == 1, "pselect", "pselect() returned %d (%s), expected 1", r, strerror(errno));
    CHECK(r == 1 && FD_ISSET(fds[0], &rfds), "pselect-bit", "%s", "the ready descriptor is not set in the result");

    close(fds[0]);
    close(fds[1]);
}


/*
 * pselect() has to sleep and wake like select() does, restart hazard and all.
 */
static void test_pselect_blocks(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "pselect-blocks", "pipe() failed: %s", strerror(errno));
        return;
    }


    pid_t pid = fork();

    if (pid < 0) {
        CHECK(0, "pselect-blocks", "fork() failed: %s", strerror(errno));
        close(fds[0]);
        close(fds[1]);
        return;
    }

    if (pid == 0) {

        close(fds[0]);
        sleep_ms(250);
        write(fds[1], "x", 1);
        close(fds[1]);

        _exit(0);
    }

    close(fds[1]);


    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fds[0], &rfds);

    struct timespec ts = {5, 0};

    uint64_t t0 = now_ms();
    int r       = pselect(fds[0] + 1, &rfds, NULL, NULL, &ts, NULL);
    uint64_t dt = now_ms() - t0;

    CHECK(r == 1, "pselect-blocks", "pselect() returned %d (%s), expected 1", r, strerror(errno));
    CHECK(dt < 4000, "pselect-blocks-woken", "pselect() took %llu ms, so it waited out the timeout instead of being woken", (unsigned long long)dt);

    int status;
    waitpid(pid, &status, 0);

    close(fds[0]);
}


/*
 * The mask pselect() installs is for the duration of the call only. It is swapped back on the
 * way out -- including out of a call that slept and was restarted, which is where a saved mask
 * is easiest to lose.
 */
static void test_pselect_restores_sigmask(void) {

    sigset_t before, during, after;

    sigemptyset(&during);
    sigaddset(&during, SIGUSR1);

    if (sigprocmask(SIG_SETMASK, NULL, &before) < 0) {
        CHECK(0, "pselect-sigmask", "sigprocmask() failed: %s", strerror(errno));
        return;
    }


    /* Two calls, because they leave by different doors: one returns on its first attempt, the
       other sleeps and comes back through the restart path, where the mask to put back has to
       have survived in the meantime. */

    struct timespec immediate = {0, 0};

    pselect(0, NULL, NULL, NULL, &immediate, &during);

    if (sigprocmask(SIG_SETMASK, NULL, &after) < 0) {
        CHECK(0, "pselect-sigmask", "sigprocmask() failed: %s", strerror(errno));
        return;
    }

    CHECK(memcmp(&before, &after, SIGMASK_BYTES) == 0, "pselect-sigmask", "%s", "the mask was not restored after a pselect() that did not sleep");


    struct timespec slept = {0, 200000000};

    pselect(0, NULL, NULL, NULL, &slept, &during);

    if (sigprocmask(SIG_SETMASK, NULL, &after) < 0) {
        CHECK(0, "pselect-sigmask-slept", "sigprocmask() failed: %s", strerror(errno));
        return;
    }

    CHECK(memcmp(&before, &after, SIGMASK_BYTES) == 0, "pselect-sigmask-slept", "%s", "the mask was not restored after a pselect() that slept");
}


/*
 * ppoll() is poll() with a timespec and a mask. Same core, so this is about the wrapper.
 */
static void test_ppoll(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "ppoll", "pipe() failed: %s", strerror(errno));
        return;
    }

    write(fds[1], "x", 1);


    struct pollfd pfd = {.fd = fds[0], .events = POLLIN, .revents = 0};
    struct timespec ts = {0, 0};

    int r = ppoll(&pfd, 1, &ts, NULL);

    CHECK(r == 1, "ppoll", "ppoll() returned %d (%s), expected 1", r, strerror(errno));
    CHECK(r == 1 && (pfd.revents & POLLIN), "ppoll-revents", "revents was 0x%x, expected POLLIN", pfd.revents);

    close(fds[0]);
    close(fds[1]);
}


/*
 * ppoll()'s timeout has to come due like everyone else's.
 */
static void test_ppoll_timeout(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "ppoll-timeout", "pipe() failed: %s", strerror(errno));
        return;
    }


    struct pollfd pfd = {.fd = fds[0], .events = POLLIN, .revents = 0};
    struct timespec ts = {0, 300000000};

    uint64_t t0 = now_ms();
    int r       = ppoll(&pfd, 1, &ts, NULL);
    uint64_t dt = now_ms() - t0;

    CHECK(r == 0, "ppoll-timeout", "ppoll() returned %d (%s), expected 0", r, strerror(errno));
    CHECK(dt >= 250, "ppoll-timeout-waited", "ppoll() returned after %llu ms, expected about 300", (unsigned long long)dt);
    CHECK(dt < 3000, "ppoll-timeout-not-overslept", "ppoll() returned after %llu ms, expected about 300", (unsigned long long)dt);

    close(fds[0]);
    close(fds[1]);
}


/*
 * Which of select() and pselect6() the libc reaches for is its own business, so each kernel
 * entry point is also called directly -- otherwise one of the two could stay a stub and every
 * test above would still pass.
 */
static void test_raw_entry_points(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "raw", "pipe() failed: %s", strerror(errno));
        return;
    }

    write(fds[1], "x", 1);


#if defined(SYS_select)
    {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fds[0], &rfds);

        struct timeval tv = {0, 0};

        long r = syscall(SYS_select, fds[0] + 1, &rfds, NULL, NULL, &tv);

        CHECK(r == 1, "raw-select", "SYS_select returned %ld (%s), expected 1", r, strerror(errno));
    }
#endif

#if defined(SYS_pselect6)
    {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fds[0], &rfds);

        struct timespec ts = {0, 0};

        /* pselect6() has seven arguments and a syscall carries six, so the mask and its size
           travel together behind one pointer. */
        struct {
            const sigset_t* ss;
            size_t ss_len;
        } sm = {NULL, 0};

        long r = syscall(SYS_pselect6, fds[0] + 1, &rfds, NULL, NULL, &ts, &sm);

        CHECK(r == 1, "raw-pselect6", "SYS_pselect6 returned %ld (%s), expected 1", r, strerror(errno));
    }
#endif

#if defined(SYS_ppoll)
    {
        struct pollfd pfd = {.fd = fds[0], .events = POLLIN, .revents = 0};
        struct timespec ts = {0, 0};

        long r = syscall(SYS_ppoll, &pfd, 1, &ts, NULL, _NSIG / 8);

        CHECK(r == 1, "raw-ppoll", "SYS_ppoll returned %ld (%s), expected 1", r, strerror(errno));
    }
#endif

    close(fds[0]);
    close(fds[1]);
}


static struct {

    const char* name;
    void (*fn)(void);

} cases[] = {
    {"readable", test_readable},
    {"writable", test_writable},
    {"zero-timeout", test_zero_timeout},
    {"timeout", test_timeout_expires},
    {"blocks", test_blocks_until_data},
    {"only-ready", test_reports_only_ready},
    {"count", test_counts_bits_not_fds},
    {"bad-fd", test_bad_fd},
    {"sleep", test_sleep},
    {"nanosleep", test_nanosleep},
    {"high-fd", test_high_fd_socket},
    {"fd-setsize", test_fd_setsize},
    {"pselect", test_pselect},
    {"pselect-blocks", test_pselect_blocks},
    {"pselect-sigmask", test_pselect_restores_sigmask},
    {"ppoll", test_ppoll},
    {"ppoll-timeout", test_ppoll_timeout},
    {"raw", test_raw_entry_points},
};


/*
 * With no argument every case runs. Naming one runs just that case, which is how these get
 * checked against an unfixed kernel: the sleeping cases hang instead of failing, and a hang in
 * one case would otherwise take the whole run down with it.
 */
int main(int argc, char** argv) {

    /* Unbuffered: a case that hangs would otherwise take its own output down with it. */
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("select-test: starting\n");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {

        if (argc > 1 && strcmp(argv[1], cases[i].name) != 0)
            continue;

        cases[i].fn();
    }

    printf("select-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
