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
 * @brief Tests for link() and linkat() on tmpfs and ext2, and for ln on top of them.
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


#define ON_TMPFS "/tmp/link-test.d"
#define ON_EXT2  "/link-test.d"


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                               \
    {                                                                             \
        total++;                                                                  \
        if (cond) {                                                               \
            printf("link-test: PASS  %s %s\n", fs, (name));                       \
        } else {                                                                  \
            failures++;                                                           \
            printf("link-test: FAIL  %s %s: " fmt "\n", fs, (name), __VA_ARGS__); \
        }                                                                         \
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
 * @brief Reads the link count of a path, without following a final symbolic link.
 *
 * @param path The path.
 * @return The link count, or -1 when lstat() fails.
 */
static long nlink(const char* path) {

    struct stat st;

    return lstat(path, &st) < 0 ? -1 : (long)st.st_nlink;
}


/**
 * @brief Reports whether two paths name the same inode.
 *
 * @param a The first path.
 * @param b The second path.
 * @return true when both lstat() and share device and inode numbers.
 */
static bool same(const char* a, const char* b) {

    struct stat sa, sb;

    if (lstat(a, &sa) < 0 || lstat(b, &sb) < 0) {
        return false;
    }

    return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
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
 * @brief Runs the cases every filesystem with hard links must pass, in a scratch directory on it.
 *
 * @param fs A label for the filesystem.
 * @param base The scratch directory.
 */
static void test_common(const char* fs, const char* base) {

    char a[128], b[128], c[128], d[128], sub[128];

    snprintf(a, sizeof(a), "%s/a", base);
    snprintf(b, sizeof(b), "%s/b", base);
    snprintf(c, sizeof(c), "%s/c", base);
    snprintf(d, sizeof(d), "%s/d", base);
    snprintf(sub, sizeof(sub), "%s/sub", base);

    put(a, "alpha");

    int r = link(a, b);
    CHECK(r == 0 && holds(b, "alpha"), "link-file", "r=%d errno=%s", r, strerror(errno));
    CHECK(same(a, b) && nlink(a) == 2 && nlink(b) == 2, "link-shares-inode", "nlink a=%ld b=%ld", nlink(a), nlink(b));
    CHECK(listed(base, "a") == 1 && listed(base, "b") == 1, "link-listed", "a=%d b=%d", listed(base, "a"), listed(base, "b"));

    put(b, "beta");
    CHECK(holds(a, "beta"), "write-through-other-name", "a does not hold %s", "beta");

    r = link(a, b);
    CHECK(r < 0 && errno == EEXIST, "link-existing-name", "r=%d errno=%s", r, strerror(errno));

    r = link(a, a);
    CHECK(r < 0 && errno == EEXIST && nlink(a) == 2, "link-onto-itself", "r=%d errno=%s", r, strerror(errno));

    r = link(base, c);
    CHECK(r < 0 && errno == EPERM, "link-directory", "r=%d errno=%s", r, strerror(errno));

    char missing[128];
    snprintf(missing, sizeof(missing), "%s/missing", base);

    r = link(missing, c);
    CHECK(r < 0 && errno == ENOENT, "link-missing-source", "r=%d errno=%s", r, strerror(errno));

    char nodir[128];
    snprintf(nodir, sizeof(nodir), "%s/nodir/x", base);

    r = link(a, nodir);
    CHECK(r < 0 && errno == ENOENT, "link-missing-directory", "r=%d errno=%s", r, strerror(errno));

    r = mkdir(sub, 0755);

    char insub[128];
    snprintf(insub, sizeof(insub), "%s/sub/a", base);

    r = r == 0 ? link(a, insub) : -1;
    CHECK(r == 0 && same(a, insub) && nlink(a) == 3, "link-other-directory", "r=%d errno=%s nlink=%ld", r, strerror(errno), nlink(a));

    r = unlink(a);
    CHECK(r == 0 && nlink(b) == 2 && holds(b, "beta") && holds(insub, "beta"), "unlink-one-name", "r=%d errno=%s nlink=%ld", r, strerror(errno), nlink(b));

    r = unlink(insub);
    CHECK(r == 0 && nlink(b) == 1 && holds(b, "beta") && rmdir(sub) == 0, "unlink-second-name", "r=%d errno=%s nlink=%ld", r, strerror(errno), nlink(b));

    const int fd = open(b, O_RDONLY);

    r = linkat(fd, "", AT_FDCWD, c, AT_EMPTY_PATH);
    CHECK(r == 0 && same(b, c), "linkat-empty-path", "r=%d errno=%s", r, strerror(errno));

    r = linkat(fd, "", AT_FDCWD, d, 0);
    CHECK(r < 0 && errno == ENOENT, "linkat-empty-path-without-flag", "r=%d errno=%s", r, strerror(errno));

    close(fd);

    const int dfd = open(base, O_RDONLY | O_DIRECTORY);

    r = linkat(dfd, "c", dfd, "d", 0);
    CHECK(r == 0 && same(c, d) && nlink(d) == 3, "linkat-relative", "r=%d errno=%s nlink=%ld", r, strerror(errno), nlink(d));

    r = linkat(dfd, "c", dfd, "x", 0x8000);
    CHECK(r < 0 && errno == EINVAL, "linkat-unknown-flag", "r=%d errno=%s", r, strerror(errno));

    close(dfd);

    char cmd[512];
    char e[128];
    snprintf(e, sizeof(e), "%s/e", base);
    snprintf(cmd, sizeof(cmd), "ln %s %s", d, e);

    r = system(cmd);
    CHECK(r == 0 && same(d, e) && nlink(e) == 4, "ln", "status=%d nlink=%ld", r, nlink(e));

    unlink(b);
    unlink(c);
    unlink(d);

    CHECK(nlink(e) == 1 && holds(e, "beta"), "last-name-keeps-data", "nlink=%ld", nlink(e));

    unlink(e);
}


/**
 * @brief Checks that a file still open after its last name went keeps its data on tmpfs.
 */
static void test_tmpfs_open(void) {

    const char* fs = "tmpfs";

    put(ON_TMPFS "/o", "omega");
    link(ON_TMPFS "/o", ON_TMPFS "/p");

    const int fd = open(ON_TMPFS "/p", O_RDONLY);

    unlink(ON_TMPFS "/o");
    unlink(ON_TMPFS "/p");

    char buffer[16] = {0};
    const ssize_t n = fd >= 0 ? pread(fd, buffer, sizeof(buffer) - 1, 0) : -1;

    CHECK(n == 5 && strcmp(buffer, "omega") == 0, "open-after-last-unlink", "n=%zd buffer=%s", n, buffer);

    if (fd >= 0) {
        close(fd);
    }
}


/**
 * @brief Checks the cross-filesystem refusal and links to the symbolic links ext2 images ship with.
 */
static void test_cross(void) {

    const char* fs = "ext2";

    put(ON_TMPFS "/x", "xi");

    int r = link(ON_TMPFS "/x", ON_EXT2 "/x");
    CHECK(r < 0 && errno == EXDEV, "link-across-filesystems", "r=%d errno=%s", r, strerror(errno));

    unlink(ON_TMPFS "/x");

    struct stat st;

    if (lstat("/bin", &st) < 0 || !S_ISLNK(st.st_mode)) {
        printf("link-test: SKIP  ext2 symlink cases, /bin is not a symbolic link\n");
        return;
    }

    r = link("/bin", ON_EXT2 "/s");
    CHECK(r == 0 && same("/bin", ON_EXT2 "/s") && lstat(ON_EXT2 "/s", &st) == 0 && S_ISLNK(st.st_mode) && nlink("/bin") == 2, "link-symlink-itself", "r=%d errno=%s", r, strerror(errno));

    r = unlink(ON_EXT2 "/s");
    CHECK(r == 0 && lstat("/bin", &st) == 0 && S_ISLNK(st.st_mode) && nlink("/bin") == 1, "unlink-symlink-name", "r=%d errno=%s", r, strerror(errno));

    r = linkat(AT_FDCWD, "/bin", AT_FDCWD, ON_EXT2 "/t", AT_SYMLINK_FOLLOW);
    CHECK(r < 0 && errno == EPERM, "linkat-follow-to-directory", "r=%d errno=%s", r, strerror(errno));
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
        printf("link-test: mkdir: %s\n", strerror(errno));
        return 1;
    }

    printf("link-test: starting\n");

    test_common("tmpfs", ON_TMPFS);
    test_common("ext2", ON_EXT2);
    test_tmpfs_open();
    test_cross();

    system("rm -rf " ON_TMPFS " " ON_EXT2);

    printf("link-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
