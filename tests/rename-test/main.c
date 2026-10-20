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
 * @brief Tests for rename(), renameat() and renameat2() on tmpfs and ext2, and for mv on top of them.
 */

#include <dirent.h>
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


#define MOVED     "/rename-test.mv"
#define LONG_NAME "a-name-long-enough-that-its-directory-entry-cannot-reuse-the-slot-left-by-a-short-one-0123456789abcdef"


static int failures      = 0;
static int total         = 0;
static const char* label = "";


#define CHECK(cond, name, fmt, ...)                                                      \
    {                                                                                    \
        total++;                                                                         \
        if (cond) {                                                                      \
            printf("rename-test: PASS  [%s] %s\n", label, (name));                       \
        } else {                                                                         \
            failures++;                                                                  \
            printf("rename-test: FAIL  [%s] %s: " fmt "\n", label, (name), __VA_ARGS__); \
        }                                                                                \
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
 * @brief Reports whether the current directory lists a name.
 * @param name The name.
 * @return true when readdir() returns it.
 */
static bool listed(const char* name) {

    DIR* dir = opendir(".");

    if (!dir) {
        return false;
    }

    bool found = false;

    for (struct dirent* ent; !found && (ent = readdir(dir)) != NULL;) {
        found = strcmp(ent->d_name, name) == 0;
    }

    closedir(dir);

    return found;
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

    put("a", "alpha");

    int r = rename("a", "b");
    CHECK(r == 0 && gone("a") && holds("b", "alpha"), "rename-file", "r=%d errno=%s", r, strerror(errno));

    bool coherent = true;

    for (int i = 0; i < 8 && coherent; i++) {

        const char* from = (i & 1) ? "a" : "b";
        const char* to   = (i & 1) ? "b" : "a";

        coherent = rename(from, to) == 0 && gone(from) && holds(to, "alpha");
    }

    CHECK(coherent, "rename-back-and-forth", "errno=%s", strerror(errno));

    CHECK(listed("b") && !listed("a"), "rename-updates-directory", "b=%d a=%d", listed("b"), listed("a"));

    r = rename("b", LONG_NAME);
    CHECK(r == 0 && gone("b") && listed(LONG_NAME) && !listed("b") && holds(LONG_NAME, "alpha"), "rename-to-long-name", "r=%d errno=%s", r, strerror(errno));

    r = rename(LONG_NAME, "b");
    CHECK(r == 0 && gone(LONG_NAME) && !listed(LONG_NAME) && holds("b", "alpha"), "rename-from-long-name", "r=%d errno=%s", r, strerror(errno));

    put("c", "gamma");

    r = rename("b", "c");
    CHECK(r == 0 && gone("b") && holds("c", "alpha"), "rename-replaces-file", "r=%d errno=%s", r, strerror(errno));

    r = rename("c", "c");
    CHECK(r == 0 && holds("c", "alpha"), "rename-onto-itself", "r=%d errno=%s", r, strerror(errno));

    r = rename("missing", "x");
    CHECK(r < 0 && errno == ENOENT, "rename-missing-source", "r=%d errno=%s", r, strerror(errno));
}


/**
 * @brief Checks RENAME_NOREPLACE, and that renameat2() refuses flags it does not know.
 */
static void test_flags(void) {

    put("d", "delta");

    int r = rename2("c", "d", RENAME_NOREPLACE);
    CHECK(r < 0 && errno == EEXIST && holds("c", "alpha") && holds("d", "delta"), "renameat2-noreplace-existing", "r=%d errno=%s", r, strerror(errno));

    r = rename2("c", "e", RENAME_NOREPLACE);
    CHECK(r == 0 && gone("c") && holds("e", "alpha"), "renameat2-noreplace-new", "r=%d errno=%s", r, strerror(errno));

    r = rename2("e", "x", RENAME_WHITEOUT);
    CHECK(r < 0 && errno == EINVAL && holds("e", "alpha"), "renameat2-unknown-flag", "r=%d errno=%s", r, strerror(errno));
}


/**
 * @brief Checks that a descriptor open on a file still reads it after the file is renamed.
 */
static void test_open_file(void) {

    char buffer[16] = {0};

    const int fd = open("e", O_RDONLY);

    const int r = rename("e", "f");

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

    mkdir("d1", 0755);
    put("d1/inner", "inner");

    int r = rename("d1", "d2");
    CHECK(r == 0 && gone("d1") && holds("d2/inner", "inner"), "rename-directory", "r=%d errno=%s", r, strerror(errno));

    r = rename("f", "d2");
    CHECK(r < 0 && errno == EISDIR, "rename-file-over-directory", "r=%d errno=%s", r, strerror(errno));

    r = rename("d2", "f");
    CHECK(r < 0 && errno == ENOTDIR, "rename-directory-over-file", "r=%d errno=%s", r, strerror(errno));

    mkdir("d3", 0755);
    put("d3/x", "x");

    r = rename("d2", "d3");
    CHECK(r < 0 && errno == ENOTEMPTY && holds("d2/inner", "inner"), "rename-over-full-directory", "r=%d errno=%s", r, strerror(errno));

    mkdir("d4", 0755);

    r = rename("d2/", "d4/");
    CHECK(r == 0 && gone("d2") && holds("d4/inner", "inner"), "rename-over-empty-directory", "r=%d errno=%s", r, strerror(errno));

    r = rename("f", "d4/f");
    CHECK(r < 0 && errno == EXDEV && holds("f", "alpha"), "rename-across-directories", "r=%d errno=%s", r, strerror(errno));
}


/**
 * @brief Renames relative to a directory descriptor.
 */
static void test_renameat(void) {

    const int dfd = open(".", O_RDONLY | O_DIRECTORY);

    const int r = dfd < 0 ? -1 : renameat(dfd, "f", dfd, "g");

    CHECK(r == 0 && gone("f") && holds("g", "alpha"), "renameat-dirfd", "dfd=%d r=%d errno=%s", dfd, r, strerror(errno));

    if (dfd >= 0) {
        close(dfd);
    }
}


/**
 * @brief Runs mv within the directory, and out of it where it has to copy.
 */
static void test_mv(void) {

    int r = system("mv g h");
    CHECK(r == 0 && gone("g") && holds("h", "alpha"), "mv-within-directory", "status=%d", r);

    r = system("mv h " MOVED);
    CHECK(r == 0 && gone("h") && holds(MOVED, "alpha"), "mv-out-of-directory-copies", "status=%d", r);

    unlink(MOVED);
}


/**
 * @brief Runs every case in a fresh scratch directory.
 * @param fs The filesystem the directory lives on.
 * @param scratch The scratch directory.
 */
static void run(const char* fs, const char* scratch) {

    char cleanup[64];
    snprintf(cleanup, sizeof(cleanup), "rm -rf %s", scratch);

    label = fs;

    system(cleanup);

    if (mkdir(scratch, 0755) < 0 || chdir(scratch) < 0) {
        CHECK(false, "scratch-directory", "%s: %s", scratch, strerror(errno));
        return;
    }

    test_files();
    test_flags();
    test_open_file();
    test_directories();
    test_renameat();
    test_mv();

    chdir("/");
    system(cleanup);
}


/**
 * @brief Runs every case on the tmpfs and on the ext2 root.
 * @return 0 when every case passed, 1 otherwise.
 */
int main(void) {

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("rename-test: starting\n");

    run("tmpfs", "/tmp/rename-test.d");
    run("ext2", "/rename-test.d");

    printf("rename-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
