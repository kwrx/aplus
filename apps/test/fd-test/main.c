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
 * @brief Regression tests for duplicating descriptors and for the descriptor and status flags.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                     \
    {                                                                   \
        total++;                                                        \
        if (cond) {                                                     \
            printf("fd-test: PASS  %s\n", (name));                      \
        } else {                                                        \
            failures++;                                                 \
            printf("fd-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                               \
    }


/**
 * @brief Opens /dev/null.
 *
 * @param flags Extra open() flags.
 * @return The descriptor, or -1 with errno set.
 */
static int open_null(int flags) {
    return open("/dev/null", O_RDONLY | flags);
}


/**
 * @brief Checks that a descriptor reusing the slot of a closed close-on-exec one does not inherit the flag.
 */
static void test_dup_reuses_slot(void) {

    int a = open_null(O_CLOEXEC);

    if (a < 0) {
        CHECK(0, "dup-reuses-slot", "open() failed: %s", strerror(errno));
        return;
    }

    close(a);

    int b = dup(0);

    if (b != a) {
        printf("fd-test: SKIP  dup-reuses-slot (dup() returned %d, not the freed %d)\n", b, a);
        close(b);
        return;
    }

    CHECK(fcntl(b, F_GETFD) == 0, "dup-reuses-slot", "%s", "dup() into the slot of a closed FD_CLOEXEC descriptor came out FD_CLOEXEC");

    close(b);
}


/**
 * @brief Checks that dup2() over a close-on-exec descriptor leaves the new one without the flag, and the old one with it.
 */
static void test_dup2_clears_cloexec(void) {

    int a = open_null(O_CLOEXEC);
    int b = open_null(O_CLOEXEC);

    if (a < 0 || b < 0) {
        CHECK(0, "dup2-cloexec", "open() failed: %s", strerror(errno));
        return;
    }

    int r = dup2(a, b);

    CHECK(r == b, "dup2-cloexec-ret", "dup2() returned %d, expected %d", r, b);
    CHECK(fcntl(b, F_GETFD) == 0, "dup2-cloexec", "%s", "the descriptor dup2() made came out FD_CLOEXEC");
    CHECK(fcntl(a, F_GETFD) == FD_CLOEXEC, "dup2-cloexec-source", "%s", "dup2() cleared FD_CLOEXEC on the descriptor it copied");

    close(a);
    close(b);
}


/**
 * @brief Checks that F_SETFL changes only the status flags, keeping the access mode.
 */
static void test_setfl_keeps_accmode(void) {

    int p[2];

    if (pipe(p) < 0) {
        CHECK(0, "setfl-accmode", "pipe() failed: %s", strerror(errno));
        return;
    }

    int r = fcntl(p[1], F_SETFL, O_NONBLOCK);
    int f = fcntl(p[1], F_GETFL);

    CHECK(r == 0, "setfl-ret", "F_SETFL returned %d (%s)", r, strerror(errno));
    CHECK(f >= 0 && (f & O_ACCMODE) == O_WRONLY, "setfl-accmode", "F_GETFL reads 0x%x after F_SETFL, access mode 0x%x instead of O_WRONLY", f, f & O_ACCMODE);
    CHECK(f >= 0 && (f & O_NONBLOCK), "setfl-nonblock", "F_GETFL reads 0x%x, without the O_NONBLOCK just set", f);

    ssize_t n = write(p[1], "x", 1);

    CHECK(n == 1, "setfl-write", "write() after F_SETFL returned %zd (%s)", n, strerror(errno));

    close(p[0]);
    close(p[1]);
}


/**
 * @brief Checks that F_SETFL can both set and clear O_NONBLOCK, and that reads honour it.
 */
static void test_setfl_status(void) {

    int p[2];

    if (pipe(p) < 0) {
        CHECK(0, "setfl-status", "pipe() failed: %s", strerror(errno));
        return;
    }

    fcntl(p[0], F_SETFL, O_NONBLOCK);

    char c;

    errno     = 0;
    ssize_t n = read(p[0], &c, 1);

    CHECK(n < 0 && errno == EAGAIN, "setfl-status-read", "a read() on an empty O_NONBLOCK pipe returned %zd (%s), expected EAGAIN", n, strerror(errno));

    fcntl(p[0], F_SETFL, 0);

    int f = fcntl(p[0], F_GETFL);

    CHECK(f >= 0 && !(f & O_NONBLOCK) && (f & O_ACCMODE) == O_RDONLY, "setfl-status-clear", "F_GETFL reads 0x%x after clearing O_NONBLOCK", f);

    close(p[0]);
    close(p[1]);
}


/**
 * @brief Checks that F_DUPFD and F_DUPFD_CLOEXEC honour their lowest descriptor, and refuse a negative or huge one.
 */
static void test_dupfd_min(void) {

    int a = fcntl(0, F_DUPFD, 10);

    CHECK(a >= 10, "dupfd-min", "F_DUPFD 10 returned %d", a);
    CHECK(a < 0 || fcntl(a, F_GETFD) == 0, "dupfd-no-cloexec", "%s", "F_DUPFD made a FD_CLOEXEC descriptor");

    int b = fcntl(0, F_DUPFD_CLOEXEC, 20);

    CHECK(b >= 20, "dupfd-cloexec-min", "F_DUPFD_CLOEXEC 20 returned %d", b);
    CHECK(b < 0 || fcntl(b, F_GETFD) == FD_CLOEXEC, "dupfd-cloexec", "%s", "F_DUPFD_CLOEXEC made a descriptor without FD_CLOEXEC");

    errno = 0;
    int c = fcntl(0, F_DUPFD, -1);

    CHECK(c < 0 && errno == EINVAL, "dupfd-negative", "F_DUPFD -1 returned %d (%s), expected EINVAL", c, strerror(errno));

    errno = 0;
    int d = fcntl(0, F_DUPFD, 1 << 20);

    CHECK(d < 0 && errno == EINVAL, "dupfd-huge", "F_DUPFD 1M returned %d (%s), expected EINVAL", d, strerror(errno));

    if (a >= 0)
        close(a);

    if (b >= 0)
        close(b);
}


/**
 * @brief Checks dup3() through the raw syscall, which libc would otherwise emulate with dup2() and fcntl().
 */
static void test_dup3(void) {

    int a = open_null(0);
    int b = open_null(0);

    if (a < 0 || b < 0) {
        CHECK(0, "dup3", "open() failed: %s", strerror(errno));
        return;
    }

    errno  = 0;
    long r = syscall(SYS_dup3, a, b, O_CLOEXEC);

    CHECK(r == b && fcntl(b, F_GETFD) == FD_CLOEXEC, "dup3-cloexec", "dup3(O_CLOEXEC) returned %ld (%s), FD_CLOEXEC %d", r, strerror(errno), fcntl(b, F_GETFD));

    errno = 0;
    r     = syscall(SYS_dup3, a, b, 0);

    CHECK(r == b && fcntl(b, F_GETFD) == 0, "dup3-plain", "dup3(0) returned %ld (%s), FD_CLOEXEC %d", r, strerror(errno), fcntl(b, F_GETFD));

    errno = 0;
    r     = syscall(SYS_dup3, a, a, 0);

    CHECK(r < 0 && errno == EINVAL, "dup3-same", "dup3() onto itself returned %ld (%s), expected EINVAL", r, strerror(errno));

    errno = 0;
    r     = syscall(SYS_dup3, a, b, O_CREAT);

    CHECK(r < 0 && errno == EINVAL, "dup3-flags", "dup3(O_CREAT) returned %ld (%s), expected EINVAL", r, strerror(errno));

    close(a);
    close(b);
}


/**
 * @brief Checks that two descriptors made by dup() share one file position.
 */
static void test_dup_shares_offset(void) {

    char path[64];

    snprintf(path, sizeof(path), "/tmp/fd-test.%d", (int)getpid());

    int a = open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);

    if (a < 0) {
        CHECK(0, "dup-offset", "open() failed: %s", strerror(errno));
        return;
    }

    write(a, "hello", 5);
    lseek(a, 0, SEEK_SET);

    int b = dup(a);

    char c;
    read(a, &c, 1);

    off_t off = lseek(b, 0, SEEK_CUR);

    CHECK(off == 1, "dup-offset", "the duplicate is at offset %ld after one byte was read from the original", (long)off);

    close(a);
    close(b);
    unlink(path);
}


/**
 * @brief Checks which descriptors survive exec: a close-on-exec one does not, its dup() does, its F_DUPFD_CLOEXEC does not.
 */
static void test_cloexec_exec(void) {

    int a = open_null(O_CLOEXEC);
    int b = dup(a);
    int c = fcntl(a, F_DUPFD_CLOEXEC, 30);

    if (a < 0 || b < 0 || c < 0) {
        CHECK(0, "cloexec-exec", "setup failed: %s", strerror(errno));
        return;
    }


    pid_t pid = fork();

    if (pid == 0) {

        char sa[16], sb[16], sc[16];

        snprintf(sa, sizeof(sa), "%d", a);
        snprintf(sb, sizeof(sb), "%d", b);
        snprintf(sc, sizeof(sc), "%d", c);

        execl("/usr/bin/fd-test", "fd-test", "--check-exec", sa, sb, sc, (char*)NULL);
        _exit(100);
    }


    int status = 0;

    waitpid(pid, &status, 0);

    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "cloexec-exec", "after exec the check reported %d (1: the FD_CLOEXEC descriptor survived, 2: its dup() was closed, 3: its F_DUPFD_CLOEXEC survived)",
          WIFEXITED(status) ? WEXITSTATUS(status) : -1);

    close(a);
    close(b);
    close(c);
}


/**
 * @brief A SIGUSR1 handler that does nothing, so the signal only interrupts a blocked call.
 *
 * @param sig The signal.
 */
static void signal_noop(int sig) {
    (void)sig;
}


/**
 * @brief Starts a child that sends SIGUSR1 to the caller after a delay, standing in for alarm(), which is ENOSYS.
 *
 * @param ms The delay in milliseconds.
 * @return The watchdog's pid, for watchdog_stop().
 */
static pid_t watchdog_start(int ms) {

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = signal_noop;

    sigaction(SIGUSR1, &sa, NULL);


    pid_t parent = getpid();
    pid_t pid    = fork();

    if (pid == 0) {

        usleep(ms * 1000);
        kill(parent, SIGUSR1);

        _exit(0);
    }

    return pid;
}


/**
 * @brief Stops a watchdog and restores the default action of SIGUSR1.
 *
 * @param pid The watchdog's pid.
 */
static void watchdog_stop(pid_t pid) {

    if (pid > 0) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
    }

    signal(SIGUSR1, SIG_DFL);
}


/**
 * @brief Reads one byte, giving up after a second instead of blocking forever.
 *
 * @param fd The descriptor.
 * @return What read() returned, with errno EINTR when it blocked.
 */
static ssize_t read_guarded(int fd) {

    pid_t w = watchdog_start(1000);

    char c;

    errno     = 0;
    ssize_t n = read(fd, &c, 1);
    int e     = errno;

    watchdog_stop(w);

    errno = e;

    return n;
}


/**
 * @brief Checks that O_NONBLOCK set on one descriptor is seen through dup(), dup2() and F_DUPFD copies, by F_GETFL and by read().
 */
static void test_shared_dup(void) {

    int p[2];

    if (pipe(p) < 0) {
        CHECK(0, "shared-dup", "pipe() failed: %s", strerror(errno));
        return;
    }

    int a = dup(p[0]);
    int b = dup2(p[0], 40);
    int c = fcntl(p[0], F_DUPFD, 50);

    if (a < 0 || b < 0 || c < 0) {
        CHECK(0, "shared-dup", "setup failed: %s", strerror(errno));
        return;
    }


    fcntl(p[0], F_SETFL, O_NONBLOCK);

    CHECK(fcntl(a, F_GETFL) & O_NONBLOCK, "shared-dup", "F_GETFL on the dup() reads 0x%x after O_NONBLOCK was set on the original", fcntl(a, F_GETFL));
    CHECK(fcntl(b, F_GETFL) & O_NONBLOCK, "shared-dup2", "F_GETFL on the dup2() reads 0x%x after O_NONBLOCK was set on the original", fcntl(b, F_GETFL));
    CHECK(fcntl(c, F_GETFL) & O_NONBLOCK, "shared-dupfd", "F_GETFL on the F_DUPFD copy reads 0x%x after O_NONBLOCK was set on the original", fcntl(c, F_GETFL));

    ssize_t n = read_guarded(a);

    CHECK(n < 0 && errno == EAGAIN, "shared-dup-read", "a read() on the empty pipe through the dup() returned %zd (%s), expected EAGAIN", n, strerror(errno));


    fcntl(b, F_SETFL, 0);

    CHECK(!(fcntl(p[0], F_GETFL) & O_NONBLOCK), "shared-dup-clear", "F_GETFL on the original reads 0x%x after O_NONBLOCK was cleared on the dup2()", fcntl(p[0], F_GETFL));

    close(a);
    close(b);
    close(c);
    close(p[0]);
    close(p[1]);


    int s[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, s) < 0) {
        CHECK(0, "shared-fionbio", "socketpair() failed: %s", strerror(errno));
        return;
    }

    int d  = dup(s[0]);
    int on = 1;

    ioctl(s[0], FIONBIO, &on);

    CHECK(fcntl(d, F_GETFL) & O_NONBLOCK, "shared-fionbio", "F_GETFL on the dup() reads 0x%x after FIONBIO on the original", fcntl(d, F_GETFL));

    n = read_guarded(d);

    CHECK(n < 0 && errno == EAGAIN, "shared-fionbio-read", "a read() on the idle socket through the dup() returned %zd (%s), expected EAGAIN", n, strerror(errno));

    close(d);
    close(s[0]);
    close(s[1]);
}


/**
 * @brief Checks that F_SETFL in a forked child is seen by the parent, and the other way round.
 */
static void test_shared_fork(void) {

    int p[2];
    int sync[2];

    if (pipe(p) < 0 || pipe(sync) < 0) {
        CHECK(0, "shared-fork", "pipe() failed: %s", strerror(errno));
        return;
    }


    pid_t pid = fork();

    if (pid == 0) {

        char c;

        if (read(sync[0], &c, 1) != 1)
            _exit(2);

        if (!(fcntl(p[0], F_GETFL) & O_NONBLOCK))
            _exit(1);

        fcntl(p[0], F_SETFL, 0);

        _exit(0);
    }


    fcntl(p[0], F_SETFL, O_NONBLOCK);
    write(sync[1], "x", 1);

    int status = 0;

    waitpid(pid, &status, 0);

    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "shared-fork-child", "the child reported %d (1: it did not see the O_NONBLOCK its parent set after fork)", WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    CHECK(!(fcntl(p[0], F_GETFL) & O_NONBLOCK), "shared-fork-parent", "F_GETFL in the parent reads 0x%x after the child cleared O_NONBLOCK", fcntl(p[0], F_GETFL));

    close(p[0]);
    close(p[1]);
    close(sync[0]);
    close(sync[1]);
}


/**
 * @brief Checks that F_GETFL reports only the access mode and the status flags, not the open-time or descriptor flags.
 */
static void test_getfl_clean(void) {

    char path[64];

    snprintf(path, sizeof(path), "/tmp/fd-test-getfl.%d", (int)getpid());

    int a = open(path, O_RDWR | O_CREAT | O_TRUNC | O_EXCL | O_CLOEXEC | O_APPEND, 0600);

    if (a < 0) {
        CHECK(0, "getfl-open", "open() failed: %s", strerror(errno));
        return;
    }

    int f = fcntl(a, F_GETFL);

    CHECK(f >= 0 && !(f & (O_CREAT | O_TRUNC | O_EXCL | O_CLOEXEC)), "getfl-open", "F_GETFL reads 0x%x, which has O_CREAT, O_TRUNC, O_EXCL or O_CLOEXEC", f);
    CHECK(f >= 0 && (f & O_ACCMODE) == O_RDWR && (f & O_APPEND), "getfl-open-status", "F_GETFL reads 0x%x, expected O_RDWR | O_APPEND", f);

    close(a);
    unlink(path);


    int p[2];

    if (pipe2(p, O_CLOEXEC | O_NONBLOCK) < 0) {
        CHECK(0, "getfl-pipe2", "pipe2() failed: %s", strerror(errno));
        return;
    }

    f = fcntl(p[1], F_GETFL);

    CHECK(f == (O_WRONLY | O_NONBLOCK), "getfl-pipe2", "F_GETFL on a pipe2(O_CLOEXEC | O_NONBLOCK) write end reads 0x%x, expected 0x%x", f, O_WRONLY | O_NONBLOCK);

    close(p[0]);
    close(p[1]);


    int s = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);

    if (s < 0) {
        CHECK(0, "getfl-socket", "socket() failed: %s", strerror(errno));
        return;
    }

    f = fcntl(s, F_GETFL);

    CHECK(f == (O_RDWR | O_NONBLOCK), "getfl-socket", "F_GETFL on a SOCK_CLOEXEC | SOCK_NONBLOCK socket reads 0x%x, expected 0x%x", f, O_RDWR | O_NONBLOCK);

    close(s);
}


/**
 * @brief Reads a whole file into a NUL-terminated buffer.
 *
 * @param path The file.
 * @param buf The buffer.
 * @param size Its size, including room for the NUL.
 * @return The number of bytes read, or -1 with errno set.
 */
static ssize_t read_file(const char* path, char* buf, size_t size) {

    int fd = open(path, O_RDONLY);

    if (fd < 0)
        return -1;

    ssize_t n = 0;
    ssize_t r = 0;

    while ((size_t)n < size - 1 && (r = read(fd, buf + n, size - 1 - n)) > 0)
        n += r;

    close(fd);

    buf[n] = '\0';

    return n;
}


/**
 * @brief Checks that writes and writev()s through two descriptors opened separately with O_APPEND all land at the end.
 *
 * @param dir The directory to put the file in, which picks the filesystem.
 * @param name The case name.
 */
static void check_append_separate(const char* dir, const char* name) {

    char path[64];
    char offname[64];

    snprintf(path, sizeof(path), "%s/fd-test-append.%d", dir, (int)getpid());
    snprintf(offname, sizeof(offname), "%s-offset", name);

    int a = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
    int b = open(path, O_WRONLY | O_APPEND);

    if (a < 0 || b < 0) {
        CHECK(0, name, "open() failed: %s", strerror(errno));
        return;
    }

    struct iovec iov[2] = {
        {(void*)"b1", 2},
        {(void*)"-", 1},
    };

    write(a, "a0-", 3);
    write(b, "b0-", 3);
    write(a, "a1-", 3);
    writev(b, iov, 2);
    write(a, "a2", 2);

    char buf[64];
    ssize_t n = read_file(path, buf, sizeof(buf));

    CHECK(n == 14 && strcmp(buf, "a0-b0-a1-b1-a2") == 0, name, "the file holds \"%s\" (%zd bytes), expected \"a0-b0-a1-b1-a2\"", n >= 0 ? buf : "", n);

    off_t off = lseek(a, 0, SEEK_CUR);

    CHECK(off == 14, offname, "the last writer is at offset %ld, expected the end of the file, 14", (long)off);

    close(a);
    close(b);
    unlink(path);
}


/**
 * @brief Checks O_APPEND through separately opened descriptors on tmpfs and on the ext2 root.
 */
static void test_append_separate(void) {

    check_append_separate("/tmp", "append-separate");
    check_append_separate("", "append-separate-ext2");
}


/**
 * @brief Checks that O_APPEND set with F_SETFL after open() sends the next write to the end, and that clearing it stops that.
 */
static void test_append_setfl(void) {

    char path[64];

    snprintf(path, sizeof(path), "/tmp/fd-test-setfl-append.%d", (int)getpid());

    int a = open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);

    if (a < 0) {
        CHECK(0, "append-setfl", "open() failed: %s", strerror(errno));
        return;
    }

    write(a, "hello", 5);
    lseek(a, 0, SEEK_SET);

    fcntl(a, F_SETFL, fcntl(a, F_GETFL) | O_APPEND);
    write(a, "XY", 2);

    char buf[64];
    ssize_t n = read_file(path, buf, sizeof(buf));

    CHECK(n == 7 && strcmp(buf, "helloXY") == 0, "append-setfl", "the file holds \"%s\" after F_SETFL O_APPEND and a write at offset 0, expected \"helloXY\"", n >= 0 ? buf : "");

    fcntl(a, F_SETFL, fcntl(a, F_GETFL) & ~O_APPEND);
    lseek(a, 0, SEEK_SET);
    write(a, "J", 1);

    n = read_file(path, buf, sizeof(buf));

    CHECK(n == 7 && strcmp(buf, "JelloXY") == 0, "append-setfl-clear", "the file holds \"%s\" after O_APPEND was cleared and a write at offset 0, expected \"JelloXY\"", n >= 0 ? buf : "");

    close(a);
    unlink(path);
}


/**
 * @brief Checks that open() with O_APPEND starts at offset 0, so a read sees the start of the file until the first write.
 */
static void test_append_open_offset(void) {

    char path[64];

    snprintf(path, sizeof(path), "/tmp/fd-test-open-append.%d", (int)getpid());

    int a = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (a < 0) {
        CHECK(0, "append-open-offset", "open() failed: %s", strerror(errno));
        return;
    }

    write(a, "hello", 5);
    close(a);

    int b = open(path, O_RDWR | O_APPEND);

    off_t off = lseek(b, 0, SEEK_CUR);

    char c    = 0;
    ssize_t n = read(b, &c, 1);

    CHECK(off == 0 && n == 1 && c == 'h', "append-open-offset", "a new O_APPEND descriptor is at offset %ld and read() returned %zd, expected offset 0 and the byte 'h'", (long)off, n);

    close(b);
    unlink(path);
}


/**
 * @brief Checks that pwrite() on an O_APPEND descriptor writes at the offset it is given and leaves the file position alone.
 */
static void test_append_pwrite(void) {

    char path[64];

    snprintf(path, sizeof(path), "/tmp/fd-test-pwrite-append.%d", (int)getpid());

    int a = open(path, O_RDWR | O_CREAT | O_TRUNC | O_APPEND, 0600);

    if (a < 0) {
        CHECK(0, "append-pwrite", "open() failed: %s", strerror(errno));
        return;
    }

    write(a, "hello", 5);

    ssize_t w = pwrite(a, "J", 1, 0);

    char buf[64];
    ssize_t n = read_file(path, buf, sizeof(buf));

    CHECK(w == 1 && n == 5 && strcmp(buf, "Jello") == 0, "append-pwrite", "pwrite() at offset 0 returned %zd and the file holds \"%s\", expected \"Jello\"", w, n >= 0 ? buf : "");

    off_t off = lseek(a, 0, SEEK_CUR);

    CHECK(off == 5, "append-pwrite-offset", "the descriptor is at offset %ld after pwrite(), expected 5", (long)off);

    close(a);
    unlink(path);
}


/**
 * @brief Checks that processes appending fixed-size records through their own O_APPEND descriptors at the same time lose none.
 */
static void test_append_concurrent(void) {

    enum { WRITERS = 3, RECORDS = 400, RECLEN = 8 };

    static char buf[WRITERS * RECORDS * RECLEN + 64];

    char path[64];

    snprintf(path, sizeof(path), "/tmp/fd-test-append-mp.%d", (int)getpid());

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd < 0) {
        CHECK(0, "append-concurrent", "open() failed: %s", strerror(errno));
        return;
    }

    close(fd);


    pid_t pids[WRITERS];

    for (int w = 0; w < WRITERS; w++) {

        if ((pids[w] = fork()) == 0) {

            int a = open(path, O_WRONLY | O_APPEND);

            if (a < 0)
                _exit(1);

            for (int i = 0; i < RECORDS; i++) {

                char rec[RECLEN + 1];

                snprintf(rec, sizeof(rec), "%c%06d\n", 'a' + w, i);

                if (write(a, rec, RECLEN) != RECLEN)
                    _exit(2);
            }

            _exit(0);
        }
    }


    int failed = 0;

    for (int w = 0; w < WRITERS; w++) {

        int status = 0;

        if (waitpid(pids[w], &status, 0) != pids[w] || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
            failed = w + 1;
    }

    CHECK(failed == 0, "append-concurrent-writers", "writer %d did not finish cleanly", failed);


    ssize_t n = read_file(path, buf, sizeof(buf));

    CHECK(n == WRITERS * RECORDS * RECLEN, "append-concurrent-size", "the file is %zd bytes, expected %d", n, WRITERS * RECORDS * RECLEN);


    int next[WRITERS] = {0};
    long bad          = -1;

    for (long k = 0; n > 0 && k + RECLEN <= n; k += RECLEN) {

        int w = buf[k] - 'a';

        if (w < 0 || w >= WRITERS || buf[k + RECLEN - 1] != '\n' || atoi(&buf[k + 1]) != next[w]) {
            bad = k;
            break;
        }

        next[w]++;
    }

    for (int w = 0; bad < 0 && w < WRITERS; w++) {
        if (next[w] != RECORDS)
            bad = n;
    }

    CHECK(bad < 0, "append-concurrent", "the records are lost, torn or out of order from byte %ld", bad);

    unlink(path);
}


/**
 * @brief The exec'd half of test_cloexec_exec().
 *
 * @param argv The three descriptors, as strings.
 * @return 0 if exactly the one expected survived, or which check failed.
 */
static int check_exec(char** argv) {

    int a = atoi(argv[0]);
    int b = atoi(argv[1]);
    int c = atoi(argv[2]);

    if (fcntl(a, F_GETFD) >= 0)
        return 1;

    if (fcntl(b, F_GETFD) < 0)
        return 2;

    if (fcntl(c, F_GETFD) >= 0)
        return 3;

    return 0;
}


static struct {

    const char* name;
    void (*fn)(void);

} cases[] = {
    {"dup-reuses-slot", test_dup_reuses_slot},
    {"dup2-cloexec", test_dup2_clears_cloexec},
    {"setfl-accmode", test_setfl_keeps_accmode},
    {"setfl-status", test_setfl_status},
    {"dupfd-min", test_dupfd_min},
    {"dup3", test_dup3},
    {"dup-offset", test_dup_shares_offset},
    {"cloexec-exec", test_cloexec_exec},
    {"shared-dup", test_shared_dup},
    {"shared-fork", test_shared_fork},
    {"getfl-clean", test_getfl_clean},
    {"append-separate", test_append_separate},
    {"append-setfl", test_append_setfl},
    {"append-open-offset", test_append_open_offset},
    {"append-pwrite", test_append_pwrite},
    {"append-concurrent", test_append_concurrent},
};


/**
 * @brief Runs every case, or the one named on the command line.
 *
 * @param argc The argument count.
 * @param argv The arguments; an optional case name, or --check-exec and three descriptors.
 * @return 0 when every case that ran passed.
 */
int main(int argc, char** argv) {

    if (argc == 5 && strcmp(argv[1], "--check-exec") == 0)
        return check_exec(&argv[2]);


    setvbuf(stdout, NULL, _IONBF, 0);

    printf("fd-test: starting\n");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {

        if (argc > 1 && strcmp(argv[1], cases[i].name) != 0)
            continue;

        cases[i].fn();
    }

    printf("fd-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
