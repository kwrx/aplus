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

/**
 * @brief Regression tests for select(), pselect6() and ppoll().
 *
 * Most of these are about select()'s three in-out sets, which a restarted syscall re-reads; run under a timeout.
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


/**
 * @brief How much of a sigset_t either side actually means, so that comparisons do not read stack litter.
 */
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


/**
 * @brief Checks that data already in the pipe is reported without sleeping.
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


/**
 * @brief Checks that an empty pipe has a writable write end and an unreadable read end.
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


/**
 * @brief Checks that a zero timeout is a probe, and that all three sets come back empty.
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


/**
 * @brief Checks that a timeout actually comes due, from a deadline stamped once and counted down across restarts.
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


/**
 * @brief Checks that select() sleeps until a writer arrives, with the sets it is restarted on still intact.
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


/**
 * @brief Checks that only the descriptors that are actually ready come back set.
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


/**
 * @brief Checks that the return value counts ready bits rather than descriptors.
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


/**
 * @brief Checks that a closed descriptor fails the whole call, which select() has no per-entry way to report.
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


/**
 * @brief Checks that select() with no descriptors is a sleep that comes back on its own.
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


/**
 * @brief Checks nanosleep(), which goes through the scheduler's own deadline rather than the poll family's.
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


/**
 * @brief Checks that select() and poll() agree about a socket, whose readiness the network stack answers.
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


/**
 * @brief Checks that select(FD_SETSIZE, ...) is bounded rather than refused.
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


/**
 * @brief Checks pselect(), which is select() with a timespec and a mask it blocks for the duration.
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


/**
 * @brief Checks that pselect() sleeps and wakes like select() does, restart hazard and all.
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


/**
 * @brief Checks that the mask pselect() installs is swapped back on the way out, including out of a restart.
 */
static void test_pselect_restores_sigmask(void) {

    sigset_t before, during, after;

    sigemptyset(&during);
    sigaddset(&during, SIGUSR1);

    if (sigprocmask(SIG_SETMASK, NULL, &before) < 0) {
        CHECK(0, "pselect-sigmask", "sigprocmask() failed: %s", strerror(errno));
        return;
    }


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


/**
 * @brief Checks ppoll(), which is poll() with a timespec and a mask.
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


/**
 * @brief Checks that ppoll()'s timeout comes due.
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


/**
 * @brief Calls each kernel entry point directly, since which one the libc reaches for is its own business.
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


/**
 * @brief Runs every case, or the one named on the command line.
 *
 * @param argc The argument count.
 * @param argv The arguments; an optional case name.
 * @return 0 when every case that ran passed.
 */
int main(int argc, char** argv) {

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
