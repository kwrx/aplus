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
 * @brief Tests for statfs(), fstatfs() and the mount table in /proc/mounts.
 */

#include <errno.h>
#include <fcntl.h>
#include <mntent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <unistd.h>


#define EXT2_MAGIC  0xEF53
#define TMPFS_MAGIC 0x01021994
#define PROC_MAGIC  0x9FA0

#define ON_EXT2  "/statfs-test.bin"
#define ON_TMPFS "/tmp/statfs-test.bin"
#define LINK     "/statfs-test.lnk"
#define JAIL     "/tmp/statfs-jail"
#define INNER    JAIL "/in ner"
#define SOURCED  "/tmp/statfs-sourced"

#define WRITTEN (256 * 1024)


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                          \
    {                                                                        \
        total++;                                                             \
        if (cond) {                                                          \
            printf("statfs-test: PASS  %s\n", (name));                       \
        } else {                                                             \
            failures++;                                                      \
            printf("statfs-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                    \
    }


/**
 * @brief Writes a file of a given size and flushes it.
 *
 * @param path The file.
 * @param size The number of bytes to write.
 * @return true if every byte was written.
 */
static bool fill(const char* path, size_t size) {

    static char block[4096];

    const int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);

    if (fd < 0) {
        return false;
    }

    memset(block, 'x', sizeof(block));

    size_t done = 0;

    while (done < size && write(fd, block, sizeof(block)) == (ssize_t)sizeof(block)) {
        done += sizeof(block);
    }

    fsync(fd);
    close(fd);

    return done >= size;
}


/**
 * @brief The f_type, geometry and flags statfs() reports for each kind of filesystem.
 */
static void test_types(void) {

    struct statfs st;

    errno = 0;
    int r = statfs("/", &st);
    CHECK(r == 0, "statfs of /", "%s", strerror(errno));
    CHECK(st.f_type == EXT2_MAGIC, "/ is ext2", "f_type %#lx", (unsigned long)st.f_type);
    CHECK(st.f_bsize > 0 && st.f_frsize > 0, "ext2 block size", "bsize %lu frsize %lu", (unsigned long)st.f_bsize, (unsigned long)st.f_frsize);
    CHECK(st.f_blocks > 0 && st.f_bfree <= st.f_blocks && st.f_bavail <= st.f_bfree, "ext2 block counts", "blocks %llu bfree %llu bavail %llu", (unsigned long long)st.f_blocks, (unsigned long long)st.f_bfree,
          (unsigned long long)st.f_bavail);
    CHECK(st.f_files > 0 && st.f_ffree <= st.f_files, "ext2 inode counts", "files %llu ffree %llu", (unsigned long long)st.f_files, (unsigned long long)st.f_ffree);
    CHECK(st.f_namelen == 255, "ext2 name length", "%lu", (unsigned long)st.f_namelen);
    CHECK(st.f_flags & 0x20, "ST_VALID is set", "f_flags %#lx", (unsigned long)st.f_flags);

    printf("statfs-test: INFO  / has %llu of %llu blocks of %lu bytes free\n", (unsigned long long)st.f_bfree, (unsigned long long)st.f_blocks, (unsigned long)st.f_frsize);

    errno = 0;
    r     = statfs("/tmp", &st);
    CHECK(r == 0 && st.f_type == TMPFS_MAGIC, "/tmp is tmpfs", "%d %s f_type %#lx", r, strerror(errno), (unsigned long)st.f_type);
    CHECK(st.f_blocks > 0 && st.f_files > 0 && st.f_ffree <= st.f_files, "tmpfs counts", "blocks %llu files %llu ffree %llu", (unsigned long long)st.f_blocks, (unsigned long long)st.f_files, (unsigned long long)st.f_ffree);

    errno = 0;
    r     = statfs("/proc", &st);
    CHECK(r == 0 && st.f_type == PROC_MAGIC, "/proc is proc", "%d %s f_type %#lx", r, strerror(errno), (unsigned long)st.f_type);

    errno = 0;
    r     = statfs("/proc/meminfo", &st);
    CHECK(r == 0 && st.f_type == PROC_MAGIC, "a file in /proc is on proc", "%d %s f_type %#lx", r, strerror(errno), (unsigned long)st.f_type);

    errno = 0;
    r     = statfs("/dev", &st);
    CHECK(r == 0 && st.f_type == TMPFS_MAGIC && st.f_blocks > 0, "the /dev bind mount reports its source", "%d %s f_type %#lx blocks %llu", r, strerror(errno), (unsigned long)st.f_type, (unsigned long long)st.f_blocks);

    errno = 0;
    r     = statfs("/dev/null", &st);
    CHECK(r == 0 && st.f_type == TMPFS_MAGIC, "a device node", "%d %s f_type %#lx", r, strerror(errno), (unsigned long)st.f_type);
}


/**
 * @brief fstatfs() agrees with statfs(), statvfs() converts it, and statfs() follows a final symlink.
 */
static void test_descriptors(void) {

    struct statfs a;
    struct statfs b;

    statfs("/", &a);

    const int fd = open("/", O_RDONLY | O_DIRECTORY);

    errno = 0;
    int r = fd >= 0 ? fstatfs(fd, &b) : -2;
    CHECK(r == 0, "fstatfs of /", "%d %s", r, strerror(errno));
    CHECK(a.f_type == b.f_type && a.f_blocks == b.f_blocks && a.f_bsize == b.f_bsize, "fstatfs matches statfs", "f_type %#lx/%#lx blocks %llu/%llu", (unsigned long)a.f_type, (unsigned long)b.f_type, (unsigned long long)a.f_blocks,
          (unsigned long long)b.f_blocks);

    if (fd >= 0) {
        close(fd);
    }

    struct statvfs v;

    errno = 0;
    r     = statvfs("/", &v);
    CHECK(r == 0, "statvfs of /", "%d %s", r, strerror(errno));
    CHECK(v.f_blocks == a.f_blocks && v.f_frsize == a.f_frsize && v.f_namemax == a.f_namelen, "statvfs matches statfs", "blocks %llu/%llu frsize %lu/%lu", (unsigned long long)v.f_blocks, (unsigned long long)a.f_blocks,
          (unsigned long)v.f_frsize, (unsigned long)a.f_frsize);
    CHECK(!(v.f_flag & ST_RDONLY), "/ is not read-only", "f_flag %#lx", (unsigned long)v.f_flag);

    unlink(LINK);

    errno = 0;
    r     = symlink("/tmp", LINK);
    CHECK(r == 0, "symlink from ext2 to tmpfs", "%d %s", r, strerror(errno));

    errno = 0;
    r     = statfs(LINK, &b);
    CHECK(r == 0 && b.f_type == TMPFS_MAGIC, "statfs follows a final symlink", "%d %s f_type %#lx", r, strerror(errno), (unsigned long)b.f_type);

    unlink(LINK);

    int fds[2] = {-1, -1};
    pipe(fds);

    errno = 0;
    r     = fds[0] >= 0 ? fstatfs(fds[0], &b) : -2;
    CHECK(r == 0, "fstatfs of a pipe", "%d %s", r, strerror(errno));

    close(fds[0]);
    close(fds[1]);

    errno = 0;
    r     = statfs("/statfs-test/does/not/exist", &b);
    CHECK(r < 0 && errno == ENOENT, "statfs of a missing path", "%d %s", r, strerror(errno));

    errno = 0;
    r     = fstatfs(-1, &b);
    CHECK(r < 0 && errno == EBADF, "fstatfs of -1", "%d %s", r, strerror(errno));

    errno = 0;
    r     = fstatfs(fds[0], &b);
    CHECK(r < 0 && errno == EBADF, "fstatfs of a closed descriptor", "%d %s", r, strerror(errno));
}


/**
 * @brief The free block and inode counts move when a file is written and given back when it is removed.
 *
 * @param path The file to write.
 * @param dir A path on the same filesystem.
 * @param fs The filesystem, for the messages.
 */
static void test_counters(const char* path, const char* dir, const char* fs) {

    char name[96];

    unlink(path);

    struct statfs before;
    struct statfs during;
    struct statfs after;

    statfs(dir, &before);

    snprintf(name, sizeof(name), "write %d KiB on %s", WRITTEN / 1024, fs);
    CHECK(fill(path, WRITTEN), name, "%s", strerror(errno));

    statfs(dir, &during);

    const unsigned long long used = (before.f_bfree - during.f_bfree) * before.f_frsize;

    snprintf(name, sizeof(name), "free space shrinks on %s", fs);
    CHECK(during.f_bfree < before.f_bfree && used >= WRITTEN, name, "bfree %llu -> %llu (%llu bytes)", (unsigned long long)before.f_bfree, (unsigned long long)during.f_bfree, used);

    snprintf(name, sizeof(name), "free inodes shrink on %s", fs);
    CHECK(during.f_ffree == before.f_ffree - 1, name, "ffree %llu -> %llu", (unsigned long long)before.f_ffree, (unsigned long long)during.f_ffree);

    unlink(path);

    statfs(dir, &after);

    snprintf(name, sizeof(name), "free space comes back on %s", fs);
    CHECK(after.f_bfree == before.f_bfree, name, "bfree %llu -> %llu", (unsigned long long)before.f_bfree, (unsigned long long)after.f_bfree);

    snprintf(name, sizeof(name), "free inodes come back on %s", fs);
    CHECK(after.f_ffree == before.f_ffree, name, "ffree %llu -> %llu", (unsigned long long)before.f_ffree, (unsigned long long)after.f_ffree);
}


/**
 * @brief Looks a mountpoint up in a mount table.
 *
 * @param table The table, such as /proc/mounts.
 * @param dir The mountpoint.
 * @param fsname Receives the entry's source, 128 bytes.
 * @param type Receives the entry's filesystem type, 128 bytes.
 * @param opts Receives the entry's options, 128 bytes.
 * @return true if the mountpoint is listed, the last entry winning.
 */
static bool find_mount(const char* table, const char* dir, char* fsname, char* type, char* opts) {

    FILE* fp = setmntent(table, "r");

    if (!fp) {
        return false;
    }

    bool found = false;

    for (struct mntent* m; (m = getmntent(fp));) {

        if (strcmp(m->mnt_dir, dir) != 0) {
            continue;
        }

        snprintf(fsname, 128, "%s", m->mnt_fsname);
        snprintf(type, 128, "%s", m->mnt_type);
        snprintf(opts, 128, "%s", m->mnt_opts);

        found = true;
    }

    endmntent(fp);

    return found;
}


/**
 * @brief Reads a whole file into a buffer.
 *
 * @param fd The file, read from its current offset.
 * @param buf The buffer.
 * @param size The size of the buffer.
 * @return The number of bytes read, NUL terminated.
 */
static size_t slurp(int fd, char* buf, size_t size) {

    size_t n = 0;

    for (ssize_t r; n + 1 < size && (r = read(fd, &buf[n], size - n - 1)) > 0;) {
        n += (size_t)r;
    }

    buf[n] = '\0';

    return n;
}


/**
 * @brief /proc/mounts links to the caller's own table and lists the boot mounts in the Linux format.
 */
static void test_proc_mounts(void) {

    char target[64] = {0};

    errno       = 0;
    ssize_t len = readlink("/proc/mounts", target, sizeof(target) - 1);
    CHECK(len > 0 && strcmp(target, "self/mounts") == 0, "/proc/mounts links to self/mounts", "%zd %s '%s'", len, strerror(errno), target);

    char fsname[128] = {0};
    char type[128]   = {0};
    char opts[128]   = {0};

    bool f = find_mount("/proc/mounts", "/", fsname, type, opts);
    CHECK(f && strcmp(type, "ext2") == 0 && strncmp(fsname, "/dev/", 5) == 0, "/ is listed as an ext2 device", "found %d '%s' '%s'", f, fsname, type);
    CHECK(f && strncmp(opts, "rw", 2) == 0, "/ is listed read-write", "'%s'", opts);

    f = find_mount("/proc/mounts", "/tmp", fsname, type, opts);
    CHECK(f && strcmp(type, "tmpfs") == 0, "/tmp is listed as tmpfs", "found %d '%s'", f, type);
    CHECK(f && strstr(opts, "nosuid") && strstr(opts, "nodev"), "/tmp keeps its fstab options", "'%s'", opts);

    f = find_mount("/proc/mounts", "/proc", fsname, type, opts);
    CHECK(f && strcmp(type, "proc") == 0, "/proc is listed as proc", "found %d '%s'", f, type);

    f = find_mount("/proc/mounts", "/dev", fsname, type, opts);
    CHECK(f && strcmp(type, "tmpfs") == 0, "/dev is listed with its source's type", "found %d '%s' '%s'", f, fsname, type);


    FILE* fp = setmntent("/proc/mounts", "r");

    int entries = 0;
    int hidden  = 0;
    int bad     = 0;

    for (struct mntent* m; fp && (m = getmntent(fp));) {

        printf("statfs-test: INFO  mount %s %s %s %s\n", m->mnt_fsname, m->mnt_dir, m->mnt_type, m->mnt_opts);

        entries++;

        if (strcmp(m->mnt_dir, "/root") == 0 || strncmp(m->mnt_dir, "/root/", 6) == 0) {
            hidden++;
        }

        struct statfs st;

        if (statfs(m->mnt_dir, &st) < 0) {
            bad++;
        }
    }

    if (fp) {
        endmntent(fp);
    }

    CHECK(entries >= 4, "the boot mounts are listed", "%d entries", entries);
    CHECK(hidden == 0, "kernel paths outside the root are hidden", "%d under /root", hidden);
    CHECK(bad == 0, "every listed mountpoint answers statfs", "%d failed", bad);


    char self[4096];
    char mine[4096];
    char path[64];

    snprintf(path, sizeof(path), "/proc/%d/mounts", getpid());

    int a = open("/proc/self/mounts", O_RDONLY);
    int b = open(path, O_RDONLY);

    slurp(a, self, sizeof(self));
    slurp(b, mine, sizeof(mine));

    CHECK(a >= 0 && b >= 0 && strcmp(self, mine) == 0, "/proc/<pid>/mounts matches /proc/self/mounts", "%d %d", a, b);

    close(a);
    close(b);
}


/**
 * @brief A chrooted task sees only the mounts beneath its root, named from it, with separators escaped.
 */
static void test_chroot_view(void) {

    mkdir(JAIL, 0755);
    mkdir(INNER, 0755);

    errno = 0;
    int r = mount("my src", INNER, "tmpfs", MS_NOSUID, NULL);
    CHECK(r == 0, "mount a tmpfs with spaces in its names", "%d %s", r, strerror(errno));

    if (r < 0) {
        return;
    }

    char table[4096];

    const int fd = open("/proc/mounts", O_RDONLY);

    slurp(fd, table, sizeof(table));
    close(fd);

    CHECK(strstr(table, "my\\040src /tmp/statfs-jail/in\\040ner tmpfs rw,nosuid 0 0\n") != NULL, "the new mount is listed with its separators escaped", "'%s'", table);

    struct statvfs v;

    errno = 0;
    r     = statvfs(INNER, &v);
    CHECK(r == 0 && (v.f_flag & ST_NOSUID), "statvfs reports the mount's nosuid", "%d %s f_flag %#lx", r, strerror(errno), (unsigned long)v.f_flag);


    int pipefd[2];
    pipe(pipefd);

    const pid_t pid = fork();

    if (pid == 0) {

        close(pipefd[0]);

        const int fd = open("/proc/self/mounts", O_RDONLY);

        char buf[1024] = {0};

        if (fd >= 0 && chroot(JAIL) == 0 && chdir("/") == 0) {
            slurp(fd, buf, sizeof(buf));
        } else {
            snprintf(buf, sizeof(buf), "setup failed: %s", strerror(errno));
        }

        write(pipefd[1], buf, strlen(buf));

        _exit(0);
    }

    close(pipefd[1]);

    char seen[1024] = {0};
    slurp(pipefd[0], seen, sizeof(seen));

    close(pipefd[0]);
    waitpid(pid, NULL, 0);

    CHECK(strcmp(seen, "my\\040src /in\\040ner tmpfs rw,nosuid 0 0\n") == 0, "a chrooted task sees only its own mounts", "'%s'", seen);
}


/**
 * @brief A filesystem without a device ignores the source it is given, and one that needs a device refuses none.
 */
static void test_mount_sources(void) {

    mkdir(SOURCED, 0755);

    errno = 0;
    int r = mount("/dev/null", SOURCED, "tmpfs", 0, NULL);
    CHECK(r == 0, "tmpfs ignores an existing source", "%d %s", r, strerror(errno));

    char fsname[128] = {0};
    char type[128]   = {0};
    char opts[128]   = {0};

    bool f = find_mount("/proc/mounts", SOURCED, fsname, type, opts);
    CHECK(f && strcmp(fsname, "/dev/null") == 0 && strcmp(type, "tmpfs") == 0 && strcmp(opts, "rw") == 0, "it is listed under that source", "found %d '%s' '%s' '%s'", f, fsname, type, opts);

    errno = 0;
    r     = mount(NULL, SOURCED, "ext2", 0, NULL);
    CHECK(r < 0 && errno == EINVAL, "ext2 without a source", "%d %s", r, strerror(errno));
}


/**
 * @brief Runs every test.
 *
 * @return 0 when they all pass, 1 otherwise.
 */
int main(void) {

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("statfs-test: starting\n");

    test_types();
    test_descriptors();
    test_counters(ON_EXT2, "/", "ext2");
    test_counters(ON_TMPFS, "/tmp", "tmpfs");
    test_proc_mounts();
    test_chroot_view();
    test_mount_sources();

    printf("statfs-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
