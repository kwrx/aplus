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
 * Tests for AF_UNIX SOCK_STREAM sockets.
 *
 * Every one of these returned ENOSYS or ENOTSOCK before local sockets existed: socket()
 * handed AF_UNIX straight to lwIP, which has no address family below AF_INET, and
 * socketpair() was a stub.
 *
 * The point of most of these cases is that a local socket is an ordinary file descriptor:
 * read, write, poll, dup, close-on-exec and fork inheritance all have to work on it without
 * anything socket-specific being involved.
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                       \
    {                                                                     \
        total++;                                                          \
        if (cond) {                                                       \
            printf("unix-test: PASS  %s\n", (name));                      \
        } else {                                                          \
            failures++;                                                   \
            printf("unix-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                 \
    }


static socklen_t fill_addr(struct sockaddr_un* un, const char* path) {

    memset(un, 0, sizeof(*un));

    un->sun_family = AF_UNIX;
    strncpy(un->sun_path, path, sizeof(un->sun_path) - 1);

    return (socklen_t)(sizeof(un->sun_family) + strlen(un->sun_path) + 1);
}


/* Data has to move in both directions over a socketpair. */
static void test_socketpair_echo(void) {

    int sv[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        CHECK(0, "socketpair", "socketpair() failed: %s", strerror(errno));
        return;
    }

    CHECK(1, "socketpair", "%s", "");


    char buf[32];

    ssize_t w = write(sv[0], "ping", 4);
    ssize_t r = read(sv[1], buf, sizeof(buf));

    CHECK(w == 4 && r == 4 && memcmp(buf, "ping", 4) == 0, "socketpair a->b", "wrote %zd, read %zd", w, r);

    w = write(sv[1], "pong", 4);
    r = read(sv[0], buf, sizeof(buf));

    CHECK(w == 4 && r == 4 && memcmp(buf, "pong", 4) == 0, "socketpair b->a", "wrote %zd, read %zd", w, r);

    close(sv[0]);
    close(sv[1]);
}


/* Closing one end is end of file on the other, and a broken pipe when written to. */
static void test_socketpair_peer_close(void) {

    int sv[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        CHECK(0, "socketpair peer close", "socketpair() failed: %s", strerror(errno));
        return;
    }

    if (write(sv[0], "last", 4) != 4) {
        CHECK(0, "socketpair peer close", "write() failed: %s", strerror(errno));
        return;
    }

    close(sv[0]);


    char buf[32];

    /* Data sent before the close still has to arrive ... */
    ssize_t first = read(sv[1], buf, sizeof(buf));

    CHECK(first == 4 && memcmp(buf, "last", 4) == 0, "socketpair drain after close", "read() returned %zd", first);

    /* ... and only then is it end of stream. */
    ssize_t empty = read(sv[1], buf, sizeof(buf));

    CHECK(empty == 0, "socketpair eof after close", "read() returned %zd, errno %d (%s)", empty, errno, strerror(errno));

    errno     = 0;
    ssize_t e = write(sv[1], "x", 1);

    CHECK(e < 0 && errno == EPIPE, "socketpair epipe after close", "write() returned %zd, errno %d (%s)", e, errno, strerror(errno));

    close(sv[1]);
}


/* A socketpair endpoint is a plain descriptor, so poll() has to work on it. */
static void test_socketpair_poll(void) {

    int sv[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        CHECK(0, "socketpair poll", "socketpair() failed: %s", strerror(errno));
        return;
    }

    struct pollfd pfd = {.fd = sv[1], .events = POLLIN, .revents = 0};

    /* Nothing sent yet. */
    int idle = poll(&pfd, 1, 0);

    CHECK(idle == 0, "socketpair poll idle", "poll() returned %d, revents 0x%x", idle, pfd.revents);

    write(sv[0], "y", 1);

    pfd.revents = 0;
    int ready   = poll(&pfd, 1, 2000);

    CHECK(ready == 1 && (pfd.revents & POLLIN), "socketpair poll readable", "poll() returned %d, revents 0x%x", ready, pfd.revents);

    close(sv[0]);
    close(sv[1]);
}


/* The full named-socket path: bind a filesystem entry, listen, connect, accept, talk. */
static void test_named_socket(void) {

    const char* path = "/tmp/unix-test.sock";

    unlink(path);

    int srv = socket(AF_UNIX, SOCK_STREAM, 0);

    if (srv < 0) {
        CHECK(0, "socket(AF_UNIX)", "socket() failed: %s", strerror(errno));
        return;
    }

    CHECK(1, "socket(AF_UNIX)", "%s", "");


    struct sockaddr_un un;
    socklen_t len = fill_addr(&un, path);

    if (bind(srv, (struct sockaddr*)&un, len) < 0) {
        CHECK(0, "bind", "bind() failed: %s", strerror(errno));
        close(srv);
        return;
    }

    CHECK(1, "bind", "%s", "");


    /* bind() has to leave a real socket node behind on the filesystem. */
    struct stat st;

    CHECK(stat(path, &st) == 0 && S_ISSOCK(st.st_mode), "bind creates S_IFSOCK node", "stat() failed or mode 0%o", stat(path, &st) == 0 ? (unsigned)st.st_mode : 0u);


    if (listen(srv, 8) < 0) {
        CHECK(0, "listen", "listen() failed: %s", strerror(errno));
        close(srv);
        return;
    }

    CHECK(1, "listen", "%s", "");


    /* Nothing has connected, so the listener must not be readable yet. */
    struct pollfd lp = {.fd = srv, .events = POLLIN, .revents = 0};

    CHECK(poll(&lp, 1, 0) == 0, "listener idle", "revents 0x%x", lp.revents);


    int cli = socket(AF_UNIX, SOCK_STREAM, 0);

    if (cli < 0 || connect(cli, (struct sockaddr*)&un, len) < 0) {
        CHECK(0, "connect", "connect() failed: %s", strerror(errno));
        close(srv);
        return;
    }

    CHECK(1, "connect", "%s", "");


    /* A pending connection is what makes a listening socket readable. */
    lp.revents = 0;

    CHECK(poll(&lp, 1, 2000) == 1 && (lp.revents & POLLIN), "listener poll pending", "revents 0x%x", lp.revents);


    int acc = accept(srv, NULL, NULL);

    if (acc < 0) {
        CHECK(0, "accept", "accept() failed: %s", strerror(errno));
        close(srv);
        close(cli);
        return;
    }

    CHECK(1, "accept", "%s", "");


    char buf[32];

    write(cli, "hello", 5);
    ssize_t r = read(acc, buf, sizeof(buf));

    CHECK(r == 5 && memcmp(buf, "hello", 5) == 0, "client to server", "read() returned %zd", r);

    write(acc, "world", 5);
    r = read(cli, buf, sizeof(buf));

    CHECK(r == 5 && memcmp(buf, "world", 5) == 0, "server to client", "read() returned %zd", r);


    close(cli);

    /* The server side has to see the client go away. */
    ssize_t eof = read(acc, buf, sizeof(buf));

    CHECK(eof == 0, "eof on client close", "read() returned %zd, errno %d", eof, errno);

    close(acc);
    close(srv);
    unlink(path);
}


/* Several clients in a row over one listener, to exercise the accept queue. */
static void test_multiple_clients(void) {

    const char* path = "/tmp/unix-test-multi.sock";

    unlink(path);

    int srv = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un un;
    socklen_t len = fill_addr(&un, path);

    if (srv < 0 || bind(srv, (struct sockaddr*)&un, len) < 0 || listen(srv, 8) < 0) {
        CHECK(0, "multiple clients", "setup failed: %s", strerror(errno));
        return;
    }


    int cli[3];
    int ok = 1;

    for (int i = 0; i < 3; i++) {

        cli[i] = socket(AF_UNIX, SOCK_STREAM, 0);

        if (cli[i] < 0 || connect(cli[i], (struct sockaddr*)&un, len) < 0)
            ok = 0;
    }

    CHECK(ok, "three clients connect", "errno %d (%s)", errno, strerror(errno));


    /* All three were queued before any was accepted. */
    ok = 1;

    for (int i = 0; i < 3; i++) {

        char c = (char)('a' + i);
        write(cli[i], &c, 1);

        int acc = accept(srv, NULL, NULL);

        if (acc < 0) {
            ok = 0;
            break;
        }

        char got = 0;

        if (read(acc, &got, 1) != 1 || got != c)
            ok = 0;

        close(acc);
    }

    CHECK(ok, "three clients accepted in order", "errno %d (%s)", errno, strerror(errno));

    for (int i = 0; i < 3; i++)
        close(cli[i]);

    close(srv);
    unlink(path);
}


/* Connecting to a path nobody is listening on is a refused connection, not a crash. */
static void test_connect_refused(void) {

    int cli = socket(AF_UNIX, SOCK_STREAM, 0);

    if (cli < 0) {
        CHECK(0, "connect refused", "socket() failed: %s", strerror(errno));
        return;
    }

    struct sockaddr_un un;
    socklen_t len = fill_addr(&un, "/tmp/unix-test-nothing-here.sock");

    errno = 0;
    int e = connect(cli, (struct sockaddr*)&un, len);

    CHECK(e < 0 && errno == ECONNREFUSED, "connect refused", "connect() returned %d, errno %d (%s)", e, errno, strerror(errno));

    close(cli);
}


/*
 * The payoff of making these ordinary descriptors: dup() and fork() inheritance work with no
 * socket-specific support, because the generic fd table already handles them.
 */
static void test_dup_and_fork(void) {

    int sv[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        CHECK(0, "dup and fork", "socketpair() failed: %s", strerror(errno));
        return;
    }

    int copy = dup(sv[0]);

    if (copy < 0) {
        CHECK(0, "dup of a socket", "dup() failed: %s", strerror(errno));
    } else {

        write(copy, "dup", 3);

        char buf[8] = {0};
        ssize_t r   = read(sv[1], buf, sizeof(buf));

        CHECK(r == 3 && memcmp(buf, "dup", 3) == 0, "dup of a socket", "read() returned %zd", r);

        close(copy);
    }


    pid_t pid = fork();

    if (pid == 0) {

        write(sv[0], "kid", 3);
        _exit(0);
    }

    char buf[8] = {0};
    ssize_t r   = read(sv[1], buf, sizeof(buf));

    int status = 0;
    waitpid(pid, &status, 0);

    CHECK(r == 3 && memcmp(buf, "kid", 3) == 0, "fork inherits a socket", "read() returned %zd", r);

    close(sv[0]);
    close(sv[1]);
}


/* shutdown(SHUT_WR) has to look like end of file to the peer. */
static void test_shutdown(void) {

    int sv[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        CHECK(0, "shutdown", "socketpair() failed: %s", strerror(errno));
        return;
    }

    if (shutdown(sv[0], SHUT_WR) < 0) {
        CHECK(0, "shutdown(SHUT_WR)", "shutdown() failed: %s", strerror(errno));
        close(sv[0]);
        close(sv[1]);
        return;
    }

    CHECK(1, "shutdown(SHUT_WR)", "%s", "");

    char buf[8];
    ssize_t r = read(sv[1], buf, sizeof(buf));

    CHECK(r == 0, "peer sees eof after shutdown", "read() returned %zd, errno %d", r, errno);

    errno     = 0;
    ssize_t w = write(sv[0], "x", 1);

    CHECK(w < 0 && errno == EPIPE, "write after shutdown is epipe", "write() returned %zd, errno %d (%s)", w, errno, strerror(errno));

    close(sv[0]);
    close(sv[1]);
}


static const struct {
    const char* name;
    void (*fn)(void);
} cases[] = {
    {"pair-echo", test_socketpair_echo},
    {"pair-close", test_socketpair_peer_close},
    {"pair-poll", test_socketpair_poll},
    {"named", test_named_socket},
    {"multi", test_multiple_clients},
    {"refused", test_connect_refused},
    {"dup-fork", test_dup_and_fork},
    {"shutdown", test_shutdown},
};


int main(int argc, char** argv) {

    /* Unbuffered: a case that hangs would otherwise take its own output down with it, and
       several of these defects hang rather than fail. */
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("unix-test: starting\n");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {

        if (argc > 1 && strcmp(argv[1], cases[i].name) != 0)
            continue;

        cases[i].fn();
    }

    printf("unix-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
