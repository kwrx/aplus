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
 * @brief Tests for mkdir() and rmdir() on the ext2 root filesystem.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <unistd.h>


#define TEST_DIR "/ext2-dir-test.d"
#define MANY     300


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                            \
    {                                                                          \
        total++;                                                               \
        if (cond) {                                                            \
            printf("ext2-dir-test: PASS  %s\n", (name));                       \
        } else {                                                               \
            failures++;                                                        \
            printf("ext2-dir-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                      \
    }


/**
 * @brief The link count of a path.
 *
 * @param path The path.
 * @return Its st_nlink, or -1 when stat() fails.
 */
static long links(const char* path) {

    struct stat st;

    return stat(path, &st) < 0 ? -1 : (long)st.st_nlink;
}


/**
 * @brief The inode number of a path.
 *
 * @param path The path.
 * @return Its st_ino, or 0 when stat() fails.
 */
static ino_t inode(const char* path) {

    struct stat st;

    return stat(path, &st) < 0 ? 0 : st.st_ino;
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
 * @brief Counts the entries of a directory, and whether "." and ".." are among them.
 *
 * @param path The directory.
 * @param dots Where to store how many of "." and ".." it lists.
 * @return The number of entries, or -1 when it cannot be read.
 */
static int entries(const char* path, int* dots) {

    DIR* d = opendir(path);

    if (!d) {
        return -1;
    }

    int count = 0;
    *dots     = 0;

    struct dirent* ent;

    while ((ent = readdir(d))) {

        count++;

        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            (*dots)++;
        }
    }

    closedir(d);

    return count;
}


/**
 * @brief Reads the free inode and block counts of the root filesystem.
 *
 * @param inodes Where to store the free inodes.
 * @param blocks Where to store the free blocks.
 * @return 0 on success, or -1.
 */
static int space(unsigned long* inodes, unsigned long* blocks) {

    struct statfs st;

    if (statfs("/", &st) < 0) {
        return -1;
    }

    *inodes = st.f_ffree;
    *blocks = st.f_bfree;

    return 0;
}


/**
 * @brief mkdir() of a new directory, its mode, its links and its "." and ".." entries.
 */
static void test_create(void) {

    const long root   = links("/");
    const mode_t mask = umask(0);

    umask(mask);

    int r = mkdir(TEST_DIR, 0750);
    CHECK(r == 0, "mkdir", "%s", strerror(errno));

    struct stat st = {0};
    stat(TEST_DIR, &st);

    CHECK(S_ISDIR(st.st_mode), "is a directory", "mode %o", st.st_mode);
    CHECK((st.st_mode & 0777) == (0750 & ~mask), "mode", "%o", st.st_mode & 0777);
    CHECK(st.st_nlink == 2, "new directory has two links", "%ld", (long)st.st_nlink);
    CHECK(links("/") == root + 1, "parent gains a link", "%ld -> %ld", root, links("/"));

    errno = 0;
    r     = mkdir(TEST_DIR, 0755);
    CHECK(r < 0 && errno == EEXIST, "mkdir of an existing directory", "%d %s", r, strerror(errno));

    int dots        = 0;
    const int count = entries(TEST_DIR, &dots);
    CHECK(count == 2 && dots == 2, "lists only . and ..", "%d entries, %d dots", count, dots);

    CHECK(inode(TEST_DIR "/.") == inode(TEST_DIR), ". is the directory", "%lu %lu", (unsigned long)inode(TEST_DIR "/."), (unsigned long)inode(TEST_DIR));
    CHECK(inode(TEST_DIR "/..") == inode("/"), ".. is the parent", "%lu %lu", (unsigned long)inode(TEST_DIR "/.."), (unsigned long)inode("/"));
}


/**
 * @brief A nested directory with a file: rmdir() refuses it while it holds the file.
 */
static void test_nested(void) {

    int r = mkdir(TEST_DIR "/sub", 0755);
    CHECK(r == 0, "mkdir nested", "%s", strerror(errno));
    CHECK(links(TEST_DIR) == 3, "nested directory adds a link", "%ld", links(TEST_DIR));

    const int fd = open(TEST_DIR "/sub/file", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    CHECK(fd >= 0 && write(fd, "hello", 5) == 5, "file in a new directory", "%s", strerror(errno));

    if (fd >= 0) {
        close(fd);
    }

    char buffer[8] = {0};
    const int in   = open(TEST_DIR "/sub/file", O_RDONLY);
    CHECK(in >= 0 && read(in, buffer, sizeof(buffer)) == 5 && strcmp(buffer, "hello") == 0, "file reads back", "'%s'", buffer);

    if (in >= 0) {
        close(in);
    }

    errno = 0;
    r     = rmdir(TEST_DIR "/sub");
    CHECK(r < 0 && errno == ENOTEMPTY, "rmdir of a non-empty directory", "%d %s", r, strerror(errno));

    errno = 0;
    r     = rmdir(TEST_DIR "/sub/file");
    CHECK(r < 0 && errno == ENOTDIR, "rmdir of a file", "%d %s", r, strerror(errno));

    errno = 0;
    r     = unlink(TEST_DIR "/sub");
    CHECK(r < 0 && errno == EISDIR, "unlink of a directory", "%d %s", r, strerror(errno));

    CHECK(unlink(TEST_DIR "/sub/file") == 0, "unlink the file", "%s", strerror(errno));

    r = rmdir(TEST_DIR "/sub");
    CHECK(r == 0, "rmdir an emptied directory", "%s", strerror(errno));
    CHECK(gone(TEST_DIR "/sub"), "removed directory is gone", "%s", strerror(errno));
    CHECK(links(TEST_DIR) == 2, "parent loses the link", "%ld", links(TEST_DIR));
}


/**
 * @brief A directory created and removed gives back exactly the inode and the block it took; skipped without statfs().
 */
static void test_space(void) {

    unsigned long inodes_before = 0;
    unsigned long blocks_before = 0;
    unsigned long inodes_during = 0;
    unsigned long blocks_during = 0;
    unsigned long inodes_after  = 0;
    unsigned long blocks_after  = 0;

    if (space(&inodes_before, &blocks_before) < 0) {
        printf("ext2-dir-test: SKIP  space check, statfs: %s\n", strerror(errno));
        return;
    }

    CHECK(mkdir(TEST_DIR "/space", 0755) == 0, "mkdir for the space check", "%s", strerror(errno));

    space(&inodes_during, &blocks_during);

    CHECK(inodes_during == inodes_before - 1, "mkdir takes one inode", "%lu -> %lu", inodes_before, inodes_during);
    CHECK(blocks_during == blocks_before - 1, "mkdir takes one block", "%lu -> %lu", blocks_before, blocks_during);

    CHECK(rmdir(TEST_DIR "/space") == 0, "rmdir for the space check", "%s", strerror(errno));

    space(&inodes_after, &blocks_after);

    CHECK(inodes_after == inodes_before, "rmdir gives the inode back", "%lu -> %lu", inodes_before, inodes_after);
    CHECK(blocks_after == blocks_before, "rmdir gives the block back", "%lu -> %lu", blocks_before, blocks_after);
}


/**
 * @brief Enough subdirectories to grow the parent past one block, then all of them removed.
 */
static void test_many(void) {

    char path[64];
    int created = 0;

    for (int i = 0; i < MANY; i++) {

        snprintf(path, sizeof(path), TEST_DIR "/many-directory-%03d", i);

        if (mkdir(path, 0755) == 0) {
            created++;
        }
    }

    CHECK(created == MANY, "mkdir many", "%d of %d", created, MANY);
    CHECK(links(TEST_DIR) == 2 + MANY, "parent counts every subdirectory", "%ld", links(TEST_DIR));

    int dots        = 0;
    const int count = entries(TEST_DIR, &dots);
    CHECK(count == MANY + 2, "readdir lists every subdirectory", "%d", count);

    int removed = 0;

    for (int i = 0; i < MANY; i++) {

        snprintf(path, sizeof(path), TEST_DIR "/many-directory-%03d", i);

        if (rmdir(path) == 0) {
            removed++;
        }
    }

    CHECK(removed == MANY, "rmdir many", "%d of %d", removed, MANY);
    CHECK(links(TEST_DIR) == 2, "parent is back to two links", "%ld", links(TEST_DIR));

    const int left = entries(TEST_DIR, &dots);
    CHECK(left == 2, "only . and .. are left", "%d", left);
}


/**
 * @brief rmdir() of the test directory itself, and of the root.
 */
static void test_remove(void) {

    const long root = links("/");

    int r = rmdir(TEST_DIR);
    CHECK(r == 0, "rmdir", "%s", strerror(errno));
    CHECK(gone(TEST_DIR), "directory is gone", "%s", strerror(errno));
    CHECK(links("/") == root - 1, "root loses the link", "%ld -> %ld", root, links("/"));

    errno = 0;
    r     = rmdir("/");
    CHECK(r < 0 && errno == EBUSY, "rmdir of the root", "%d %s", r, strerror(errno));
}


/**
 * @brief Runs every test.
 *
 * @return 0 when they all pass, 1 otherwise.
 */
int main(void) {

    setvbuf(stdout, NULL, _IONBF, 0);

    system("rm -rf " TEST_DIR);

    printf("ext2-dir-test: starting\n");

    test_create();
    test_nested();
    test_space();
    test_many();
    test_remove();

    printf("ext2-dir-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
