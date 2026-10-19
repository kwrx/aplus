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

/**
 * @brief Tests for fcntl() record locks: F_GETLK, F_SETLK, F_SETLKW and the OFD variants, across processes.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


#define ON_TMPFS "/tmp/fcntl-lock-test.bin"
#define ON_EXT2  "/fcntl-lock-test.bin"


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                              \
    {                                                                            \
        total++;                                                                 \
        if (cond) {                                                              \
            printf("fcntl-lock-test: PASS  %s\n", (name));                       \
        } else {                                                                 \
            failures++;                                                          \
            printf("fcntl-lock-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                        \
    }


/**
 * @brief Issues a lock command on a byte range.
 *
 * @param fd The descriptor.
 * @param cmd F_SETLK, F_SETLKW, F_OFD_SETLK and so on.
 * @param type F_RDLCK, F_WRLCK or F_UNLCK.
 * @param start The first byte.
 * @param len The length, 0 for everything after @p start.
 * @return What fcntl() returned.
 */
static int lock(int fd, int cmd, short type, off_t start, off_t len) {

    struct flock fl = {
        .l_type   = type,
        .l_whence = SEEK_SET,
        .l_start  = start,
        .l_len    = len,
    };

    return fcntl(fd, cmd, &fl);
}


/**
 * @brief Asks which lock would block a request.
 *
 * @param fd The descriptor.
 * @param cmd F_GETLK or F_OFD_GETLK.
 * @param type F_RDLCK or F_WRLCK.
 * @param start The first byte.
 * @param len The length.
 * @return The answer; l_type is F_UNLCK when nothing blocks, -1 when fcntl() fails.
 */
static struct flock query(int fd, int cmd, short type, off_t start, off_t len) {

    struct flock fl = {
        .l_type   = type,
        .l_whence = SEEK_SET,
        .l_start  = start,
        .l_len    = len,
    };

    if (fcntl(fd, cmd, &fl) < 0) {
        fl.l_type = -1;
    }

    return fl;
}


/**
 * @brief Runs a function in a child process and adds its results to the totals.
 *
 * @param body The function, given the file the parent opened.
 * @param fd The descriptor.
 * @return The pid of the child, already reaped.
 */
static pid_t child(void (*body)(int), int fd) {

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(false, "pipe", "%s", strerror(errno));
        return -1;
    }

    const pid_t pid = fork();

    if (pid == 0) {

        close(fds[0]);

        total    = 0;
        failures = 0;

        body(fd);

        int counts[2] = {total, failures};

        if (write(fds[1], counts, sizeof(counts)) != sizeof(counts)) {
            _exit(1);
        }

        _exit(0);
    }

    close(fds[1]);

    int counts[2] = {0, 0};

    if (read(fds[0], counts, sizeof(counts)) != sizeof(counts)) {
        CHECK(false, "child reports", "%s", "no report");
    }

    close(fds[0]);

    total += counts[0];
    failures += counts[1];

    int status;
    waitpid(pid, &status, 0);

    return pid;
}


/**
 * @brief Elapsed milliseconds since a moment.
 *
 * @param since The moment.
 * @return The milliseconds.
 */
static long elapsed(const struct timespec* since) {

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    return (now.tv_sec - since->tv_sec) * 1000 + (now.tv_nsec - since->tv_nsec) / 1000000;
}


static pid_t parent_pid;


/**
 * @brief In a child: the parent's write lock on [0,99] blocks writes inside it, F_GETLK reports it, and bytes past it are free.
 *
 * @param fd The file.
 */
static void conflicts(int fd) {

    errno = 0;
    int r = lock(fd, F_SETLK, F_WRLCK, 50, 10);
    CHECK(r < 0 && (errno == EAGAIN || errno == EACCES), "write lock inside another process's lock", "%d %s", r, strerror(errno));

    errno = 0;
    r     = lock(fd, F_SETLK, F_RDLCK, 0, 1);
    CHECK(r < 0 && (errno == EAGAIN || errno == EACCES), "read lock inside another process's write lock", "%d %s", r, strerror(errno));

    struct flock fl = query(fd, F_GETLK, F_WRLCK, 50, 10);
    CHECK(fl.l_type == F_WRLCK && fl.l_start == 0 && fl.l_len == 100 && fl.l_pid == parent_pid, "F_GETLK reports the holder", "type %d start %ld len %ld pid %d", fl.l_type, (long)fl.l_start, (long)fl.l_len, fl.l_pid);

    r = lock(fd, F_SETLK, F_RDLCK, 100, 100);
    CHECK(r == 0, "lock past the other process's range", "%s", strerror(errno));
}


/**
 * @brief In a child: read locks share, a write lock does not.
 *
 * @param fd The file.
 */
static void shared_reads(int fd) {

    int r = lock(fd, F_SETLK, F_RDLCK, 0, 10);
    CHECK(r == 0, "read locks share", "%s", strerror(errno));

    errno = 0;
    r     = lock(fd, F_SETLK, F_WRLCK, 0, 10);
    CHECK(r < 0 && errno == EAGAIN, "write lock over another process's read lock", "%d %s", r, strerror(errno));
}


/**
 * @brief In a child: the parent unlocked [40,59] in the middle of its write lock on [0,99].
 *
 * @param fd The file.
 */
static void split(int fd) {

    int r = lock(fd, F_SETLK, F_WRLCK, 40, 20);
    CHECK(r == 0, "the unlocked middle is free", "%s", strerror(errno));

    errno = 0;
    r     = lock(fd, F_SETLK, F_WRLCK, 39, 1);
    CHECK(r < 0 && errno == EAGAIN, "the left part stays locked", "%d %s", r, strerror(errno));

    errno = 0;
    r     = lock(fd, F_SETLK, F_WRLCK, 60, 1);
    CHECK(r < 0 && errno == EAGAIN, "the right part stays locked", "%d %s", r, strerror(errno));
}


/**
 * @brief In a child: nothing is locked any more.
 *
 * @param fd The file.
 */
static void unlocked(int fd) {

    int r = lock(fd, F_SETLK, F_WRLCK, 0, 0);
    CHECK(r == 0, "the whole file is free", "%s", strerror(errno));
}


/**
 * @brief In a child: F_SETLKW waits until the parent drops its lock, about 300 ms later.
 *
 * @param fd The file.
 */
static void waits(int fd) {

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int r         = lock(fd, F_SETLKW, F_WRLCK, 0, 1);
    const long ms = elapsed(&start);

    CHECK(r == 0, "F_SETLKW gets the lock", "%s", strerror(errno));
    CHECK(ms >= 200, "F_SETLKW waited for the holder", "%ld ms", ms);
}


/**
 * @brief Runs the cases on a file.
 *
 * @param path The file.
 * @param fs The filesystem, for the messages.
 */
static void test_file(const char* path, const char* fs) {

    char name[96];

    unlink(path);

    const int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);

    snprintf(name, sizeof(name), "open on %s", fs);
    CHECK(fd >= 0, name, "%s", strerror(errno));

    if (fd < 0) {
        return;
    }


    int r = lock(fd, F_SETLK, F_WRLCK, 0, 100);
    snprintf(name, sizeof(name), "write lock on %s", fs);
    CHECK(r == 0, name, "%s", strerror(errno));

    struct flock fl = query(fd, F_GETLK, F_WRLCK, 0, 100);
    CHECK(fl.l_type == F_UNLCK, "own locks never conflict", "type %d", fl.l_type);

    const int again = open(path, O_RDWR);
    r               = lock(again, F_SETLK, F_WRLCK, 0, 100);
    CHECK(r == 0, "same process, another descriptor", "%s", strerror(errno));

    child(conflicts, fd);

    fl = query(fd, F_GETLK, F_WRLCK, 100, 100);
    CHECK(fl.l_type == F_UNLCK, "a child's locks go when it exits", "type %d pid %d", fl.l_type, fl.l_pid);


    r = lock(fd, F_SETLK, F_UNLCK, 0, 0);
    CHECK(r == 0, "unlock everything", "%s", strerror(errno));

    r = lock(fd, F_SETLK, F_RDLCK, 0, 10);
    CHECK(r == 0, "read lock", "%s", strerror(errno));

    child(shared_reads, fd);


    CHECK(lock(fd, F_SETLK, F_WRLCK, 0, 100) == 0, "upgrade to a write lock", "%s", strerror(errno));
    CHECK(lock(fd, F_SETLK, F_UNLCK, 40, 20) == 0, "unlock the middle", "%s", strerror(errno));

    child(split, fd);


    close(again);

    child(unlocked, fd);


    CHECK(lock(fd, F_SETLK, F_WRLCK, 0, 1) == 0, "lock for the wait", "%s", strerror(errno));

    const pid_t pid = fork();

    if (pid == 0) {
        total    = 0;
        failures = 0;
        waits(fd);
        _exit(failures ? 1 : 0);
    }

    struct timespec pause = {0, 300 * 1000000L};
    nanosleep(&pause, NULL);

    CHECK(lock(fd, F_SETLK, F_UNLCK, 0, 1) == 0, "unlock for the waiter", "%s", strerror(errno));

    int status = 0;
    waitpid(pid, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "the waiter got the lock", "status %x", status);

    close(fd);
    unlink(path);
}


/**
 * @brief Lock types the descriptor's access mode forbids, and malformed requests.
 */
static void test_errors(void) {

    const int ro = open(ON_TMPFS, O_RDONLY | O_CREAT, 0644);
    const int wo = open(ON_TMPFS, O_WRONLY);

    errno = 0;
    int r = lock(ro, F_SETLK, F_WRLCK, 0, 1);
    CHECK(r < 0 && errno == EBADF, "write lock on a read-only descriptor", "%d %s", r, strerror(errno));

    errno = 0;
    r     = lock(wo, F_SETLK, F_RDLCK, 0, 1);
    CHECK(r < 0 && errno == EBADF, "read lock on a write-only descriptor", "%d %s", r, strerror(errno));

    CHECK(lock(ro, F_SETLK, F_RDLCK, 0, 1) == 0, "read lock on a read-only descriptor", "%s", strerror(errno));

    errno = 0;
    r     = lock(ro, F_SETLK, 99, 0, 1);
    CHECK(r < 0 && errno == EINVAL, "unknown lock type", "%d %s", r, strerror(errno));

    errno = 0;
    r     = lock(ro, F_SETLK, F_RDLCK, -5, 1);
    CHECK(r < 0 && errno == EINVAL, "negative start", "%d %s", r, strerror(errno));

    struct flock fl = {.l_type = F_RDLCK, .l_whence = 7};
    errno           = 0;
    r               = fcntl(ro, F_SETLK, &fl);
    CHECK(r < 0 && errno == EINVAL, "unknown whence", "%d %s", r, strerror(errno));

    errno = 0;
    r     = lock(-1, F_SETLK, F_RDLCK, 0, 1);
    CHECK(r < 0 && errno == EBADF, "bad descriptor", "%d %s", r, strerror(errno));

    close(ro);
    close(wo);
    unlink(ON_TMPFS);
}


/**
 * @brief Open file description locks belong to the open file, not to the process.
 */
static void test_ofd(void) {

    const int a = open(ON_TMPFS, O_RDWR | O_CREAT | O_TRUNC, 0644);
    const int b = open(ON_TMPFS, O_RDWR);

    CHECK(lock(a, F_OFD_SETLK, F_WRLCK, 0, 10) == 0, "OFD write lock", "%s", strerror(errno));

    errno = 0;
    int r = lock(b, F_OFD_SETLK, F_WRLCK, 0, 10);
    CHECK(r < 0 && errno == EAGAIN, "another open file of the same process conflicts", "%d %s", r, strerror(errno));

    struct flock fl = query(b, F_OFD_GETLK, F_WRLCK, 0, 10);
    CHECK(fl.l_type == F_WRLCK && fl.l_pid == -1, "F_OFD_GETLK reports pid -1", "type %d pid %d", fl.l_type, fl.l_pid);

    struct flock bad = {.l_type = F_WRLCK, .l_whence = SEEK_SET, .l_pid = 1};
    errno            = 0;
    r                = fcntl(b, F_OFD_SETLK, &bad);
    CHECK(r < 0 && errno == EINVAL, "OFD request with a pid", "%d %s", r, strerror(errno));

    close(a);

    CHECK(lock(b, F_OFD_SETLK, F_WRLCK, 0, 10) == 0, "closing the open file drops its OFD lock", "%s", strerror(errno));

    close(b);
    unlink(ON_TMPFS);
}


/**
 * @brief Runs every test.
 *
 * @return 0 when they all pass, 1 otherwise.
 */
int main(void) {

    setvbuf(stdout, NULL, _IONBF, 0);

    parent_pid = getpid();

    printf("fcntl-lock-test: starting\n");

    test_file(ON_TMPFS, "tmpfs");
    test_file(ON_EXT2, "ext2");
    test_errors();
    test_ofd();

    printf("fcntl-lock-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
