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
 * @brief Tests for fsync() and fdatasync() on ext2, tmpfs, pipes and bad descriptors.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>


#define ON_EXT2  "/fsync-test.bin"
#define ON_TMPFS "/tmp/fsync-test.bin"
#define ROUNDS   200


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                         \
    {                                                                       \
        total++;                                                            \
        if (cond) {                                                         \
            printf("fsync-test: PASS  %s\n", (name));                       \
        } else {                                                            \
            failures++;                                                     \
            printf("fsync-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                   \
    }


/**
 * @brief fsync() and fdatasync() of a file written on a filesystem, and of its directory.
 *
 * @param path The file.
 * @param dir The directory it is in.
 * @param fs The filesystem, for the messages.
 */
static void test_file(const char* path, const char* dir, const char* fs) {

    char name[64];

    const int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);

    snprintf(name, sizeof(name), "open on %s", fs);
    CHECK(fd >= 0, name, "%s", strerror(errno));

    if (fd < 0) {
        return;
    }

    snprintf(name, sizeof(name), "write on %s", fs);
    CHECK(write(fd, "fsync-test\n", 11) == 11, name, "%s", strerror(errno));

    errno = 0;
    int r = fsync(fd);
    snprintf(name, sizeof(name), "fsync of a file on %s", fs);
    CHECK(r == 0, name, "%d %s", r, strerror(errno));

    errno = 0;
    r     = fdatasync(fd);
    snprintf(name, sizeof(name), "fdatasync of a file on %s", fs);
    CHECK(r == 0, name, "%d %s", r, strerror(errno));

    close(fd);

    struct stat st = {0};
    stat(path, &st);
    snprintf(name, sizeof(name), "size survives on %s", fs);
    CHECK(st.st_size == 11, name, "%ld", (long)st.st_size);

    const int dfd = open(dir, O_RDONLY | O_DIRECTORY);

    errno = 0;
    r     = dfd >= 0 ? fsync(dfd) : -1;
    snprintf(name, sizeof(name), "fsync of a directory on %s", fs);
    CHECK(r == 0, name, "%d %s", r, strerror(errno));

    if (dfd >= 0) {
        close(dfd);
    }

    unlink(path);
}


/**
 * @brief Descriptors that cannot be flushed: a pipe, a closed descriptor and an O_PATH one.
 */
static void test_errors(void) {

    int fds[2] = {-1, -1};

    CHECK(pipe(fds) == 0, "pipe", "%s", strerror(errno));

    errno = 0;
    int r = fsync(fds[1]);
    CHECK(r < 0 && errno == EINVAL, "fsync of a pipe", "%d %s", r, strerror(errno));

    errno = 0;
    r     = fdatasync(fds[0]);
    CHECK(r < 0 && errno == EINVAL, "fdatasync of a pipe", "%d %s", r, strerror(errno));

    close(fds[0]);
    close(fds[1]);

    errno = 0;
    r     = fsync(fds[1]);
    CHECK(r < 0 && errno == EBADF, "fsync of a closed descriptor", "%d %s", r, strerror(errno));

    errno = 0;
    r     = fdatasync(-1);
    CHECK(r < 0 && errno == EBADF, "fdatasync of -1", "%d %s", r, strerror(errno));

    const int path = open("/tmp", O_PATH);

    errno = 0;
    r     = path >= 0 ? fsync(path) : -2;
    CHECK(r < 0 && errno == EBADF, "fsync of an O_PATH descriptor", "%d %s", r, strerror(errno));

    if (path >= 0) {
        close(path);
    }
}


/**
 * @brief Repeated fsync() of an ext2 file stays cheap, so it does not rewrite every cached inode.
 */
static void test_cost(void) {

    const int fd = open(ON_EXT2, O_RDWR | O_CREAT | O_TRUNC, 0644);

    if (fd < 0) {
        CHECK(false, "open for the cost check", "%s", strerror(errno));
        return;
    }

    struct timespec start;
    struct timespec end;

    int ok = 0;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < ROUNDS; i++) {

        if (write(fd, "x", 1) == 1 && fsync(fd) == 0) {
            ok++;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    close(fd);
    unlink(ON_EXT2);

    const double us = ((end.tv_sec - start.tv_sec) * 1e6 + (end.tv_nsec - start.tv_nsec) / 1e3) / ROUNDS;

    printf("fsync-test: INFO  write+fsync on ext2 takes %.1f us\n", us);

    CHECK(ok == ROUNDS, "repeated write+fsync", "%d of %d", ok, ROUNDS);
    CHECK(us < 20000.0, "fsync costs less than 20 ms", "%.1f us", us);
}


/**
 * @brief Runs every test.
 *
 * @return 0 when they all pass, 1 otherwise.
 */
int main(void) {

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("fsync-test: starting\n");

    test_file(ON_EXT2, "/", "ext2");
    test_file(ON_TMPFS, "/tmp", "tmpfs");
    test_errors();
    test_cost();

    printf("fsync-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
