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

/*
 * Regression tests for sockets being ordinary file descriptors.
 *
 * These used to live in a numbering space of their own, the lwIP index plus CONFIG_OPEN_MAX,
 * which meant a socket was not really a descriptor: dup(), dup2(), fstat() and everything else
 * that bounds-checks an fd against CONFIG_OPEN_MAX answered EBADF for a perfectly good socket.
 * The visible casualty was any server that moves an accepted connection onto stdin/stdout --
 * inetd-style servers and CGI do it as a matter of course, and busybox httpd died on it with
 * "can't duplicate file descriptor".
 *
 * So most of what is checked here is not socket behaviour at all. It is that a socket answers
 * the same generic descriptor calls as a pipe or a file, which is the whole point of the
 * exercise -- and in particular that dup2() onto a low, already-open descriptor works, since
 * that is the case the old layout could not express at all.
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <netinet/in.h>
#include <signal.h>
#include <sys/syscall.h>
#include <time.h>


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                          \
    {                                                                        \
        total++;                                                             \
        if (cond) {                                                          \
            printf("socket-test: PASS  %s\n", (name));                       \
        } else {                                                             \
            failures++;                                                      \
            printf("socket-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                    \
    }


#if CONFIG_HAVE_NETWORK


static int make_socket(void) {
    return socket(AF_INET, SOCK_STREAM, 0);
}


/*
 * The header claim: a socket comes out of the ordinary descriptor space. Anything at or above
 * CONFIG_OPEN_MAX is the old parallel numbering, and every generic fd call would reject it.
 */
static void test_low_fd(void) {

    int fd = make_socket();

    if (fd < 0) {
        CHECK(0, "low-fd", "socket() failed: %s", strerror(errno));
        return;
    }

    CHECK(fd < CONFIG_OPEN_MAX, "low-fd", "socket() returned fd %d, which is outside the descriptor table (CONFIG_OPEN_MAX is %d)", fd, CONFIG_OPEN_MAX);

    close(fd);
}


/*
 * fstat() is the cheapest proof that the fd resolves to something real, and a socket has to
 * report itself as one.
 */
static void test_fstat(void) {

    int fd = make_socket();

    if (fd < 0) {
        CHECK(0, "fstat", "socket() failed: %s", strerror(errno));
        return;
    }


    struct stat st;

    memset(&st, 0, sizeof(st));

    int r = fstat(fd, &st);

    CHECK(r == 0, "fstat", "fstat() on a socket returned %d (%s)", r, strerror(errno));
    CHECK(r == 0 && S_ISSOCK(st.st_mode), "fstat-mode", "st_mode is 0%o, which is not a socket", (unsigned)st.st_mode);

    close(fd);
}


/*
 * dup() has to produce a second, independent descriptor for the same socket; closing one must
 * leave the other usable, which is what the shared reference count is for.
 */
static void test_dup(void) {

    int fd = make_socket();

    if (fd < 0) {
        CHECK(0, "dup", "socket() failed: %s", strerror(errno));
        return;
    }


    int d = dup(fd);

    CHECK(d >= 0, "dup", "dup() on a socket returned %d (%s)", d, strerror(errno));

    if (d >= 0) {

        CHECK(d != fd, "dup-distinct", "dup() returned the same descriptor %d", d);

        /* Close the original; the duplicate must still resolve. */
        close(fd);

        struct stat st;
        int r = fstat(d, &st);

        CHECK(r == 0, "dup-survives-close", "the duplicate stopped working after the original was closed: %s", strerror(errno));

        close(d);

    } else {

        close(fd);
    }
}


/*
 * The case the old layout could not express, and the one that broke httpd: move a socket onto
 * a specific low descriptor that is already open.
 */
static void test_dup2_onto_stdio(void) {

    int fd = make_socket();

    if (fd < 0) {
        CHECK(0, "dup2", "socket() failed: %s", strerror(errno));
        return;
    }


    /* Keep a way back to the real stdout before standing on it. */
    int saved = dup(STDOUT_FILENO);

    if (saved < 0) {
        CHECK(0, "dup2", "could not save stdout: %s", strerror(errno));
        close(fd);
        return;
    }


    int r = dup2(fd, STDOUT_FILENO);

    /* Restore immediately: printf() below has to reach the console, not the socket. */
    int restored = dup2(saved, STDOUT_FILENO);

    close(saved);
    close(fd);

    CHECK(r == STDOUT_FILENO, "dup2", "dup2(socket, 1) returned %d (%s), expected 1", r, strerror(errno));
    CHECK(restored == STDOUT_FILENO, "dup2-restore", "%s", "could not put the real stdout back");
}


/*
 * The descriptor flags are the generic ones, not something the socket layer keeps to itself.
 */
static void test_fcntl_flags(void) {

    int fd = make_socket();

    if (fd < 0) {
        CHECK(0, "fcntl", "socket() failed: %s", strerror(errno));
        return;
    }


    int fl = fcntl(fd, F_GETFL);

    CHECK(fl >= 0, "fcntl-getfl", "F_GETFL on a socket returned %d (%s)", fl, strerror(errno));

    if (fl >= 0) {

        int r = fcntl(fd, F_SETFL, fl | O_NONBLOCK);

        CHECK(r >= 0, "fcntl-setfl", "F_SETFL O_NONBLOCK returned %d (%s)", r, strerror(errno));

        int back = fcntl(fd, F_GETFL);

        CHECK(back >= 0 && (back & O_NONBLOCK), "fcntl-nonblock-sticks", "F_GETFL came back 0x%x, without O_NONBLOCK", back);
    }


    /* close-on-exec lives on the descriptor, so it has to work here too. */
    CHECK(fcntl(fd, F_SETFD, FD_CLOEXEC) == 0, "fcntl-setfd", "F_SETFD failed: %s", strerror(errno));
    CHECK((fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0, "fcntl-getfd", "%s", "FD_CLOEXEC did not stick");

    close(fd);
}


/*
 * A socket has to be watchable next to descriptors that are not sockets -- the readiness core
 * reaches them by different routes, and a server sits in one select() over both.
 */
static void test_select_mixed(void) {

    int fd = make_socket();

    if (fd < 0) {
        CHECK(0, "select-mixed", "socket() failed: %s", strerror(errno));
        return;
    }

    int fds[2];

    if (pipe(fds) < 0) {
        CHECK(0, "select-mixed", "pipe() failed: %s", strerror(errno));
        close(fd);
        return;
    }

    write(fds[1], "x", 1);


    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    FD_SET(fds[0], &rfds);

    struct timeval tv = {0, 0};

    int n = (fd > fds[0] ? fd : fds[0]) + 1;
    int r = select(n, &rfds, NULL, NULL, &tv);

    CHECK(r >= 1, "select-mixed", "select() over a socket and a pipe returned %d (%s)", r, strerror(errno));
    CHECK(r >= 1 && FD_ISSET(fds[0], &rfds), "select-mixed-pipe", "%s", "the pipe with data was not reported");

    close(fds[0]);
    close(fds[1]);
    close(fd);
}


/*
 * close() has to release the socket through the same path as everything else, or a server that
 * runs for a while quietly runs out of them.
 */
static void test_close_releases(void) {

    int first = make_socket();

    if (first < 0) {
        CHECK(0, "close-releases", "socket() failed: %s", strerror(errno));
        return;
    }

    close(first);


    /* Open and close many more than the stack can hold at once. */
    int ok = 1;

    for (int i = 0; i < 64; i++) {

        int fd = make_socket();

        if (fd < 0) {
            ok = 0;
            break;
        }

        close(fd);
    }

    CHECK(ok, "close-releases", "%s", "ran out of sockets, so close() is not releasing them");
}


/*
 * A child has to inherit the socket across fork(), which is how every accept-and-fork server
 * hands the connection to its worker.
 */
static void test_fork_inherits(void) {

    int fd = make_socket();

    if (fd < 0) {
        CHECK(0, "fork-inherits", "socket() failed: %s", strerror(errno));
        return;
    }


    pid_t pid = fork();

    if (pid < 0) {
        CHECK(0, "fork-inherits", "fork() failed: %s", strerror(errno));
        close(fd);
        return;
    }

    if (pid == 0) {

        struct stat st;
        _exit(fstat(fd, &st) == 0 ? 0 : 1);
    }


    int status = 0;
    waitpid(pid, &status, 0);

    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "fork-inherits", "%s", "the child could not use the inherited socket");

    close(fd);
}


/*
 * Local sockets were always ordinary descriptors; they are checked here so that unifying the
 * lwIP ones cannot quietly take them apart.
 */
static void test_unix_still_works(void) {

    int sv[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        CHECK(0, "unix-socketpair", "socketpair() failed: %s", strerror(errno));
        return;
    }

    int d = dup(sv[0]);

    CHECK(d >= 0, "unix-dup", "dup() on a local socket returned %d (%s)", d, strerror(errno));

    if (d >= 0)
        close(d);

    close(sv[0]);
    close(sv[1]);
}


/*
 * Wait for a child, but not forever: a child that ignores the signal under test would
 * otherwise hang the whole run rather than fail it.
 */
static int wait_for_exit(pid_t pid, int* status, int seconds) {

    for (int i = 0; i < seconds * 10; i++) {

        if (waitpid(pid, status, WNOHANG) == pid)
            return 1;

        struct timespec ts = {.tv_sec = 0, .tv_nsec = 100000000L};
        nanosleep(&ts, NULL);
    }

    return 0;
}


/*
 * Park a child in accept() and leave it there; the caller signals it.
 */
static pid_t spawn_blocked_in_accept(int block_everything) {

    pid_t pid = fork();

    if (pid != 0)
        return pid;


    if (block_everything) {

        /* Straight to the kernel: the libc strips SIGKILL and SIGSTOP out of the set before
           the syscall, and whether the kernel refuses them too is exactly what is in
           question here. */
        sigset_t all;
        sigfillset(&all);

        syscall(SYS_rt_sigprocmask, SIG_SETMASK, &all, NULL, _NSIG / 8);
    }


    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0)
        _exit(98);


    struct sockaddr_in in;
    memset(&in, 0, sizeof(in));

    in.sin_family      = AF_INET;
    in.sin_port        = 0;
    in.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr*)&in, sizeof(in)) < 0)
        _exit(97);

    if (listen(fd, 1) < 0)
        _exit(96);


    struct sockaddr_in peer;
    socklen_t plen = sizeof(peer);

    /* Nothing will ever connect, so this only returns if the kernel makes it. */
    accept(fd, (struct sockaddr*)&peer, &plen);

    _exit(99);
}


/*
 * A task parked inside the network stack still has to be killable. accept() waits inside lwIP
 * rather than on an inode, so it reaches the scheduler by a different route than a sleeping
 * read() does, and a signal that never arrives leaves a process nothing can shift.
 */
static void test_signal_reaches_blocked_accept(void) {

    pid_t pid = spawn_blocked_in_accept(0);

    if (pid < 0) {
        CHECK(0, "signal-accept", "fork() failed: %s", strerror(errno));
        return;
    }


    sleep(2);
    kill(pid, SIGTERM);

    int status = 0;
    int reaped = wait_for_exit(pid, &status, 8);

    CHECK(reaped, "signal-accept", "%s", "a child blocked in accept() did not die on SIGTERM");

    if (reaped) {

        CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM, "signal-accept-status", "child left status 0x%x, expected death by SIGTERM", status);

    } else {

        kill(pid, SIGKILL);
    }
}


/*
 * SIGKILL cannot be blocked, whatever the mask says -- otherwise a process can make itself
 * permanently unkillable, which is the one thing the signal is for.
 */
static void test_sigkill_ignores_mask(void) {

    pid_t pid = spawn_blocked_in_accept(1);

    if (pid < 0) {
        CHECK(0, "sigkill-mask", "fork() failed: %s", strerror(errno));
        return;
    }


    sleep(2);
    kill(pid, SIGKILL);

    int status = 0;
    int reaped = wait_for_exit(pid, &status, 8);

    CHECK(reaped, "sigkill-mask", "%s", "a child that blocked every signal survived SIGKILL");

    if (!reaped) {

        /* Nothing else will shift it; leave the run able to finish. */
        kill(pid, SIGKILL);
        waitpid(pid, &status, WNOHANG);
    }
}


static struct {

    const char* name;
    void (*fn)(void);

} cases[] = {
    {"low-fd", test_low_fd},
    {"fstat", test_fstat},
    {"dup", test_dup},
    {"dup2", test_dup2_onto_stdio},
    {"fcntl", test_fcntl_flags},
    {"select-mixed", test_select_mixed},
    {"close-releases", test_close_releases},
    {"fork-inherits", test_fork_inherits},
    {"unix", test_unix_still_works},
    {"signal-accept", test_signal_reaches_blocked_accept},
    {"sigkill-mask", test_sigkill_ignores_mask},
};


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("socket-test: starting\n");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {

        if (argc > 1 && strcmp(argv[1], cases[i].name) != 0)
            continue;

        cases[i].fn();
    }

    printf("socket-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}

#else

int main(int argc, char** argv) {

    fprintf(stderr, "socket-test: network support is not enabled in this build\n");
    return 1;
}

#endif
