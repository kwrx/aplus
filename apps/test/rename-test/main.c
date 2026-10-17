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
 * @brief Tests for rename(), renameat() and renameat2() on tmpfs, and for mv on top of them.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>


#ifndef RENAME_NOREPLACE
    #define RENAME_NOREPLACE (1 << 0)
#endif

#ifndef RENAME_WHITEOUT
    #define RENAME_WHITEOUT (1 << 2)
#endif


#define SCRATCH "/tmp/rename-test.d"
#define ON_EXT2 "/rename-test.ext2"
#define MOVED   "/rename-test.mv"


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                          \
    {                                                                        \
        total++;                                                             \
        if (cond) {                                                          \
            printf("rename-test: PASS  %s\n", (name));                       \
        } else {                                                             \
            failures++;                                                      \
            printf("rename-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                    \
    }


/**
 * @brief Writes a file, replacing whatever it held.
 *
 * @param path The file.
 * @param text What it should hold.
 * @return 0 on success, or -1.
 */
static int put(const char* path, const char* text) {

    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd < 0) {
        return -1;
    }

    const ssize_t n = write(fd, text, strlen(text));

    close(fd);

    return n == (ssize_t)strlen(text) ? 0 : -1;
}


/**
 * @brief Reports whether a file holds exactly some text.
 *
 * @param path The file.
 * @param text The text expected.
 * @return true when it does.
 */
static bool holds(const char* path, const char* text) {

    char buffer[64] = {0};

    const int fd = open(path, O_RDONLY);

    if (fd < 0) {
        return false;
    }

    const ssize_t n = read(fd, buffer, sizeof(buffer) - 1);

    close(fd);

    return n >= 0 && strcmp(buffer, text) == 0;
}


/**
 * @brief Reports whether a path names nothing.
 *
 * @param path The path.
 * @return true when stat() fails with ENOENT.
 */
static bool gone(const char* path) {

    struct stat st;

    return stat(path, &st) < 0 && errno == ENOENT;
}


/**
 * @brief Renames with renameat2(), which the C library does not wrap.
 *
 * @param from The old path.
 * @param to The new path.
 * @param flags RENAME_* flags.
 * @return 0 on success, or -1 with errno set.
 */
static int rename2(const char* from, const char* to, unsigned int flags) {

    return (int)syscall(SYS_renameat2, AT_FDCWD, from, AT_FDCWD, to, flags);
}


/**
 * @brief Renames files within one directory, back and forth, over another file and onto themselves.
 */
static void test_files(void) {

    put(SCRATCH "/a", "alpha");

    int r = rename(SCRATCH "/a", SCRATCH "/b");
    CHECK(r == 0 && gone(SCRATCH "/a") && holds(SCRATCH "/b", "alpha"), "rename-file", "r=%d errno=%s", r, strerror(errno));

    bool coherent = true;

    for (int i = 0; i < 8 && coherent; i++) {

        const char* from = (i & 1) ? SCRATCH "/a" : SCRATCH "/b";
        const char* to   = (i & 1) ? SCRATCH "/b" : SCRATCH "/a";

        coherent = rename(from, to) == 0 && gone(from) && holds(to, "alpha");
    }

    CHECK(coherent, "rename-back-and-forth", "errno=%s", strerror(errno));

    put(SCRATCH "/c", "gamma");

    r = rename(SCRATCH "/b", SCRATCH "/c");
    CHECK(r == 0 && gone(SCRATCH "/b") && holds(SCRATCH "/c", "alpha"), "rename-replaces-file", "r=%d errno=%s", r, strerror(errno));

    r = rename(SCRATCH "/c", SCRATCH "/c");
    CHECK(r == 0 && holds(SCRATCH "/c", "alpha"), "rename-onto-itself", "r=%d errno=%s", r, strerror(errno));

    r = rename(SCRATCH "/missing", SCRATCH "/x");
    CHECK(r < 0 && errno == ENOENT, "rename-missing-source", "r=%d errno=%s", r, strerror(errno));
}


/**
 * @brief Checks RENAME_NOREPLACE, and that renameat2() refuses flags it does not know.
 */
static void test_flags(void) {

    put(SCRATCH "/d", "delta");

    int r = rename2(SCRATCH "/c", SCRATCH "/d", RENAME_NOREPLACE);
    CHECK(r < 0 && errno == EEXIST && holds(SCRATCH "/c", "alpha") && holds(SCRATCH "/d", "delta"), "renameat2-noreplace-existing", "r=%d errno=%s", r, strerror(errno));

    r = rename2(SCRATCH "/c", SCRATCH "/e", RENAME_NOREPLACE);
    CHECK(r == 0 && gone(SCRATCH "/c") && holds(SCRATCH "/e", "alpha"), "renameat2-noreplace-new", "r=%d errno=%s", r, strerror(errno));

    r = rename2(SCRATCH "/e", SCRATCH "/x", RENAME_WHITEOUT);
    CHECK(r < 0 && errno == EINVAL && holds(SCRATCH "/e", "alpha"), "renameat2-unknown-flag", "r=%d errno=%s", r, strerror(errno));
}


/**
 * @brief Checks that a descriptor open on a file still reads it after the file is renamed.
 */
static void test_open_file(void) {

    char buffer[16] = {0};

    const int fd = open(SCRATCH "/e", O_RDONLY);

    const int r = rename(SCRATCH "/e", SCRATCH "/f");

    const ssize_t n = fd < 0 ? -1 : read(fd, buffer, sizeof(buffer) - 1);

    if (fd >= 0) {
        close(fd);
    }

    CHECK(r == 0 && n == 5 && strcmp(buffer, "alpha") == 0, "rename-keeps-open-file", "r=%d n=%zd errno=%s", r, n, strerror(errno));
}


/**
 * @brief Renames directories, and checks the errors for mixing files and directories and for leaving the directory.
 */
static void test_directories(void) {

    mkdir(SCRATCH "/d1", 0755);
    put(SCRATCH "/d1/inner", "inner");

    int r = rename(SCRATCH "/d1", SCRATCH "/d2");
    CHECK(r == 0 && gone(SCRATCH "/d1") && holds(SCRATCH "/d2/inner", "inner"), "rename-directory", "r=%d errno=%s", r, strerror(errno));

    r = rename(SCRATCH "/f", SCRATCH "/d2");
    CHECK(r < 0 && errno == EISDIR, "rename-file-over-directory", "r=%d errno=%s", r, strerror(errno));

    r = rename(SCRATCH "/d2", SCRATCH "/f");
    CHECK(r < 0 && errno == ENOTDIR, "rename-directory-over-file", "r=%d errno=%s", r, strerror(errno));

    mkdir(SCRATCH "/d3", 0755);
    put(SCRATCH "/d3/x", "x");

    r = rename(SCRATCH "/d2", SCRATCH "/d3");
    CHECK(r < 0 && errno == ENOTEMPTY && holds(SCRATCH "/d2/inner", "inner"), "rename-over-full-directory", "r=%d errno=%s", r, strerror(errno));

    mkdir(SCRATCH "/d4", 0755);

    r = rename(SCRATCH "/d2/", SCRATCH "/d4/");
    CHECK(r == 0 && gone(SCRATCH "/d2") && holds(SCRATCH "/d4/inner", "inner"), "rename-over-empty-directory", "r=%d errno=%s", r, strerror(errno));

    r = rename(SCRATCH "/f", SCRATCH "/d4/f");
    CHECK(r < 0 && errno == EXDEV && holds(SCRATCH "/f", "alpha"), "rename-across-directories", "r=%d errno=%s", r, strerror(errno));
}


/**
 * @brief Renames relative to a directory descriptor.
 */
static void test_renameat(void) {

    const int dfd = open(SCRATCH, O_RDONLY | O_DIRECTORY);

    const int r = dfd < 0 ? -1 : renameat(dfd, "f", dfd, "g");

    CHECK(r == 0 && gone(SCRATCH "/f") && holds(SCRATCH "/g", "alpha"), "renameat-dirfd", "dfd=%d r=%d errno=%s", dfd, r, strerror(errno));

    if (dfd >= 0) {
        close(dfd);
    }
}


/**
 * @brief Checks that ext2, which cannot rename, answers EXDEV so callers fall back to copying.
 */
static void test_ext2(void) {

    put(ON_EXT2, "ext2");

    const int r = rename(ON_EXT2, ON_EXT2 ".new");

    CHECK(r < 0 && errno == EXDEV && holds(ON_EXT2, "ext2"), "rename-on-ext2-is-exdev", "r=%d errno=%s", r, strerror(errno));

    unlink(ON_EXT2);
    unlink(ON_EXT2 ".new");
}


/**
 * @brief Runs mv within tmpfs, and onto ext2 where it has to copy.
 */
static void test_mv(void) {

    int r = system("mv " SCRATCH "/g " SCRATCH "/h");
    CHECK(r == 0 && gone(SCRATCH "/g") && holds(SCRATCH "/h", "alpha"), "mv-within-directory", "status=%d", r);

    r = system("mv " SCRATCH "/h " MOVED);
    CHECK(r == 0 && gone(SCRATCH "/h") && holds(MOVED, "alpha"), "mv-to-ext2-copies", "status=%d", r);

    unlink(MOVED);
}


/**
 * @brief Runs every case in a scratch directory on the tmpfs.
 *
 * @return 0 when every case passed, 1 otherwise.
 */
int main(void) {

    setvbuf(stdout, NULL, _IONBF, 0);

    system("rm -rf " SCRATCH);

    if (mkdir(SCRATCH, 0755) < 0) {
        printf("rename-test: mkdir(%s): %s\n", SCRATCH, strerror(errno));
        return 1;
    }

    printf("rename-test: starting\n");

    test_files();
    test_flags();
    test_open_file();
    test_directories();
    test_renameat();
    test_ext2();
    test_mv();

    system("rm -rf " SCRATCH);

    printf("rename-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
