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
 * @brief Regression tests for tmpfs file sizes, file offsets, symbolic links and the lifetime of removed files.
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
#include <sys/wait.h>
#include <unistd.h>


#define SCRATCH "/tmp/tmpfs-test.d"

#define CHAIN_LENGTH 12


/**
 * @brief How many files churn() creates to get freed kernel memory handed out again.
 */
#define CHURN_FILES 32


/**
 * @brief How many processes create files at once, and how many each creates, in the inode number check.
 */
#define INO_WORKERS 4
#define INO_FILES   300


/**
 * @brief How much the kernel heap may grow over a leak check before it counts as a leak, in kB.
 */
#define LEAK_SLACK_KB 256


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
 * @brief Tells whether an open file holds a patterned run of bytes from its start.
 *
 * @param fd The file.
 * @param size How many bytes to compare.
 * @param seed The pattern expected.
 * @return 1 if every byte matches, 0 otherwise.
 */
static int fd_has_pattern(int fd, size_t size, unsigned seed) {

    unsigned char chunk[1024];
    size_t done = 0;

    while (done < size) {

        size_t n = size - done < sizeof(chunk) ? size - done : sizeof(chunk);

        if (pread(fd, chunk, n, (off_t)done) != (ssize_t)n)
            return 0;

        for (size_t i = 0; i < n; i++) {

            if (chunk[i] != pattern(seed, done + i))
                return 0;
        }

        done += n;
    }

    return 1;
}


/**
 * @brief Creates and fills a batch of files, so that pages the kernel has just freed are handed out again.
 */
static void churn(void) {

    char path[64];

    for (int i = 0; i < CHURN_FILES; i++) {

        snprintf(path, sizeof(path), SCRATCH "/churn-%d", i);
        write_pattern(path, 4096, 100 + (unsigned)i);
    }
}


/**
 * @brief Checks the files churn() created and removes them.
 *
 * @return 1 if every file still held what was written to it.
 */
static int churn_intact(void) {

    char path[64];
    int intact = 1;

    for (int i = 0; i < CHURN_FILES; i++) {

        snprintf(path, sizeof(path), SCRATCH "/churn-%d", i);

        int fd = open(path, O_RDONLY);

        if (fd < 0 || !fd_has_pattern(fd, 4096, 100 + (unsigned)i))
            intact = 0;

        if (fd >= 0)
            close(fd);

        unlink(path);
    }

    return intact;
}


/**
 * @brief Reads how much memory the kernel heap holds, from the Slab line of /proc/meminfo.
 *
 * @return The heap size in kB, or -1 if it could not be read.
 */
static long slab_kb(void) {

    FILE* f = fopen("/proc/meminfo", "r");

    if (!f)
        return -1;


    char line[128];
    long kb = -1;

    while (kb < 0 && fgets(line, sizeof(line), f))
        sscanf(line, "Slab: %ld kB", &kb);

    fclose(f);

    return kb;
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


/**
 * @brief Reads, extends and names an open file after its only directory entry is gone.
 */
static void test_unlink_open(void) {

    const char* path = SCRATCH "/unlinked";

    if (write_pattern(path, 65536, 21) < 0) {
        CHECK(0, "unlink-open", "could not write %s: %s", path, strerror(errno));
        return;
    }

    int fd = open(path, O_RDWR);

    if (fd < 0) {
        CHECK(0, "unlink-open", "open(): %s", strerror(errno));
        return;
    }


    int e    = unlink(path);
    int gone = access(path, F_OK) < 0 && errno == ENOENT;

    CHECK(e == 0 && gone, "unlink-open-gone", "unlink returned %d and the name is %s", e, gone ? "gone" : "still there");

    churn();

    CHECK(fd_has_pattern(fd, 65536, 21), "unlink-open-read", "%s", "what was read back through the descriptor differs from what was written");


    unsigned char tail[4096];

    for (size_t i = 0; i < sizeof(tail); i++)
        tail[i] = pattern(21, 65536 + i);

    ssize_t w = pwrite(fd, tail, sizeof(tail), 65536);

    struct stat st;
    int s = fstat(fd, &st);

    CHECK(w == (ssize_t)sizeof(tail) && s == 0 && st.st_size == 65536 + (off_t)sizeof(tail) && fd_has_pattern(fd, 65536 + sizeof(tail), 21), "unlink-open-write", "pwrite returned %zd, fstat %d, size %ld", w, s, s == 0 ? (long)st.st_size : -1L);


    char link[64];
    char target[256] = "";

    snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);

    ssize_t n = readlink(link, target, sizeof(target) - 1);

    CHECK(n > 0, "unlink-open-proc", "readlink(%s) returned %zd: %s", link, n, strerror(errno));

    close(fd);

    CHECK(churn_intact(), "unlink-open-churn", "%s", "files written after the unlink were damaged");
}


/**
 * @brief Writes and reads back a file from tmpfile(), which removes its name as soon as it is created.
 */
static void test_tmpfile(void) {

    FILE* f = tmpfile();

    if (!f) {
        CHECK(0, "tmpfile", "tmpfile(): %s", strerror(errno));
        return;
    }


    static char buf[10000];

    for (size_t i = 0; i < sizeof(buf); i++)
        buf[i] = (char)pattern(33, i);

    size_t w = fwrite(buf, 1, sizeof(buf), f);

    fflush(f);
    churn();

    rewind(f);
    memset(buf, 0, sizeof(buf));

    size_t r = fread(buf, 1, sizeof(buf), f);
    int same = (r == sizeof(buf));

    for (size_t i = 0; same && i < r; i++)
        same = ((unsigned char)buf[i] == pattern(33, i));

    fclose(f);
    churn_intact();

    CHECK(w == sizeof(buf) && same, "tmpfile", "wrote %zu bytes and read back %zu, which are %s", w, r, same ? "the same" : "different");
}


/**
 * @brief Removes the working directory, then looks at it, tries to create a file in it and climbs out of it.
 */
static void test_rmdir_cwd(void) {

    const char* dir = SCRATCH "/gone";

    if (mkdir(dir, 0755) < 0 || chdir(dir) < 0) {
        CHECK(0, "rmdir-cwd", "could not enter %s: %s", dir, strerror(errno));
        return;
    }


    int e = unlinkat(AT_FDCWD, dir, AT_REMOVEDIR);

    CHECK(e == 0, "rmdir-cwd", "removing the working directory: %s", strerror(errno));

    churn();


    struct stat st;
    int s = stat(".", &st);

    CHECK(s == 0 && S_ISDIR(st.st_mode), "rmdir-cwd-stat", "stat(\".\") returned %d: %s", s, strerror(errno));


    errno  = 0;
    int fd = open("inside", O_WRONLY | O_CREAT, 0644);

    CHECK(fd < 0 && errno == ENOENT, "rmdir-cwd-creat", "creating a file in the removed directory returned %d, errno %d", fd, errno);

    if (fd >= 0)
        close(fd);


    char cwd[256] = "";
    int c         = chdir("..");

    if (c == 0 && !getcwd(cwd, sizeof(cwd)))
        cwd[0] = '\0';

    CHECK(c == 0 && strcmp(cwd, SCRATCH) == 0, "rmdir-cwd-parent", "chdir(\"..\") returned %d and the working directory is \"%s\"", c, cwd);

    chdir("/");

    CHECK(churn_intact(), "rmdir-cwd-churn", "%s", "files written after the removal were damaged");
}


/**
 * @brief Removes the working directory and then its parent, and climbs out through both.
 */
static void test_rmdir_ancestors(void) {

    const char* outer = SCRATCH "/outer";
    const char* inner = SCRATCH "/outer/inner";

    if (mkdir(outer, 0755) < 0 || mkdir(inner, 0755) < 0 || chdir(inner) < 0) {
        CHECK(0, "rmdir-ancestors", "could not enter %s: %s", inner, strerror(errno));
        return;
    }


    int e1 = unlinkat(AT_FDCWD, inner, AT_REMOVEDIR);
    int e2 = unlinkat(AT_FDCWD, outer, AT_REMOVEDIR);

    CHECK(e1 == 0 && e2 == 0, "rmdir-ancestors", "removing the two directories returned %d and %d: %s", e1, e2, strerror(errno));

    churn();


    struct stat st;

    int c1 = chdir("..");
    int s1 = stat(".", &st);
    int c2 = chdir("..");

    char cwd[256] = "";

    if (c2 == 0 && !getcwd(cwd, sizeof(cwd)))
        cwd[0] = '\0';

    CHECK(c1 == 0 && s1 == 0 && c2 == 0 && strcmp(cwd, SCRATCH) == 0, "rmdir-ancestors-walk", "chdir %d, stat %d, chdir %d, working directory \"%s\"", c1, s1, c2, cwd);

    chdir("/");

    CHECK(churn_intact(), "rmdir-ancestors-churn", "%s", "files written after the removal were damaged");
}


/**
 * @brief Refuses to remove a directory that still has a file in it, or a file as if it were a directory.
 */
static void test_rmdir_nonempty(void) {

    const char* dir  = SCRATCH "/full";
    const char* file = SCRATCH "/full/x";

    mkdir(dir, 0755);

    if (write_pattern(file, 16, 5) < 0) {
        CHECK(0, "rmdir-nonempty", "could not write %s: %s", file, strerror(errno));
        return;
    }


    errno = 0;
    int e = unlinkat(AT_FDCWD, dir, AT_REMOVEDIR);

    CHECK(e < 0 && errno == ENOTEMPTY, "rmdir-nonempty", "removing a directory with a file in it returned %d, errno %d", e, errno);


    int fd = open(file, O_RDONLY);

    CHECK(fd >= 0 && fd_has_pattern(fd, 16, 5), "rmdir-nonempty-file", "the file inside is %s", fd >= 0 ? "damaged" : "gone");

    if (fd >= 0)
        close(fd);


    errno = 0;
    e     = unlinkat(AT_FDCWD, file, AT_REMOVEDIR);

    CHECK(e < 0 && errno == ENOTDIR, "rmdir-notdir", "removing a file as a directory returned %d, errno %d", e, errno);


    unlink(file);

    errno = 0;
    e     = rmdir(dir);

    CHECK(e == 0 && access(dir, F_OK) < 0, "rmdir", "rmdir() of the emptied directory returned %d, errno %d", e, errno);
}


/**
 * @brief Creates and removes files from several processes at once, then checks each kept file is its own.
 *
 * Two files handed the same inode number share one data blob.
 */
static void test_ino_unique(void) {

    pid_t pids[INO_WORKERS];

    for (int k = 0; k < INO_WORKERS; k++) {

        if ((pids[k] = fork()) != 0)
            continue;


        char path[64];

        snprintf(path, sizeof(path), SCRATCH "/ino-%d", k);

        if (mkdir(path, 0755) < 0)
            _exit(1);

        for (int i = 0; i < INO_FILES; i++) {

            snprintf(path, sizeof(path), SCRATCH "/ino-%d/f-%d", k, i);

            if (write_pattern(path, 64, (unsigned)(k * 1000 + i)) < 0)
                _exit(2);

            if (i & 1) {

                snprintf(path, sizeof(path), SCRATCH "/ino-%d/f-%d", k, i - 1);

                if (unlink(path) < 0)
                    _exit(3);
            }
        }

        _exit(0);
    }


    int failed = 0;

    for (int k = 0; k < INO_WORKERS; k++) {

        int status = 0;

        if (pids[k] < 0 || waitpid(pids[k], &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
            failed++;
    }


    static ino_t inos[INO_WORKERS * INO_FILES / 2];

    int n     = 0;
    int wrong = 0;
    int dups  = 0;

    for (int k = 0; k < INO_WORKERS; k++) {

        for (int i = 1; i < INO_FILES; i += 2) {

            char path[64];
            snprintf(path, sizeof(path), SCRATCH "/ino-%d/f-%d", k, i);

            struct stat st;
            int fd = open(path, O_RDONLY);

            if (fd >= 0 && fstat(fd, &st) == 0 && fd_has_pattern(fd, 64, (unsigned)(k * 1000 + i)))
                inos[n++] = st.st_ino;
            else
                wrong++;

            if (fd >= 0)
                close(fd);

            unlink(path);
        }

        char path[64];
        snprintf(path, sizeof(path), SCRATCH "/ino-%d", k);

        unlinkat(AT_FDCWD, path, AT_REMOVEDIR);
    }

    for (int a = 0; a < n; a++) {

        for (int b = 0; b < a; b++) {

            if (inos[a] == inos[b])
                dups++;
        }
    }

    CHECK(failed == 0 && wrong == 0 && dups == 0, "ino-unique", "%d workers failed, %d files missing or wrong, %d inode numbers shared", failed, wrong, dups);
}


/**
 * @brief Removes open files and non-empty-then-emptied directories many times, checking the kernel heap stays put.
 */
static void test_leak_unlink(void) {

    const char* file = SCRATCH "/leak";
    const char* dir  = SCRATCH "/leakdir";
    const char* sub  = SCRATCH "/leakdir/x";

    long before = slab_kb();
    int done    = 0;

    for (int i = 0; i < 300; i++) {

        if (write_pattern(file, 8192, 7) < 0)
            break;

        int fd = open(file, O_RDONLY);

        if (fd < 0)
            break;

        unlink(file);
        close(fd);

        done++;
    }

    long after_files = slab_kb();

    CHECK(done == 300 && before >= 0 && after_files - before < LEAK_SLACK_KB, "leak-unlink-open", "%d rounds, kernel heap grew by %ld kB", done, after_files - before);


    done = 0;

    for (int i = 0; i < 300; i++) {

        if (mkdir(dir, 0755) < 0 || write_pattern(sub, 16, 7) < 0 || unlink(sub) < 0 || unlinkat(AT_FDCWD, dir, AT_REMOVEDIR) < 0)
            break;

        done++;
    }

    long after_dirs = slab_kb();

    CHECK(done == 300 && after_dirs - after_files < LEAK_SLACK_KB, "leak-rmdir", "%d rounds, kernel heap grew by %ld kB", done, after_dirs - after_files);
}


/**
 * @brief Creates, finds and removes a directory through paths that end in slashes, as mkdir -p and rm -r pass them.
 */
static void test_trailing_slash(void) {

    const char* dir = SCRATCH "/slashed//";

    errno  = 0;
    int e1 = mkdir(dir, 0755);

    struct stat st;
    int s1 = stat(SCRATCH "/slashed", &st);

    CHECK(e1 == 0 && s1 == 0 && S_ISDIR(st.st_mode), "trailing-slash-mkdir", "mkdir(\"%s\") returned %d, errno %d", dir, e1, errno);


    int s2 = stat(dir, &st);

    CHECK(s2 == 0 && S_ISDIR(st.st_mode), "trailing-slash-stat", "stat(\"%s\") returned %d, errno %d", dir, s2, errno);


    errno  = 0;
    int e2 = rmdir(dir);

    CHECK(e2 == 0 && access(SCRATCH "/slashed", F_OK) < 0, "trailing-slash-rmdir", "rmdir(\"%s\") returned %d, errno %d", dir, e2, errno);
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
    {"unlink-open", test_unlink_open},
    {"tmpfile", test_tmpfile},
    {"rmdir-cwd", test_rmdir_cwd},
    {"rmdir-ancestors", test_rmdir_ancestors},
    {"rmdir-nonempty", test_rmdir_nonempty},
    {"ino-unique", test_ino_unique},
    {"leak-unlink", test_leak_unlink},
    {"trailing-slash", test_trailing_slash},
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
