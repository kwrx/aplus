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
 * @brief Tests for symlink(), symlinkat() and readlink() on tmpfs and ext2, and for ln -s on top of them.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#define ON_TMPFS "/tmp/symlink-test.d"
#define ON_EXT2  "/symlink-test.d"


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                                  \
    {                                                                                \
        total++;                                                                     \
        if (cond) {                                                                  \
            printf("symlink-test: PASS  %s %s\n", fs, (name));                       \
        } else {                                                                     \
            failures++;                                                              \
            printf("symlink-test: FAIL  %s %s: " fmt "\n", fs, (name), __VA_ARGS__); \
        }                                                                            \
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
 * @brief Reports whether a file, reached through every link on its path, holds exactly some text.
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
 * @brief Reports whether readlink() gives back exactly a target, and lstat() its length.
 *
 * @param path The link.
 * @param target The target expected.
 * @return true when both agree with it.
 */
static bool reads_as(const char* path, const char* target) {

    static char buffer[8192];

    struct stat st;

    const ssize_t n = readlink(path, buffer, sizeof(buffer));

    if (n != (ssize_t)strlen(target) || memcmp(buffer, target, n) != 0) {
        return false;
    }

    return lstat(path, &st) == 0 && S_ISLNK(st.st_mode) && st.st_size == (off_t)n;
}


/**
 * @brief Reports whether a path names a symbolic link.
 *
 * @param path The path.
 * @return true when lstat() succeeds and reports a link.
 */
static bool is_link(const char* path) {

    struct stat st;

    return lstat(path, &st) == 0 && S_ISLNK(st.st_mode);
}


/**
 * @brief Reports whether nothing at all, not even a link, carries a path.
 *
 * @param path The path.
 * @return true when lstat() fails with ENOENT.
 */
static bool gone(const char* path) {

    struct stat st;

    return lstat(path, &st) < 0 && errno == ENOENT;
}


/**
 * @brief Counts the entries of a directory with a given name.
 *
 * @param dir The directory.
 * @param name The name.
 * @return How many entries carry it, or -1 when the directory cannot be read.
 */
static int listed(const char* dir, const char* name) {

    DIR* d = opendir(dir);

    if (!d) {
        return -1;
    }

    int n = 0;

    for (struct dirent* e; (e = readdir(d)) != NULL;) {
        if (strcmp(e->d_name, name) == 0) {
            n++;
        }
    }

    closedir(d);

    return n;
}


/**
 * @brief Builds a relative target of an exact length that leads to "a" or "ab" in the link's own directory.
 *
 * @param out Receives the target.
 * @param length The length wanted, at least 2.
 */
static void padded(char* out, size_t length) {

    const char* name = (length % 2) ? "a" : "ab";
    size_t k         = (length - strlen(name)) / 2;

    out[0] = '\0';

    for (size_t i = 0; i < k; i++) {
        strcat(out, "./");
    }

    strcat(out, name);
}


/**
 * @brief Checks links whose targets straddle the 60 bytes ext2 keeps inside the inode, and one far longer.
 *
 * @param fs A label for the filesystem.
 * @param base The scratch directory.
 */
static void test_lengths(const char* fs, const char* base) {

    static const size_t lengths[] = {1, 59, 60, 61, 1000};

    static char target[1024];

    char link[128];
    char name[32];

    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {

        padded(target, lengths[i]);
        snprintf(link, sizeof(link), "%s/len-%zu", base, lengths[i]);
        snprintf(name, sizeof(name), "length-%zu", lengths[i]);

        const int r = symlink(target, link);
        CHECK(r == 0 && reads_as(link, target) && holds(link, "alpha"), name, "r=%d errno=%s", r, strerror(errno));
    }

    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {

        snprintf(link, sizeof(link), "%s/len-%zu", base, lengths[i]);
        snprintf(name, sizeof(name), "unlink-length-%zu", lengths[i]);

        const int r = unlink(link);
        CHECK(r == 0 && gone(link), name, "r=%d errno=%s", r, strerror(errno));
    }
}


/**
 * @brief Runs the cases every filesystem with symbolic links must pass, in a scratch directory on it.
 *
 * @param fs A label for the filesystem.
 * @param base The scratch directory.
 */
static void test_common(const char* fs, const char* base) {

    char a[128], ab[128], l1[128], l2[128], dl[128], loop1[128], loop2[128], sub[128], subf[128], ld[128], inld[128], hard[128], ln[128], miss[128], buf[512];

    snprintf(a, sizeof(a), "%s/a", base);
    snprintf(ab, sizeof(ab), "%s/ab", base);
    snprintf(l1, sizeof(l1), "%s/l1", base);
    snprintf(l2, sizeof(l2), "%s/l2", base);
    snprintf(dl, sizeof(dl), "%s/dangling", base);
    snprintf(loop1, sizeof(loop1), "%s/loop1", base);
    snprintf(loop2, sizeof(loop2), "%s/loop2", base);
    snprintf(sub, sizeof(sub), "%s/sub", base);
    snprintf(subf, sizeof(subf), "%s/sub/f", base);
    snprintf(ld, sizeof(ld), "%s/ld", base);
    snprintf(inld, sizeof(inld), "%s/ld/f", base);
    snprintf(hard, sizeof(hard), "%s/hard", base);
    snprintf(ln, sizeof(ln), "%s/ln", base);
    snprintf(miss, sizeof(miss), "%s/missing", base);

    put(a, "alpha");
    put(ab, "alpha");

    int r = symlink(a, l1);
    CHECK(r == 0 && is_link(l1) && listed(base, "l1") == 1, "symlink-absolute", "r=%d errno=%s", r, strerror(errno));
    CHECK(reads_as(l1, a), "readlink-absolute", "readlink(%s) does not give %s", l1, a);
    CHECK(holds(l1, "alpha"), "follow-absolute", "%s does not read as %s", l1, "alpha");

    struct stat sa, sl;
    CHECK(stat(a, &sa) == 0 && stat(l1, &sl) == 0 && sa.st_ino == sl.st_ino && S_ISREG(sl.st_mode), "stat-follows", "ino a=%ld l1=%ld", (long)sa.st_ino, (long)sl.st_ino);

    ssize_t n = readlink(l1, buf, 3);
    CHECK(n == 3 && memcmp(buf, a, 3) == 0, "readlink-short-buffer", "n=%zd errno=%s", n, strerror(errno));

    n = readlink(a, buf, sizeof(buf));
    CHECK(n < 0 && errno == EINVAL, "readlink-not-a-link", "n=%zd errno=%s", n, strerror(errno));

    r = symlink("a", l2);
    CHECK(r == 0 && reads_as(l2, "a") && holds(l2, "alpha"), "symlink-relative", "r=%d errno=%s", r, strerror(errno));

    r = symlink(miss, dl);
    CHECK(r == 0 && is_link(dl) && stat(dl, &sl) < 0 && errno == ENOENT, "symlink-dangling", "r=%d errno=%s", r, strerror(errno));

    r = symlink("x", a);
    CHECK(r < 0 && errno == EEXIST && holds(a, "alpha"), "symlink-over-file", "r=%d errno=%s", r, strerror(errno));

    r = symlink("x", dl);
    CHECK(r < 0 && errno == EEXIST && reads_as(dl, miss), "symlink-over-dangling-link", "r=%d errno=%s", r, strerror(errno));

    r = symlink("", ln);
    CHECK(r < 0 && errno == ENOENT && gone(ln), "symlink-empty-target", "r=%d errno=%s", r, strerror(errno));

    char* huge = malloc(4097);
    memset(huge, 'x', 4096);
    huge[4096] = '\0';

    r = symlink(huge, ln);
    CHECK(r < 0 && errno == ENAMETOOLONG && gone(ln), "symlink-target-too-long", "r=%d errno=%s", r, strerror(errno));

    free(huge);

    snprintf(buf, sizeof(buf), "%s/nodir/x", base);
    r = symlink("x", buf);
    CHECK(r < 0 && errno == ENOENT, "symlink-missing-directory", "r=%d errno=%s", r, strerror(errno));

    snprintf(buf, sizeof(buf), "%s/slash/", base);
    r = symlink("x", buf);
    CHECK(r < 0 && errno == ENOENT, "symlink-trailing-slash", "r=%d errno=%s", r, strerror(errno));

    const int dfd = open(base, O_RDONLY | O_DIRECTORY);

    r = symlinkat("ab", dfd, "l3");
    n = readlinkat(dfd, "l3", buf, sizeof(buf));
    CHECK(r == 0 && n == 2 && memcmp(buf, "ab", 2) == 0, "symlinkat-relative", "r=%d n=%zd errno=%s", r, n, strerror(errno));

    close(dfd);

    mkdir(sub, 0755);
    put(subf, "phi");

    r = symlink("sub", ld);
    CHECK(r == 0 && holds(inld, "phi") && stat(ld, &sl) == 0 && S_ISDIR(sl.st_mode) && is_link(ld), "symlink-to-directory", "r=%d errno=%s", r, strerror(errno));

    r = symlink(loop2, loop1);
    r |= symlink(loop1, loop2);

    errno        = 0;
    const int fd = open(loop1, O_RDONLY);
    CHECK(r == 0 && fd < 0 && errno == ELOOP, "symlink-loop", "r=%d fd=%d errno=%s", r, fd, strerror(errno));

    if (fd >= 0) {
        close(fd);
    }

    r = link(l2, hard);
    CHECK(r == 0 && reads_as(hard, "a"), "hard-link-to-symlink", "r=%d errno=%s", r, strerror(errno));

    r = unlink(l2);
    CHECK(r == 0 && gone(l2) && reads_as(hard, "a"), "unlink-first-name", "r=%d errno=%s", r, strerror(errno));

    r = unlink(hard);
    CHECK(r == 0 && gone(hard) && holds(a, "alpha"), "unlink-last-name", "r=%d errno=%s", r, strerror(errno));

    r = unlink(l1);
    CHECK(r == 0 && gone(l1) && holds(a, "alpha") && listed(base, "l1") == 0, "unlink-keeps-target", "r=%d errno=%s", r, strerror(errno));

    r = unlink(dl);
    CHECK(r == 0 && gone(dl), "unlink-dangling", "r=%d errno=%s", r, strerror(errno));

    r = unlink(ld);
    CHECK(r == 0 && gone(ld) && holds(subf, "phi"), "unlink-link-to-directory", "r=%d errno=%s", r, strerror(errno));

    snprintf(buf, sizeof(buf), "ln -s a %s", ln);
    r = system(buf);
    CHECK(r == 0 && reads_as(ln, "a") && holds(ln, "alpha"), "ln-s", "status=%d", r);

    test_lengths(fs, base);

    snprintf(buf, sizeof(buf), "rm -rf %s", base);
    r = system(buf);
    CHECK(r == 0 && gone(base), "rm-rf", "status=%d", r);
}


/**
 * @brief Checks a link that crosses from ext2 into tmpfs.
 */
static void test_cross(void) {

    const char* fs = "ext2";

    mkdir(ON_TMPFS, 0755);
    mkdir(ON_EXT2, 0755);
    put(ON_TMPFS "/x", "xi");

    int r = symlink(ON_TMPFS "/x", ON_EXT2 "/x");
    CHECK(r == 0 && holds(ON_EXT2 "/x", "xi") && reads_as(ON_EXT2 "/x", ON_TMPFS "/x"), "symlink-across-filesystems", "r=%d errno=%s", r, strerror(errno));

    unlink(ON_EXT2 "/x");
    unlink(ON_TMPFS "/x");
}


/**
 * @brief Runs every case in scratch directories on the tmpfs and on the ext2 root.
 *
 * @return 0 when every case passed, 1 otherwise.
 */
int main(void) {

    setvbuf(stdout, NULL, _IONBF, 0);

    system("rm -rf " ON_TMPFS " " ON_EXT2);

    if (mkdir(ON_TMPFS, 0755) < 0 || mkdir(ON_EXT2, 0755) < 0) {
        printf("symlink-test: mkdir: %s\n", strerror(errno));
        return 1;
    }

    printf("symlink-test: starting\n");

    test_common("tmpfs", ON_TMPFS);
    test_common("ext2", ON_EXT2);
    test_cross();

    system("rm -rf " ON_TMPFS " " ON_EXT2);

    printf("symlink-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
