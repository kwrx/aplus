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
 * @brief Regression tests for sockets being ordinary file descriptors, and for how they block.
 *
 * Most of what is checked is that a socket answers the same generic descriptor calls as a pipe or a file. The last
 * cases can hang a kernel that sleeps inside lwIP, so each can also run on its own.
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

#include <dirent.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/uio.h>
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


static int make_socket(void) {
    return socket(AF_INET, SOCK_STREAM, 0);
}


/**
 * @brief Checks that a socket comes out of the ordinary descriptor space, below CONFIG_OPEN_MAX.
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


/**
 * @brief Checks that fstat() resolves the descriptor and reports it as a socket.
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


/**
 * @brief Checks that dup() produces a second descriptor for the same socket, and that closing one leaves the other.
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

        close(fd);

        struct stat st;
        int r = fstat(d, &st);

        CHECK(r == 0, "dup-survives-close", "the duplicate stopped working after the original was closed: %s", strerror(errno));

        close(d);

    } else {

        close(fd);
    }
}


/**
 * @brief Checks that a socket can be moved onto a specific low descriptor that is already open.
 */
static void test_dup2_onto_stdio(void) {

    int fd = make_socket();

    if (fd < 0) {
        CHECK(0, "dup2", "socket() failed: %s", strerror(errno));
        return;
    }


    int saved = dup(STDOUT_FILENO);

    if (saved < 0) {
        CHECK(0, "dup2", "could not save stdout: %s", strerror(errno));
        close(fd);
        return;
    }


    int r = dup2(fd, STDOUT_FILENO);

    int restored = dup2(saved, STDOUT_FILENO);

    close(saved);
    close(fd);

    CHECK(r == STDOUT_FILENO, "dup2", "dup2(socket, 1) returned %d (%s), expected 1", r, strerror(errno));
    CHECK(restored == STDOUT_FILENO, "dup2-restore", "%s", "could not put the real stdout back");
}


/**
 * @brief Checks that the descriptor flags are the generic ones rather than something the socket layer keeps.
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


    CHECK(fcntl(fd, F_SETFD, FD_CLOEXEC) == 0, "fcntl-setfd", "F_SETFD failed: %s", strerror(errno));
    CHECK((fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0, "fcntl-getfd", "%s", "FD_CLOEXEC did not stick");

    close(fd);
}


/**
 * @brief Checks that a socket can be watched next to descriptors that are not sockets.
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


/**
 * @brief Checks that close() releases a socket, which a server that runs for a while depends on.
 */
static void test_close_releases(void) {

    int first = make_socket();

    if (first < 0) {
        CHECK(0, "close-releases", "socket() failed: %s", strerror(errno));
        return;
    }

    close(first);


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


/**
 * @brief Checks that a child inherits a socket across fork(), as every accept-and-fork server needs.
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


/**
 * @brief Checks that local sockets still work, so that unifying the lwIP ones cannot take them apart.
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


/**
 * @brief Waits for a child, but not forever, so that one ignoring the signal under test fails rather than hangs.
 *
 * @param pid The child to wait for.
 * @param status Receives the child's exit status.
 * @param seconds How long to wait.
 * @return 1 when the child exited, 0 when the wait timed out.
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


/**
 * @brief Parks a child in accept() and leaves it there for the caller to signal.
 *
 * @param block_everything Whether the child blocks every signal it can first.
 * @return The child's pid, or -1.
 */
static pid_t spawn_blocked_in_accept(int block_everything) {

    pid_t pid = fork();

    if (pid != 0)
        return pid;


    if (block_everything) {

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

    accept(fd, (struct sockaddr*)&peer, &plen);

    _exit(99);
}


/**
 * @brief Checks that a task parked inside the network stack is still killable.
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


/**
 * @brief Checks that SIGKILL cannot be blocked, whatever the mask says.
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

        kill(pid, SIGKILL);
        waitpid(pid, &status, WNOHANG);
    }
}


/**
 * @brief How much the bulk transfer sends, far more than the send buffer and the receive window together.
 */
#define BULK_BYTES (2 * 1024 * 1024)


/**
 * @brief The byte the bulk transfer carries at a given offset.
 *
 * @param offset The offset into the stream.
 * @return The expected byte.
 */
static unsigned char bulk_pattern(size_t offset) {
    return (unsigned char)((offset * 7U) + (offset >> 11));
}


/**
 * @brief Streams patterned data over loopback TCP to a reader that stalls twice, checking every byte arrives.
 *
 * Each stall fills the sender's send buffer, so its blocking write has to wait for the stack more than once.
 */
static void test_bulk_tcp(void) {

    int srv = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in in;
    memset(&in, 0, sizeof(in));

    in.sin_family      = AF_INET;
    in.sin_port        = 0;
    in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    socklen_t len = sizeof(in);

    if (srv < 0 || bind(srv, (struct sockaddr*)&in, sizeof(in)) < 0 || listen(srv, 1) < 0 || getsockname(srv, (struct sockaddr*)&in, &len) < 0) {

        CHECK(0, "bulk-tcp", "listener setup failed: %s", strerror(errno));

        if (srv >= 0)
            close(srv);

        return;
    }


    pid_t pid = fork();

    if (pid == 0) {

        close(srv);

        int cli = socket(AF_INET, SOCK_STREAM, 0);

        if (cli < 0 || connect(cli, (struct sockaddr*)&in, sizeof(in)) < 0)
            _exit(90);

        unsigned char chunk[8192];
        size_t sent = 0;

        while (sent < BULK_BYTES) {

            size_t n = BULK_BYTES - sent < sizeof(chunk) ? BULK_BYTES - sent : sizeof(chunk);

            for (size_t i = 0; i < n; i++)
                chunk[i] = bulk_pattern(sent + i);

            ssize_t w = write(cli, chunk, n);

            if (w <= 0)
                _exit(91);

            sent += (size_t)w;
        }

        close(cli);
        _exit(0);
    }


    int acc = pid > 0 ? accept(srv, NULL, NULL) : -1;

    sleep(1);

    unsigned char buf[4096];

    size_t got   = 0;
    int mismatch = 0;
    int stalled  = 0;

    while (acc >= 0 && got < BULK_BYTES) {

        if (!stalled && got >= BULK_BYTES / 2) {
            sleep(1);
            stalled = 1;
        }

        ssize_t n = read(acc, buf, sizeof(buf));

        if (n <= 0)
            break;

        for (ssize_t i = 0; i < n; i++)
            mismatch += (buf[i] != bulk_pattern(got + (size_t)i));

        got += (size_t)n;
    }

    if (acc >= 0)
        close(acc);

    close(srv);


    int status = -1;
    int reaped = pid > 0 && wait_for_exit(pid, &status, 20);

    if (pid > 0 && !reaped)
        kill(pid, SIGKILL);

    CHECK(got == BULK_BYTES && mismatch == 0 && reaped && status == 0, "bulk-tcp", "received %zu of %d bytes, %d mismatched, sender status 0x%x", got, BULK_BYTES, mismatch, status);
}


/**
 * @brief Milliseconds on the monotonic clock.
 *
 * @return The current time.
 */
static uint64_t now_ms(void) {

    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
        return 0;

    return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}


/**
 * @brief Sleeps for a number of milliseconds.
 *
 * @param ms How long to sleep.
 */
static void sleep_ms(long ms) {

    struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};

    while (nanosleep(&ts, &ts) < 0 && errno == EINTR)
        ;
}


/**
 * @brief Opens a loopback TCP listener on a port the stack picks.
 *
 * @param addr Receives the address it listens on.
 * @return The listener, or -1.
 */
static int tcp_listener(struct sockaddr_in* addr) {

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0)
        return -1;


    memset(addr, 0, sizeof(*addr));

    addr->sin_family      = AF_INET;
    addr->sin_port        = 0;
    addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    socklen_t len = sizeof(*addr);

    if (bind(fd, (struct sockaddr*)addr, sizeof(*addr)) < 0 || listen(fd, 8) < 0 || getsockname(fd, (struct sockaddr*)addr, &len) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}


/**
 * @brief Makes a connected loopback TCP pair.
 *
 * @param cli Receives the connecting end.
 * @param acc Receives the accepted end.
 * @return 0, or -1.
 */
static int tcp_pair(int* cli, int* acc) {

    struct sockaddr_in in;

    int srv = tcp_listener(&in);

    if (srv < 0)
        return -1;


    *cli = socket(AF_INET, SOCK_STREAM, 0);

    if (*cli < 0 || connect(*cli, (struct sockaddr*)&in, sizeof(in)) < 0) {

        if (*cli >= 0)
            close(*cli);

        close(srv);
        return -1;
    }


    *acc = accept(srv, NULL, NULL);

    close(srv);

    if (*acc < 0) {
        close(*cli);
        return -1;
    }

    return 0;
}


/**
 * @brief A thread parked in one read or recv, and what it got.
 */
struct parked_reader {

    int fd;
    int use_recv;

    volatile int done;

    ssize_t n;
    int err;
    char c;
};


/**
 * @brief Body of a thread that reads one byte and records the outcome.
 *
 * @param arg The parked_reader to fill in.
 * @return NULL.
 */
static void* parked_reader_main(void* arg) {

    struct parked_reader* r = arg;

    r->n   = r->use_recv ? recv(r->fd, &r->c, 1, 0) : read(r->fd, &r->c, 1);
    r->err = errno;

    __atomic_store_n(&r->done, 1, __ATOMIC_SEQ_CST);

    return NULL;
}


/**
 * @brief Waits up to a number of milliseconds for a parked reader to finish.
 *
 * @param r The reader.
 * @param ms How long to wait.
 * @return 1 if it finished.
 */
static int parked_reader_wait(struct parked_reader* r, long ms) {

    for (long t = 0; t < ms && !__atomic_load_n(&r->done, __ATOMIC_SEQ_CST); t += 20)
        sleep_ms(20);

    return __atomic_load_n(&r->done, __ATOMIC_SEQ_CST);
}


/**
 * @brief Checks that a thread blocked reading a socket leaves the descriptor table usable by its siblings.
 */
static void test_read_vs_fdops(void) {

    int cli, acc;

    if (tcp_pair(&cli, &acc) < 0) {
        CHECK(0, "read-vs-fdops", "loopback pair failed: %s", strerror(errno));
        return;
    }


    struct parked_reader r = {.fd = cli};
    pthread_t t;

    pthread_create(&t, NULL, parked_reader_main, &r);

    sleep_ms(300);


    uint64_t t0 = now_ms();
    int ops     = 0;

    for (int i = 0; i < 50; i++) {

        int d = open("/dev/null", O_RDONLY);

        if (d >= 0) {
            close(d);
            ops++;
        }
    }

    int fl      = fcntl(cli, F_GETFL);
    uint64_t dt = now_ms() - t0;


    write(acc, "x", 1);

    int done = parked_reader_wait(&r, 5000);

    CHECK(ops == 50 && fl >= 0 && dt < 2000, "read-vs-fdops", "%d of 50 open/close pairs and F_GETFL %d took %lu ms beside a blocked reader", ops, fl, (unsigned long)dt);
    CHECK(done && r.n == 1 && r.c == 'x', "read-vs-fdops-data", "reader %s, returned %zd (%s)", done ? "finished" : "still blocked", r.n, strerror(r.err));

    if (done)
        pthread_join(t, NULL);

    close(cli);
    close(acc);
}


/**
 * @brief Checks that closing a socket another thread is blocked receiving on wakes that thread instead of hanging it.
 */
static void test_close_while_recv(void) {

    int cli, acc;

    if (tcp_pair(&cli, &acc) < 0) {
        CHECK(0, "close-while-recv", "loopback pair failed: %s", strerror(errno));
        return;
    }


    struct parked_reader r = {.fd = cli, .use_recv = 1};
    pthread_t t;

    pthread_create(&t, NULL, parked_reader_main, &r);

    sleep_ms(300);

    close(cli);


    int done = parked_reader_wait(&r, 3000);

    CHECK(done && r.n <= 0, "close-while-recv", "reader %s, returned %zd (%s)", done ? "finished" : "still blocked", r.n, strerror(r.err));

    if (done)
        pthread_join(t, NULL);

    close(acc);
}


/**
 * @brief Checks that closing a socket with SO_LINGER and unsent data returns at once and still delivers the data.
 */
static void test_linger_close(void) {

    int cli, acc;

    if (tcp_pair(&cli, &acc) < 0) {
        CHECK(0, "linger-close", "loopback pair failed: %s", strerror(errno));
        return;
    }


    struct linger lg = {.l_onoff = 1, .l_linger = 5};

    setsockopt(cli, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));


    struct timeval tv = {.tv_sec = 0, .tv_usec = 200000};

    setsockopt(cli, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));


    static unsigned char chunk[65536];
    size_t sent = 0;

    for (size_t i = 0; i < sizeof(chunk); i++)
        chunk[i] = (unsigned char)i;

    while (sent < 4 * 1024 * 1024) {

        ssize_t n = write(cli, chunk, sizeof(chunk));

        if (n > 0)
            sent += (size_t)n;

        if (n < (ssize_t)sizeof(chunk))
            break;
    }


    uint64_t t0 = now_ms();

    int r       = close(cli);
    uint64_t dt = now_ms() - t0;


    size_t got = 0;

    for (;;) {

        struct pollfd pfd = {.fd = acc, .events = POLLIN};

        if (poll(&pfd, 1, 5000) <= 0)
            break;

        ssize_t n = read(acc, chunk, sizeof(chunk));

        if (n <= 0)
            break;

        got += (size_t)n;
    }

    close(acc);

    CHECK(r == 0 && dt < 1000, "linger-close", "close() returned %d after %lu ms", r, (unsigned long)dt);
    CHECK(sent > 0 && got == sent, "linger-close-data", "peer received %zu of the %zu bytes queued before close()", got, sent);
}


static void usr1_noop(int sig) {
    (void)sig;
}


/**
 * @brief Checks that a handler interrupting a blocking recv() returns into a working recv(), with or without SA_RESTART.
 *
 * @param restart Whether the handler is installed with SA_RESTART.
 * @param name The name to report under.
 */
static void handler_recv_round(int restart, const char* name) {

    int cli, acc;

    if (tcp_pair(&cli, &acc) < 0) {
        CHECK(0, name, "loopback pair failed: %s", strerror(errno));
        return;
    }


    pid_t pid = fork();

    if (pid == 0) {

        close(acc);


        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));

        sa.sa_handler = usr1_noop;
        sa.sa_flags   = restart ? SA_RESTART : 0;

        sigaction(SIGUSR1, &sa, NULL);


        char c    = 0;
        ssize_t n = 0;
        int eintr = 0;

        for (;;) {

            n = recv(cli, &c, 1, 0);

            if (n < 0 && errno == EINTR) {
                eintr++;
                continue;
            }

            break;
        }

        if (n != 1 || c != 'y')
            _exit(1);

        _exit(eintr ? 10 : 0);
    }

    close(cli);


    sleep_ms(300);
    kill(pid, SIGUSR1);
    sleep_ms(300);

    send(acc, "y", 1, 0);


    int status = 0;
    int reaped = wait_for_exit(pid, &status, 5);

    if (!reaped)
        kill(pid, SIGKILL), waitpid(pid, &status, 0);

    int want = restart ? 0 : 10;

    CHECK(reaped && WIFEXITED(status) && WEXITSTATUS(status) == want, name, "child %s with status 0x%x, expected exit %d", reaped ? "finished" : "hung", status, want);

    close(acc);
}


static void test_handler_recv(void) {

    handler_recv_round(1, "handler-recv-restart");
    handler_recv_round(0, "handler-recv-eintr");
}


/**
 * @brief Checks that two processes blocked in accept() on one listener each get a connection.
 */
static void test_accept_two(void) {

    struct sockaddr_in in;

    int srv = tcp_listener(&in);

    if (srv < 0) {
        CHECK(0, "accept-two", "listener setup failed: %s", strerror(errno));
        return;
    }


    pid_t pids[2];

    for (int i = 0; i < 2; i++) {

        if ((pids[i] = fork()) == 0) {

            int c = accept(srv, NULL, NULL);

            if (c < 0)
                _exit(1);

            write(c, "a", 1);
            close(c);

            _exit(0);
        }
    }

    close(srv);

    sleep_ms(300);


    int got = 0;

    for (int i = 0; i < 2; i++) {

        int c = socket(AF_INET, SOCK_STREAM, 0);

        if (c < 0 || connect(c, (struct sockaddr*)&in, sizeof(in)) < 0) {

            if (c >= 0)
                close(c);

            continue;
        }

        struct pollfd pfd = {.fd = c, .events = POLLIN};
        char ch           = 0;

        if (poll(&pfd, 1, 5000) == 1 && read(c, &ch, 1) == 1 && ch == 'a')
            got++;

        close(c);
    }


    int ok = 0;

    for (int i = 0; i < 2; i++) {

        int status = 0;

        if (wait_for_exit(pids[i], &status, 5)) {

            ok += WIFEXITED(status) && WEXITSTATUS(status) == 0;

        } else {

            kill(pids[i], SIGKILL);
            waitpid(pids[i], &status, 0);
        }
    }

    CHECK(got == 2 && ok == 2, "accept-two", "%d of 2 connections answered, %d of 2 acceptors exited cleanly", got, ok);
}


/**
 * @brief Checks the non-blocking connect() protocol: EINPROGRESS, then POLLOUT, then SO_ERROR, then EISCONN.
 */
static void test_connect_nonblock(void) {

    struct sockaddr_in in;

    int srv = tcp_listener(&in);

    if (srv < 0) {
        CHECK(0, "connect-nonblock", "listener setup failed: %s", strerror(errno));
        return;
    }


    int c = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    int r   = c >= 0 ? connect(c, (struct sockaddr*)&in, sizeof(in)) : -1;
    int err = errno;

    CHECK(c >= 0 && (r == 0 || err == EINPROGRESS), "connect-nonblock", "connect() returned %d (%s)", r, strerror(err));


    struct pollfd pfd = {.fd = c, .events = POLLOUT};

    int p = poll(&pfd, 1, 3000);

    int soerr     = -1;
    socklen_t len = sizeof(soerr);

    getsockopt(c, SOL_SOCKET, SO_ERROR, &soerr, &len);

    CHECK(p == 1 && (pfd.revents & POLLOUT) && soerr == 0, "connect-nonblock-done", "poll() %d revents 0x%x, SO_ERROR %d", p, pfd.revents, soerr);


    r   = connect(c, (struct sockaddr*)&in, sizeof(in));
    err = errno;

    CHECK(r < 0 && err == EISCONN, "connect-nonblock-again", "second connect() returned %d (%s)", r, strerror(err));

    close(c);
    close(srv);
}


/**
 * @brief Checks that connecting to a port nobody listens on fails with ECONNREFUSED, blocking or not.
 */
static void test_connect_refused(void) {

    struct sockaddr_in in;

    int srv = tcp_listener(&in);

    if (srv < 0) {
        CHECK(0, "connect-refused", "listener setup failed: %s", strerror(errno));
        return;
    }

    close(srv);


    int c = socket(AF_INET, SOCK_STREAM, 0);

    uint64_t t0 = now_ms();
    int r       = connect(c, (struct sockaddr*)&in, sizeof(in));
    int err     = errno;

    CHECK(r < 0 && err == ECONNREFUSED && now_ms() - t0 < 5000, "connect-refused", "blocking connect() returned %d (%s)", r, strerror(err));

    close(c);


    c = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    r   = connect(c, (struct sockaddr*)&in, sizeof(in));
    err = errno;

    struct pollfd pfd = {.fd = c, .events = POLLOUT};

    int p = (r < 0 && err == EINPROGRESS) ? poll(&pfd, 1, 3000) : 0;

    int soerr     = 0;
    socklen_t len = sizeof(soerr);

    getsockopt(c, SOL_SOCKET, SO_ERROR, &soerr, &len);

    CHECK(p == 1 && soerr == ECONNREFUSED, "connect-refused-nonblock", "connect() %d (%s), poll() %d, SO_ERROR %d", r, strerror(err), p, soerr);

    close(c);
}


/**
 * @brief Checks every way of asking for a non-blocking receive: O_NONBLOCK, MSG_DONTWAIT and FIONBIO.
 */
static void test_nonblock_recv(void) {

    int cli, acc;

    if (tcp_pair(&cli, &acc) < 0) {
        CHECK(0, "nonblock-recv", "loopback pair failed: %s", strerror(errno));
        return;
    }


    pid_t pid = fork();

    if (pid == 0) {

        int bad = 0;
        char c;

        fcntl(cli, F_SETFL, O_NONBLOCK);

        if (!(recv(cli, &c, 1, 0) < 0 && errno == EAGAIN))
            bad |= 1;

        fcntl(cli, F_SETFL, 0);

        if (!(recv(cli, &c, 1, MSG_DONTWAIT) < 0 && errno == EAGAIN))
            bad |= 2;


        int on = 1;
        ioctl(cli, FIONBIO, &on);

        if (!(read(cli, &c, 1) < 0 && errno == EAGAIN))
            bad |= 4;

        on = 0;
        ioctl(cli, FIONBIO, &on);

        _exit(bad);
    }


    int status = 0;
    int reaped = wait_for_exit(pid, &status, 5);

    if (!reaped)
        kill(pid, SIGKILL), waitpid(pid, &status, 0);

    CHECK(reaped && WIFEXITED(status) && WEXITSTATUS(status) == 0, "nonblock-recv", "child %s with status 0x%x (1: O_NONBLOCK, 2: MSG_DONTWAIT, 4: FIONBIO did not give EAGAIN)", reaped ? "finished" : "blocked", status);

    close(cli);
    close(acc);
}


/**
 * @brief Checks that SO_RCVTIMEO bounds a blocking receive.
 */
static void test_rcvtimeo(void) {

    int cli, acc;

    if (tcp_pair(&cli, &acc) < 0) {
        CHECK(0, "rcvtimeo", "loopback pair failed: %s", strerror(errno));
        return;
    }


    struct timeval tv = {.tv_sec = 0, .tv_usec = 300000};

    setsockopt(cli, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));


    char c;

    uint64_t t0 = now_ms();
    ssize_t n   = recv(cli, &c, 1, 0);
    int err     = errno;
    uint64_t dt = now_ms() - t0;

    CHECK(n < 0 && (err == EAGAIN || err == EWOULDBLOCK) && dt >= 250 && dt < 3000, "rcvtimeo", "recv() returned %zd (%s) after %lu ms", n, strerror(err), (unsigned long)dt);

    close(cli);
    close(acc);
}


/**
 * @brief How much one blocking send() and one writev() hand over, far more than the send buffer holds.
 */
#define WHOLE_SEND  (1024 * 1024)
#define WHOLE_IOV   (200 * 1024)
#define WHOLE_TOTAL (WHOLE_SEND + 3 * WHOLE_IOV)


/**
 * @brief Checks that a blocking send() and writev() on a stream hand over everything in one call, exactly once.
 */
static void test_send_whole(void) {

    int cli, acc;

    if (tcp_pair(&cli, &acc) < 0) {
        CHECK(0, "send-whole", "loopback pair failed: %s", strerror(errno));
        return;
    }


    pid_t pid = fork();

    if (pid == 0) {

        close(cli);

        sleep_ms(500);


        unsigned char buf[4096];

        size_t got   = 0;
        int mismatch = 0;

        for (;;) {

            ssize_t n = read(acc, buf, sizeof(buf));

            if (n <= 0)
                break;

            for (ssize_t i = 0; i < n; i++)
                mismatch += (buf[i] != bulk_pattern(got + (size_t)i));

            got += (size_t)n;
        }

        _exit(got == WHOLE_TOTAL && mismatch == 0 ? 0 : 1);
    }

    close(acc);


    static unsigned char data[WHOLE_TOTAL];

    for (size_t i = 0; i < sizeof(data); i++)
        data[i] = bulk_pattern(i);


    ssize_t s = send(cli, data, WHOLE_SEND, 0);

    struct iovec iov[3] = {
        {.iov_base = &data[WHOLE_SEND],                 .iov_len = WHOLE_IOV},
        {.iov_base = &data[WHOLE_SEND + WHOLE_IOV],     .iov_len = WHOLE_IOV},
        {.iov_base = &data[WHOLE_SEND + 2 * WHOLE_IOV], .iov_len = WHOLE_IOV},
    };

    ssize_t w = writev(cli, iov, 3);

    close(cli);


    int status = 0;
    int reaped = wait_for_exit(pid, &status, 20);

    if (!reaped)
        kill(pid, SIGKILL), waitpid(pid, &status, 0);

    CHECK(s == WHOLE_SEND && w == 3 * WHOLE_IOV, "send-whole", "send() returned %zd of %d, writev() %zd of %d", s, WHOLE_SEND, w, 3 * WHOLE_IOV);
    CHECK(reaped && WIFEXITED(status) && WEXITSTATUS(status) == 0, "send-whole-data", "receiver %s with status 0x%x", reaped ? "finished" : "hung", status);
}


/**
 * @brief How many one-byte round trips the ping-pong makes.
 */
#define PINGPONG_ROUNDS 5000


/**
 * @brief Bounces a byte back and forth over loopback TCP, looking for a stall from a lost wakeup.
 */
static void test_pingpong(void) {

    int cli, acc;

    if (tcp_pair(&cli, &acc) < 0) {
        CHECK(0, "pingpong", "loopback pair failed: %s", strerror(errno));
        return;
    }


    pid_t pid = fork();

    if (pid == 0) {

        close(cli);

        for (int i = 0; i < PINGPONG_ROUNDS; i++) {

            char c;

            if (read(acc, &c, 1) != 1 || write(acc, &c, 1) != 1)
                _exit(1);
        }

        _exit(0);
    }

    close(acc);


    int stalls  = 0;
    int rounds  = 0;
    uint64_t t0 = now_ms();

    for (rounds = 0; rounds < PINGPONG_ROUNDS; rounds++) {

        char c = (char)rounds;

        if (write(cli, &c, 1) != 1)
            break;

        struct pollfd pfd = {.fd = cli, .events = POLLIN};

        if (poll(&pfd, 1, 3000) != 1) {
            stalls++;
            break;
        }

        char r;

        if (read(cli, &r, 1) != 1 || r != c)
            break;
    }

    uint64_t dt = now_ms() - t0;

    close(cli);


    int status = 0;
    int reaped = wait_for_exit(pid, &status, 10);

    if (!reaped)
        kill(pid, SIGKILL), waitpid(pid, &status, 0);

    CHECK(rounds == PINGPONG_ROUNDS && stalls == 0 && reaped && status == 0, "pingpong", "%d of %d rounds in %lu ms, %d stalls, echo status 0x%x", rounds, PINGPONG_ROUNDS, (unsigned long)dt, stalls, status);
}


/**
 * @brief Lists /proc/<pid>/fd and resolves every entry, as ls -l does.
 *
 * @param pid The process to list.
 * @return The number of entries, or -1.
 */
static int list_proc_fds(pid_t pid) {

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/fd", (int)pid);

    DIR* d = opendir(path);

    if (!d)
        return -1;


    int n = 0;
    struct dirent* e;

    while ((e = readdir(d)) != NULL) {

        if (e->d_name[0] == '.')
            continue;

        char link[352];
        char target[256];

        snprintf(link, sizeof(link), "%s/%s", path, e->d_name);
        readlink(link, target, sizeof(target));

        n++;
    }

    closedir(d);

    return n;
}


/**
 * @brief Checks that /proc/<pid>/fd of a process blocked reading a socket can be listed.
 */
static void test_procfs_fd_blocked(void) {

    int cli, acc;

    if (tcp_pair(&cli, &acc) < 0) {
        CHECK(0, "procfs-fd-blocked", "loopback pair failed: %s", strerror(errno));
        return;
    }


    pid_t pid = fork();

    if (pid == 0) {

        close(acc);

        char c = 0;

        _exit(read(cli, &c, 1) == 1 && c == 'p' ? 0 : 1);
    }

    close(cli);

    sleep_ms(300);


    int listed  = 0;
    uint64_t t0 = now_ms();

    for (int i = 0; i < 200; i++)
        listed += list_proc_fds(pid) > 0;

    uint64_t dt = now_ms() - t0;


    write(acc, "p", 1);

    int status = 0;
    int reaped = wait_for_exit(pid, &status, 5);

    if (!reaped)
        kill(pid, SIGKILL), waitpid(pid, &status, 0);

    CHECK(listed == 200, "procfs-fd-blocked", "listed /proc/%d/fd %d of 200 times in %lu ms", (int)pid, listed, (unsigned long)dt);
    CHECK(reaped && status == 0, "procfs-fd-blocked-child", "child %s with status 0x%x", reaped ? "finished" : "hung", status);

    close(acc);
}


/**
 * @brief Lists another process's /proc/<pid>/fd a number of times, then waits to be released before exiting.
 *
 * @param peer The process to list.
 * @param done Written once the listing is done.
 * @param release Read before exiting.
 */
static void procfs_cross_lister(pid_t peer, int done, int release) {

    int err = 0;

    for (int i = 0; i < 300 && !err; i++) {

        if (list_proc_fds(peer) < 0)
            err = errno ? errno : 127;
    }

    char c = 'd';

    write(done, &c, 1);
    read(release, &c, 1);

    _exit(err);
}


/**
 * @brief Checks that two processes listing each other's /proc/<pid>/fd at the same time do not deadlock.
 */
static void test_procfs_cross(void) {

    int p[2], d[2], r[2];

    if (pipe(p) < 0 || pipe(d) < 0 || pipe(r) < 0) {
        CHECK(0, "procfs-cross", "pipe() failed: %s", strerror(errno));
        return;
    }


    pid_t a = fork();

    if (a == 0) {

        close(p[1]);

        pid_t peer = 0;

        if (read(p[0], &peer, sizeof(peer)) != sizeof(peer))
            _exit(2);

        procfs_cross_lister(peer, d[1], r[0]);
    }


    pid_t b = fork();

    if (b == 0)
        procfs_cross_lister(a, d[1], r[0]);


    close(p[0]);

    write(p[1], &b, sizeof(b));
    close(p[1]);


    int finished = 0;

    while (finished < 2) {

        struct pollfd pfd = {.fd = d[0], .events = POLLIN};
        char c;

        if (poll(&pfd, 1, 30000) != 1 || read(d[0], &c, 1) != 1)
            break;

        finished++;
    }

    write(r[1], "rr", 2);


    int sa = 0, sb = 0;

    int ra = wait_for_exit(a, &sa, 10);
    int rb = wait_for_exit(b, &sb, 10);

    if (!ra)
        kill(a, SIGKILL), waitpid(a, &sa, 0);

    if (!rb)
        kill(b, SIGKILL), waitpid(b, &sb, 0);

    close(d[0]);
    close(d[1]);
    close(r[0]);
    close(r[1]);

    CHECK(finished == 2 && sa == 0 && sb == 0, "procfs-cross", "%d of 2 listers finished, errno %d/%d", finished, WEXITSTATUS(sa), WEXITSTATUS(sb));
}


/**
 * @brief Set by the thread churning descriptors once it is done.
 */
static volatile int churn_stop = 0;


/**
 * @brief Body of a thread that opens and closes descriptors until told to stop.
 *
 * @param arg Unused.
 * @return NULL.
 */
static void* churn_main(void* arg) {

    (void)arg;

    while (!churn_stop) {

        int d = open("/dev/null", O_RDONLY);

        if (d >= 0)
            close(d);
    }

    return NULL;
}


/**
 * @brief Checks that fork() beside a thread closing descriptors hands the child only descriptors that work.
 */
static void test_fork_vs_close(void) {

    pthread_t t;

    churn_stop = 0;

    pthread_create(&t, NULL, churn_main, NULL);


    int bad = 0;

    for (int i = 0; i < 200; i++) {

        pid_t pid = fork();

        if (pid == 0) {

            int broken = 0;

            for (int fd = 0; fd < 64; fd++) {

                if (fcntl(fd, F_GETFD) < 0)
                    continue;

                struct stat st;

                if (fstat(fd, &st) < 0)
                    broken++;
            }

            _exit(broken ? 1 : 0);
        }


        int status = 0;

        if (pid < 0 || waitpid(pid, &status, 0) != pid || status != 0)
            bad++;
    }

    churn_stop = 1;
    pthread_join(t, NULL);

    CHECK(bad == 0, "fork-vs-close", "%d of 200 children saw a broken descriptor or failed", bad);
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
    {"bulk-tcp", test_bulk_tcp},
    {"accept-two", test_accept_two},
    {"connect-nonblock", test_connect_nonblock},
    {"connect-refused", test_connect_refused},
    {"nonblock-recv", test_nonblock_recv},
    {"rcvtimeo", test_rcvtimeo},
    {"send-whole", test_send_whole},
    {"pingpong", test_pingpong},
    {"procfs-cross", test_procfs_cross},
    {"fork-vs-close", test_fork_vs_close},
    {"read-vs-fdops", test_read_vs_fdops},
    {"close-while-recv", test_close_while_recv},
    {"linger-close", test_linger_close},
    {"handler-recv", test_handler_recv},
    {"procfs-fd-blocked", test_procfs_fd_blocked},
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
