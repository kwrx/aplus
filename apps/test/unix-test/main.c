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
 * @brief Tests for AF_UNIX SOCK_STREAM sockets.
 *
 * Most cases check that a local socket is an ordinary descriptor, with nothing socket-specific involved.
 */

#include <errno.h>
#include <signal.h>
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


/**
 * @brief How much the kernel heap may grow over a leak check before it counts as a leak, in kB.
 */
#define LEAK_SLACK_KB 256


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


#define PREFIX "unix-test"

/**
 * @brief How many files churn_files() creates to get freed kernel memory handed out again.
 */
#define CHURN_FILES 32


/**
 * @brief The byte a churn file holds at a given offset.
 *
 * @param i Which churn file.
 * @param offset The offset into it.
 * @return The expected byte.
 */
static unsigned char churn_byte(int i, size_t offset) {
    return (unsigned char)((i * 29) + (offset * 7) + (offset >> 8) + 1);
}


/**
 * @brief Creates and fills a batch of files, so that pages the kernel has just freed are handed out again.
 */
static void churn_files(void) {

    unsigned char buf[4096];
    char path[64];

    for (int i = 0; i < CHURN_FILES; i++) {

        snprintf(path, sizeof(path), "/tmp/%s-churn-%d", PREFIX, i);

        for (size_t j = 0; j < sizeof(buf); j++)
            buf[j] = churn_byte(i, j);

        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (fd < 0)
            continue;

        if (write(fd, buf, sizeof(buf)) < 0)
            perror("churn write");

        close(fd);
    }
}


/**
 * @brief Checks the files churn_files() created and removes them.
 *
 * @return 1 if every file still held what was written to it.
 */
static int churn_intact(void) {

    unsigned char buf[4096];
    char path[64];
    int intact = 1;

    for (int i = 0; i < CHURN_FILES; i++) {

        snprintf(path, sizeof(path), "/tmp/%s-churn-%d", PREFIX, i);

        int fd = open(path, O_RDONLY);

        if (fd < 0 || read(fd, buf, sizeof(buf)) != (ssize_t)sizeof(buf)) {
            intact = 0;
        } else {

            for (size_t j = 0; j < sizeof(buf); j++) {

                if (buf[j] != churn_byte(i, j)) {
                    intact = 0;
                    break;
                }
            }
        }

        if (fd >= 0)
            close(fd);

        unlink(path);
    }

    return intact;
}


static socklen_t fill_addr(struct sockaddr_un* un, const char* path) {

    memset(un, 0, sizeof(*un));

    un->sun_family = AF_UNIX;
    strncpy(un->sun_path, path, sizeof(un->sun_path) - 1);

    return (socklen_t)(sizeof(un->sun_family) + strlen(un->sun_path) + 1);
}


/**
 * @brief Checks that data moves in both directions over a socketpair.
 */
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


/**
 * @brief Checks that closing one end is end of file on the other, and a broken pipe when written to.
 */
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

    ssize_t first = read(sv[1], buf, sizeof(buf));

    CHECK(first == 4 && memcmp(buf, "last", 4) == 0, "socketpair drain after close", "read() returned %zd", first);

    ssize_t empty = read(sv[1], buf, sizeof(buf));

    CHECK(empty == 0, "socketpair eof after close", "read() returned %zd, errno %d (%s)", empty, errno, strerror(errno));

    errno     = 0;
    ssize_t e = write(sv[1], "x", 1);

    CHECK(e < 0 && errno == EPIPE, "socketpair epipe after close", "write() returned %zd, errno %d (%s)", e, errno, strerror(errno));

    close(sv[1]);
}


/**
 * @brief Checks that poll() works on a socketpair endpoint.
 */
static void test_socketpair_poll(void) {

    int sv[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        CHECK(0, "socketpair poll", "socketpair() failed: %s", strerror(errno));
        return;
    }

    struct pollfd pfd = {.fd = sv[1], .events = POLLIN, .revents = 0};

    int idle = poll(&pfd, 1, 0);

    CHECK(idle == 0, "socketpair poll idle", "poll() returned %d, revents 0x%x", idle, pfd.revents);

    write(sv[0], "y", 1);

    pfd.revents = 0;
    int ready   = poll(&pfd, 1, 2000);

    CHECK(ready == 1 && (pfd.revents & POLLIN), "socketpair poll readable", "poll() returned %d, revents 0x%x", ready, pfd.revents);

    close(sv[0]);
    close(sv[1]);
}


/**
 * @brief Checks the full named-socket path: bind, listen, connect, accept, talk.
 */
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


    struct stat st;

    CHECK(stat(path, &st) == 0 && S_ISSOCK(st.st_mode), "bind creates S_IFSOCK node", "stat() failed or mode 0%o", stat(path, &st) == 0 ? (unsigned)st.st_mode : 0u);


    if (listen(srv, 8) < 0) {
        CHECK(0, "listen", "listen() failed: %s", strerror(errno));
        close(srv);
        return;
    }

    CHECK(1, "listen", "%s", "");


    struct pollfd lp = {.fd = srv, .events = POLLIN, .revents = 0};

    CHECK(poll(&lp, 1, 0) == 0, "listener idle", "revents 0x%x", lp.revents);


    int cli = socket(AF_UNIX, SOCK_STREAM, 0);

    if (cli < 0 || connect(cli, (struct sockaddr*)&un, len) < 0) {
        CHECK(0, "connect", "connect() failed: %s", strerror(errno));
        close(srv);
        return;
    }

    CHECK(1, "connect", "%s", "");


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

    ssize_t eof = read(acc, buf, sizeof(buf));

    CHECK(eof == 0, "eof on client close", "read() returned %zd, errno %d", eof, errno);

    close(acc);
    close(srv);
    unlink(path);
}


/**
 * @brief Checks several clients in a row over one listener, to exercise the accept queue.
 */
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


/**
 * @brief Checks that connecting to a path nobody is listening on is a refused connection.
 */
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


/**
 * @brief Checks that dup() and fork() inheritance work on a local socket with no socket-specific support.
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


/**
 * @brief Checks that shutdown(SHUT_WR) looks like end of file to the peer.
 */
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


/**
 * @brief Creates and closes sockets, pairs and connections, checking the kernel heap stays put.
 *
 * The last connection is left unaccepted when the listener closes, which is the path that used to strand both ends.
 */
static void test_no_leak(void) {

    const char* path = "/tmp/unix-test-leak.sock";

    long before = slab_kb();
    int made    = 0;

    for (int i = 0; i < 300; i++) {

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);

        if (fd < 0)
            break;

        close(fd);
        made++;
    }

    long after_sockets = slab_kb();

    CHECK(made == 300 && before >= 0 && after_sockets - before < LEAK_SLACK_KB, "leak-socket", "%d sockets made, kernel heap grew by %ld kB", made, after_sockets - before);


    made = 0;

    for (int i = 0; i < 300; i++) {

        int sv[2];

        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0)
            break;

        close(sv[0]);
        close(sv[1]);
        made++;
    }

    long after_pairs = slab_kb();

    CHECK(made == 300 && after_pairs - after_sockets < LEAK_SLACK_KB, "leak-socketpair", "%d pairs made, kernel heap grew by %ld kB", made, after_pairs - after_sockets);


    unlink(path);

    struct sockaddr_un un;
    socklen_t len = fill_addr(&un, path);

    int srv = socket(AF_UNIX, SOCK_STREAM, 0);

    if (srv < 0 || bind(srv, (struct sockaddr*)&un, len) < 0 || listen(srv, 8) < 0) {

        CHECK(0, "leak-connect", "listener setup failed: %s", strerror(errno));

        if (srv >= 0)
            close(srv);

        return;
    }

    made = 0;

    for (int i = 0; i < 100; i++) {

        int cli = socket(AF_UNIX, SOCK_STREAM, 0);

        if (cli < 0)
            break;

        if (connect(cli, (struct sockaddr*)&un, len) < 0) {
            close(cli);
            break;
        }

        int acc = accept(srv, NULL, NULL);

        if (acc >= 0) {
            close(acc);
            made++;
        }

        close(cli);
    }

    int stranded = socket(AF_UNIX, SOCK_STREAM, 0);

    if (stranded >= 0)
        connect(stranded, (struct sockaddr*)&un, len);

    close(srv);

    if (stranded >= 0)
        close(stranded);

    unlink(path);

    long after_connect = slab_kb();

    CHECK(made == 100 && after_connect - after_pairs < LEAK_SLACK_KB, "leak-connect", "%d connections accepted, kernel heap grew by %ld kB", made, after_connect - after_pairs);
}


/**
 * @brief How many connections the accept race makes.
 */
#define ACCEPT_RACE_ROUNDS 2000


static void alarm_noop(int sig) {
    (void)sig;
}


/**
 * @brief Connects to a listener over and over while it sits in a blocking accept(), looking for a lost wakeup.
 */
static void test_accept_race(void) {

    const char* path = "/tmp/unix-test-accept-race.sock";

    unlink(path);


    int srv = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un un;
    socklen_t len = fill_addr(&un, path);

    if (srv < 0 || bind(srv, (struct sockaddr*)&un, len) < 0 || listen(srv, 4) < 0) {

        CHECK(0, "accept-race", "listener setup failed: %s", strerror(errno));

        if (srv >= 0)
            close(srv);

        return;
    }


    pid_t pid = fork();

    if (pid == 0) {

        close(srv);

        for (int i = 0; i < ACCEPT_RACE_ROUNDS; i++) {

            int c = socket(AF_UNIX, SOCK_STREAM, 0);

            if (c < 0 || connect(c, (struct sockaddr*)&un, len) < 0)
                _exit(1);

            char ch;

            if (read(c, &ch, 1) != 1)
                _exit(2);

            close(c);
        }

        _exit(0);
    }


    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = alarm_noop;

    sigaction(SIGALRM, &sa, NULL);


    int rounds = 0;
    int stalls = 0;

    for (rounds = 0; rounds < ACCEPT_RACE_ROUNDS; rounds++) {

        alarm(5);

        int c = accept(srv, NULL, NULL);

        alarm(0);

        if (c < 0) {

            stalls += errno == EINTR;
            break;
        }

        write(c, "k", 1);
        close(c);
    }

    signal(SIGALRM, SIG_DFL);

    close(srv);
    unlink(path);


    int status = 0;

    if (stalls)
        kill(pid, SIGKILL);

    waitpid(pid, &status, 0);

    CHECK(rounds == ACCEPT_RACE_ROUNDS && stalls == 0, "accept-race", "%d of %d connections accepted, %d stalls, connector status 0x%x", rounds, ACCEPT_RACE_ROUNDS, stalls, status);
}


/**
 * @brief Creates sockets and socket pairs with SOCK_NONBLOCK and SOCK_CLOEXEC, and checks both flags took.
 */
static void test_sock_flags(void) {

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

    int fl = fd >= 0 ? fcntl(fd, F_GETFL) : -1;
    int fd_flags = fd >= 0 ? fcntl(fd, F_GETFD) : -1;

    CHECK(fd >= 0 && fl >= 0 && (fl & O_NONBLOCK) && fd_flags >= 0 && (fd_flags & FD_CLOEXEC), "sock-flags", "socket() returned %d (%s), F_GETFL 0x%x, F_GETFD 0x%x", fd, fd < 0 ? strerror(errno) : "ok", fl, fd_flags);

    if (fd >= 0)
        close(fd);


    int sv[2] = {-1, -1};
    int e     = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv);

    int f0 = e == 0 ? fcntl(sv[0], F_GETFD) : -1;
    int f1 = e == 0 ? fcntl(sv[1], F_GETFD) : -1;

    CHECK(e == 0 && f0 >= 0 && (f0 & FD_CLOEXEC) && f1 >= 0 && (f1 & FD_CLOEXEC), "sock-flags-pair", "socketpair() returned %d (%s), F_GETFD 0x%x 0x%x", e, e < 0 ? strerror(errno) : "ok", f0, f1);

    if (e == 0) {
        close(sv[0]);
        close(sv[1]);
    }
}


/**
 * @brief Keeps using a listening socket after its path is removed, while new files reuse freed kernel memory.
 */
static void test_bound_unlink(void) {

    const char* path = "/tmp/unix-test-bound.sock";

    unlink(path);

    int srv = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un un;
    socklen_t len = fill_addr(&un, path);

    if (srv < 0 || bind(srv, (struct sockaddr*)&un, len) < 0 || listen(srv, 4) < 0) {
        CHECK(0, "bound-unlink", "could not set up the listener: %s", strerror(errno));
        return;
    }


    struct sockaddr_un a0 = {0};
    struct sockaddr_un a1 = {0};

    socklen_t l0 = sizeof(a0);
    socklen_t l1 = sizeof(a1);

    int g0 = getsockname(srv, (struct sockaddr*)&a0, &l0);
    int e  = unlink(path);

    churn_files();

    int g1 = getsockname(srv, (struct sockaddr*)&a1, &l1);

    CHECK(g0 == 0 && e == 0 && g1 == 0 && strcmp(a0.sun_path, a1.sun_path) == 0, "bound-unlink-name", "getsockname gave \"%s\" before the unlink and \"%s\" after", a0.sun_path, a1.sun_path);


    errno = 0;

    int cli = socket(AF_UNIX, SOCK_STREAM, 0);
    int c   = cli >= 0 ? connect(cli, (struct sockaddr*)&un, len) : -1;

    CHECK(c < 0 && (errno == ENOENT || errno == ECONNREFUSED), "bound-unlink-connect", "connecting to the removed path returned %d, errno %d", c, errno);

    if (cli >= 0)
        close(cli);

    close(srv);

    CHECK(churn_intact(), "bound-unlink-close", "%s", "files written after the unlink were damaged when the listener closed");
}


/**
 * @brief Connects to a path over and over while another process keeps closing and rebinding the listener behind it.
 */
static void test_connect_close_race(void) {

    const char* path = "/tmp/unix-test-race.sock";

    struct sockaddr_un un;
    socklen_t len = fill_addr(&un, path);

    unlink(path);


    pid_t pid = fork();

    if (pid == 0) {

        for (int i = 0; i < 3000; i++) {

            int s = socket(AF_UNIX, SOCK_STREAM, 0);

            if (s < 0)
                _exit(1);

            (void)!connect(s, (struct sockaddr*)&un, len);
            close(s);
        }

        _exit(0);
    }


    int rounds = 0;

    for (int i = 0; pid > 0 && i < 300; i++) {

        unlink(path);

        int srv = socket(AF_UNIX, SOCK_STREAM, 0);

        if (srv < 0)
            break;

        if (fcntl(srv, F_SETFL, O_NONBLOCK) == 0 && bind(srv, (struct sockaddr*)&un, len) == 0 && listen(srv, 8) == 0) {

            for (int j = 0; j < 4; j++) {

                int a = accept(srv, NULL, NULL);

                if (a >= 0)
                    close(a);
            }

            rounds++;
        }

        close(srv);
    }

    unlink(path);


    int status = -1;

    if (pid > 0)
        waitpid(pid, &status, 0);

    CHECK(rounds > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0, "connect-close-race", "%d listener rounds, connecting child left status 0x%x", rounds, status);
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
    {"leak", test_no_leak},
    {"accept-race", test_accept_race},
    {"sock-flags", test_sock_flags},
    {"bound-unlink", test_bound_unlink},
    {"connect-close-race", test_connect_close_race},
};


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGPIPE, SIG_IGN);

    printf("unix-test: starting\n");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {

        if (argc > 1 && strcmp(argv[1], cases[i].name) != 0)
            continue;

        cases[i].fn();
    }

    printf("unix-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
