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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
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
