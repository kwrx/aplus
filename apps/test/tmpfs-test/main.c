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
 * @brief Regression tests for tmpfs file sizes, file offsets and symbolic link resolution.
 *
 * Each case can be run on its own by name, since several of them used to take the kernel down.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#define SCRATCH "/tmp/tmpfs-test.d"

#define CHAIN_LENGTH 12


static int failures = 0;
static int total    = 0;
static int skipped  = 0;


#define CHECK(cond, name, fmt, ...)                                         \
    {                                                                       \
        total++;                                                            \
        if (cond) {                                                         \
            printf("tmpfs-test: PASS  %s\n", (name));                       \
        } else {                                                            \
            failures++;                                                     \
            printf("tmpfs-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                   \
    }


/**
 * @brief The byte a patterned file holds at a given offset.
 *
 * @param seed Distinguishes one file's pattern from another's.
 * @param offset The offset into the file.
 * @return The expected byte.
 */
static unsigned char pattern(unsigned seed, size_t offset) {
    return (unsigned char)((seed * 31U) + (offset * 131U) + (offset >> 8));
}


/**
 * @brief Creates or replaces a file holding a patterned run of bytes.
 *
 * @param path The file to write.
 * @param size How many bytes to write.
 * @param seed The pattern to write.
 * @return 0 on success, -1 on failure.
 */
static int write_pattern(const char* path, size_t size, unsigned seed) {

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd < 0)
        return -1;


    unsigned char chunk[1024];
    size_t done = 0;

    while (done < size) {

        size_t n = size - done < sizeof(chunk) ? size - done : sizeof(chunk);

        for (size_t i = 0; i < n; i++)
            chunk[i] = pattern(seed, done + i);

        if (write(fd, chunk, n) != (ssize_t)n)
            return close(fd), -1;

        done += n;
    }

    return close(fd);
}


/**
 * @brief Overwrites files of growing size through O_TRUNC and shrinks one with ftruncate(), checking what is left.
 */
static void test_truncate_shrink(void) {

    const char* path = SCRATCH "/truncate";

    int bad_rounds = 0;

    for (unsigned round = 0; round < 64; round++) {

        if (write_pattern(path, 16384 + (round * 1024), round) < 0) {
            bad_rounds++;
            continue;
        }

        int fd = open(path, O_WRONLY | O_TRUNC);

        if (fd < 0 || write(fd, "tiny", 4) != 4) {
            bad_rounds++;

            if (fd >= 0)
                close(fd);

            continue;
        }

        close(fd);


        struct stat st;
        char back[8] = {0};

        fd = open(path, O_RDONLY);

        if (fd < 0 || fstat(fd, &st) < 0 || st.st_size != 4 || read(fd, back, sizeof(back)) != 4 || memcmp(back, "tiny", 4) != 0)
            bad_rounds++;

        if (fd >= 0)
            close(fd);
    }

    CHECK(bad_rounds == 0, "otrunc-shrink", "%d of 64 rounds left the wrong size or content", bad_rounds);


    if (write_pattern(path, 20000, 7) < 0) {
        CHECK(0, "ftruncate-shrink", "could not write %s: %s", path, strerror(errno));
        return;
    }

    int fd = open(path, O_RDWR);

    if (fd < 0) {
        CHECK(0, "ftruncate-shrink", "open(%s): %s", path, strerror(errno));
        return;
    }

    int e = ftruncate(fd, 100);

    unsigned char back[128];
    ssize_t n = pread(fd, back, sizeof(back), 0);

    int same = (n == 100);

    for (ssize_t i = 0; same && i < n; i++)
        same = (back[i] == pattern(7, (size_t)i));

    close(fd);

    CHECK(e == 0 && same, "ftruncate-shrink", "ftruncate returned %d and a read back of %zd bytes %s", e, n, same ? "matched" : "did not match");
}


/**
 * @brief Reads at and past the end of a short file, which must report end of file rather than copy anything.
 */
static void test_read_past_eof(void) {

    const char* path = SCRATCH "/short";

    if (write_pattern(path, 10, 3) < 0) {
        CHECK(0, "read-past-eof", "could not write %s: %s", path, strerror(errno));
        return;
    }

    int fd = open(path, O_RDONLY);

    if (fd < 0) {
        CHECK(0, "read-past-eof", "open(%s): %s", path, strerror(errno));
        return;
    }


    char buf[64];

    ssize_t n = pread(fd, buf, sizeof(buf), 4);
    CHECK(n == 6, "pread-tail", "pread at 4 returned %zd, expected 6", n);

    n = pread(fd, buf, sizeof(buf), 10);
    CHECK(n == 0, "pread-at-eof", "pread at the end returned %zd, expected 0", n);

    n = pread(fd, buf, sizeof(buf), 1000);
    CHECK(n == 0, "pread-past-eof", "pread past the end returned %zd, expected 0", n);

    off_t o = lseek(fd, 500, SEEK_SET);
    n       = read(fd, buf, sizeof(buf));
    CHECK(o == 500 && n == 0, "read-past-eof", "lseek returned %ld and read returned %zd, expected 500 and 0", (long)o, n);

    close(fd);
}


/**
 * @brief Seeks to negative offsets and with an unsupported whence, all of which must fail and leave the offset alone.
 */
static void test_lseek_invalid(void) {

    const char* path = SCRATCH "/seek";

    if (write_pattern(path, 10, 5) < 0) {
        CHECK(0, "lseek-invalid", "could not write %s: %s", path, strerror(errno));
        return;
    }

    int fd = open(path, O_RDONLY);

    if (fd < 0) {
        CHECK(0, "lseek-invalid", "open(%s): %s", path, strerror(errno));
        return;
    }

    lseek(fd, 5, SEEK_SET);


    errno   = 0;
    off_t o = lseek(fd, -1, SEEK_SET);
    CHECK(o == -1 && errno == EINVAL, "lseek-set-negative", "returned %ld errno %d, expected -1 and EINVAL", (long)o, errno);

    errno = 0;
    o     = lseek(fd, -10, SEEK_CUR);
    CHECK(o == -1 && errno == EINVAL, "lseek-cur-negative", "returned %ld errno %d, expected -1 and EINVAL", (long)o, errno);

    errno = 0;
    o     = lseek(fd, -20, SEEK_END);
    CHECK(o == -1 && errno == EINVAL, "lseek-end-negative", "returned %ld errno %d, expected -1 and EINVAL", (long)o, errno);

    errno = 0;
    o     = lseek(fd, 0, 42);
    CHECK(o == -1 && errno == EINVAL, "lseek-bad-whence", "returned %ld errno %d, expected -1 and EINVAL", (long)o, errno);

    o = lseek(fd, 0, SEEK_CUR);
    CHECK(o == 5, "lseek-unchanged", "offset is %ld after the failed seeks, expected 5", (long)o);

    close(fd);
}


/**
 * @brief Opens a pair of links pointing at each other, which must fail with ELOOP instead of recursing forever.
 */
static void test_symlink_loop(void) {

    unlink(SCRATCH "/loop-a");
    unlink(SCRATCH "/loop-b");

    int e1 = symlink(SCRATCH "/loop-b", SCRATCH "/loop-a");
    int e2 = symlink(SCRATCH "/loop-a", SCRATCH "/loop-b");

    if (e1 < 0 || e2 < 0) {

        if (errno == ENOSYS) {
            printf("tmpfs-test: SKIP  symlink-loop: symlink() is not implemented\n");
            skipped++;
            return;
        }

        CHECK(0, "symlink-loop", "symlink(): %s", strerror(errno));
        return;
    }


    errno  = 0;
    int fd = open(SCRATCH "/loop-a", O_RDONLY);

    CHECK(fd < 0 && errno == ELOOP, "symlink-loop", "open returned %d errno %d, expected -1 and ELOOP", fd, errno);

    if (fd >= 0)
        close(fd);
}


/**
 * @brief Opens the far end of a chain of links, each pointing at the next, which is deep but finite.
 */
static void test_symlink_chain(void) {

    char link[64];
    char target[64];

    if (write_pattern(SCRATCH "/chain-0", 32, 9) < 0) {
        CHECK(0, "symlink-chain", "could not write the chain target: %s", strerror(errno));
        return;
    }

    for (int i = 1; i <= CHAIN_LENGTH; i++) {

        snprintf(link, sizeof(link), SCRATCH "/chain-%d", i);
        snprintf(target, sizeof(target), SCRATCH "/chain-%d", i - 1);

        unlink(link);

        if (symlink(target, link) < 0) {

            if (errno == ENOSYS) {
                printf("tmpfs-test: SKIP  symlink-chain: symlink() is not implemented\n");
                skipped++;
                return;
            }

            CHECK(0, "symlink-chain", "symlink(%s): %s", link, strerror(errno));
            return;
        }
    }


    snprintf(link, sizeof(link), SCRATCH "/chain-%d", CHAIN_LENGTH);

    int fd = open(link, O_RDONLY);

    unsigned char back[32];
    ssize_t n = fd >= 0 ? read(fd, back, sizeof(back)) : -1;

    int same = (n == 32);

    for (ssize_t i = 0; same && i < n; i++)
        same = (back[i] == pattern(9, (size_t)i));

    if (fd >= 0)
        close(fd);

    CHECK(same, "symlink-chain", "opening a chain of %d links gave fd %d and read %zd bytes", CHAIN_LENGTH, fd, n);
}


static struct {

    const char* name;
    void (*fn)(void);

} cases[] = {
    {"lseek-invalid", test_lseek_invalid},
    {"read-past-eof", test_read_past_eof},
    {"truncate-shrink", test_truncate_shrink},
    {"symlink-chain", test_symlink_chain},
    {"symlink-loop", test_symlink_loop},
};


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);

    if (mkdir(SCRATCH, 0755) < 0 && errno != EEXIST) {
        printf("tmpfs-test: mkdir(%s): %s\n", SCRATCH, strerror(errno));
        return 1;
    }

    printf("tmpfs-test: starting\n");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {

        if (argc > 1 && strcmp(argv[1], cases[i].name) != 0)
            continue;

        cases[i].fn();
    }

    printf("tmpfs-test: %d/%d passed, %d failed, %d skipped\n", total - failures, total, failures, skipped);

    return failures ? 1 : 0;
}
