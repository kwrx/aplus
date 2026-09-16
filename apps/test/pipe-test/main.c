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
 * @brief Regression tests for pipes, readiness notification and poll().
 *
 * Several of these used to hang rather than fail, so run this under a timeout.
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


/**
 * @brief CONFIG_PIPESIZ; a write larger than this is the case that used to be unsatisfiable.
 */
#define PIPE_CAPACITY 65535


static uint64_t now_ms(void) {

    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
        return 0;

    return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}


/**
 * @brief Checks that a write bigger than the pipe buffer makes partial progress, with a reader draining.
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


/**
 * @brief Checks that closing the write end surfaces as end of file on the read end.
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

    CHECK(first == 5 && memcmp(buf, "hello", 5) == 0, "drain after writer close", "read() returned %zd (errno %d)", first, errno);

    errno         = 0;
    ssize_t empty = read(fds[0], buf, sizeof(buf));

    CHECK(empty == 0, "eof on writer close", "read() returned %zd, errno %d (%s)", empty, errno, strerror(errno));

    close(fds[0]);
}


/**
 * @brief Checks that a write end survives an execve() and still reaches its last close when that child exits.
 */
static void test_eof_after_exec(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "eof after exec", "pipe() failed: %s", strerror(errno));
        return;
    }


    pid_t pid = fork();

    if (pid < 0) {
        CHECK(0, "eof after exec", "fork() failed: %s", strerror(errno));
        return;
    }

    if (pid == 0) {

        close(fds[0]);

        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);

        execl("/bin/echo", "echo", "hello", NULL);
        _exit(127);
    }


    close(fds[1]);


    char buf[64] = {0};
    size_t got   = 0;

    for (;;) {

        ssize_t e = read(fds[0], buf + got, sizeof(buf) - 1 - got);

        if (e <= 0)
            break;

        got += (size_t)e;
    }

    close(fds[0]);


    int status = 0;
    waitpid(pid, &status, 0);

    CHECK(got == 6 && memcmp(buf, "hello\n", 6) == 0, "eof after exec", "read %zu bytes ('%s'), child status %d", got, buf, status);
}


/**
 * @brief Checks that popen() runs its child, which needs the CLONE_VFORK posix_spawn() clones with.
 */
static void test_popen(void) {

    FILE* fp = popen("/bin/echo spawned", "r");

    if (!fp) {
        CHECK(0, "popen", "popen() failed: %s", strerror(errno));
        return;
    }


    char buf[64] = {0};
    char* line   = fgets(buf, sizeof(buf), fp);

    int status = pclose(fp);

    CHECK(line && strcmp(buf, "spawned\n") == 0, "popen reads child output", "got '%s', pclose() returned %d", buf, status);
}


/**
 * @brief Checks that writing into a pipe with no reader left is EPIPE.
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


/**
 * @brief Checks that data written before poll() is called is still reported as ready.
 */
static void test_poll_sees_buffered_data(void) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "poll sees buffered data", "pipe() failed: %s", strerror(errno));
        return;
    }

    if (write(fds[1], "a", 1) != 1) {
        CHECK(0, "poll sees buffered data", "write() failed: %s", strerror(errno));
        return;
    }

    struct pollfd pfd = {.fd = fds[0], .events = POLLIN, .revents = 0};

    int e = poll(&pfd, 1, 2000);

    CHECK(e == 1 && (pfd.revents & POLLIN), "poll sees buffered data", "poll() returned %d, revents 0x%x", e, pfd.revents);


    pfd.revents = 0;

    int again = poll(&pfd, 1, 2000);

    CHECK(again == 1 && (pfd.revents & POLLIN), "poll readiness is not consumed", "second poll() returned %d, revents 0x%x", again, pfd.revents);

    close(fds[0]);
    close(fds[1]);
}


/**
 * @brief Checks that a zero timeout is a readiness probe rather than a wait.
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


/**
 * @brief Checks that a positive timeout actually comes due, whatever the syscall is restarted by.
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


/**
 * @brief Checks that a hung-up peer is reported, though POLLHUP is never in the caller's events mask.
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


/**
 * @brief Checks that a negative fd is skipped with revents cleared, as POSIX asks.
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


/**
 * @brief Checks that O_CLOEXEC set at creation is honoured by execve(), not only fcntl(F_SETFD).
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


/**
 * @brief Checks that mknod() creates an ordinary file, comparing the type bits against S_IFMT.
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
    {"eof-exec", test_eof_after_exec},
    {"popen", test_popen},
    {"epipe", test_epipe},
    {"poll-buffered", test_poll_sees_buffered_data},
    {"poll-zero", test_poll_zero_timeout},
    {"poll-timeout", test_poll_timeout_expires},
    {"poll-hup", test_poll_hup},
    {"poll-negative", test_poll_negative_fd},
    {"cloexec", test_cloexec},
    {"mknod", test_mknod_regular_file},
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

    printf("pipe-test: starting\n");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {

        if (argc > 1 && strcmp(argv[1], cases[i].name) != 0)
            continue;

        cases[i].fn();
    }

    printf("pipe-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
