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
 * @brief Regression tests for how tasks exit, stop, continue and are reaped, and for the signals that go with it.
 *
 * Each case can be run on its own by name, since several of them used to take the kernel down.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


#define LEAK_SLACK_KB 512


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                             \
    {                                                                           \
        total++;                                                                \
        if (cond) {                                                             \
            printf("lifecycle-test: PASS  %s\n", (name));                       \
        } else {                                                                \
            failures++;                                                         \
            printf("lifecycle-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                       \
    }


/**
 * @brief Reads the monotonic clock.
 *
 * @return Milliseconds since an arbitrary point.
 */
static uint64_t now_ms(void) {

    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
        return 0;

    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}


/**
 * @brief Sleeps for at least the given time, carrying on across signal handlers.
 *
 * @param ms How long to sleep.
 */
static void sleep_ms(unsigned ms) {

    uint64_t end = now_ms() + ms;

    for (uint64_t now = now_ms(); now < end; now = now_ms()) {

        struct timespec ts = {.tv_sec = (end - now) / 1000, .tv_nsec = (long)((end - now) % 1000) * 1000000L};

        nanosleep(&ts, NULL);
    }
}


/**
 * @brief Reads how much memory the kernel heap holds, from /proc/meminfo.
 *
 * @return The Slab figure in kB, or -1 if it cannot be read.
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
 * @brief Polls a child with WNOHANG until it is reported or time runs out.
 *
 * @param pid The child to wait for.
 * @param status Receives its status.
 * @param options Extra waitpid() options.
 * @param ms How long to keep trying.
 * @return What waitpid() returned, or 0 on timeout.
 */
static pid_t wait_timeout(pid_t pid, int* status, int options, unsigned ms) {

    uint64_t end = now_ms() + ms;

    for (;;) {

        pid_t r = waitpid(pid, status, options | WNOHANG);

        if (r != 0)
            return r;

        if (now_ms() >= end)
            return 0;

        sleep_ms(5);
    }
}


/**
 * @brief Asks whether /proc still lists a pid.
 *
 * @param pid The pid to look for.
 * @return 1 if it is listed, 0 otherwise.
 */
static int proc_exists(pid_t pid) {

    char path[32];
    struct stat st;

    snprintf(path, sizeof(path), "/proc/%d", pid);

    return stat(path, &st) == 0;
}


/**
 * @brief Reads the state letter /proc/<pid>/stat reports for a task.
 *
 * @param pid The task to read.
 * @return The state letter, or '?' if it cannot be read.
 */
static char proc_state(pid_t pid) {

    char path[32];
    char buf[256] = {0};

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    int fd = open(path, O_RDONLY);

    if (fd < 0)
        return '?';

    ssize_t n = read(fd, buf, sizeof(buf) - 1);

    close(fd);

    if (n <= 0)
        return '?';


    char* p = strrchr(buf, ')');

    return (p && p[1] == ' ' && p[2]) ? p[2] : '?';
}


/**
 * @brief Checks that an orphan is handed to init, which reports as its parent and reaps it.
 */
static void test_orphan(void) {

    int p[2];

    if (pipe(p) < 0) {
        CHECK(0, "orphan", "pipe(): %s", strerror(errno));
        return;
    }


    pid_t mid = fork();

    if (mid == 0) {

        pid_t g = fork();

        if (g == 0) {

            pid_t first = getppid();
            uint64_t end = now_ms() + 2000;

            while (getppid() == first && now_ms() < end)
                sleep_ms(5);

            pid_t ppid = getppid();

            write(p[1], &ppid, sizeof(ppid));
            sleep_ms(50);
            _exit(0);
        }

        write(p[1], &g, sizeof(g));
        _exit(0);
    }

    close(p[1]);


    pid_t g    = -1;
    pid_t ppid = -1;
    int st     = 0;

    read(p[0], &g, sizeof(g));
    waitpid(mid, &st, 0);
    read(p[0], &ppid, sizeof(ppid));
    close(p[0]);

    CHECK(ppid == 1, "orphan-ppid", "the orphan's getppid() is %d, expected 1", ppid);


    uint64_t end = now_ms() + 3000;

    while (g > 0 && proc_exists(g) && now_ms() < end)
        sleep_ms(20);

    CHECK(g > 0 && !proc_exists(g), "orphan-reaped", "orphan %d is still listed in /proc 3s after it exited", g);
}


/**
 * @brief Lists the descriptors of a zombie, which no longer has a descriptor table.
 */
static void test_zombie_fd(void) {

    pid_t c = fork();

    if (c == 0)
        _exit(0);

    sleep_ms(100);


    char path[64];
    char link[64];
    int entries = -1;

    snprintf(path, sizeof(path), "/proc/%d/fd", c);

    DIR* d = opendir(path);

    if (d) {

        entries = 0;

        for (struct dirent* e = readdir(d); e; e = readdir(d))
            entries += (e->d_name[0] != '.');

        closedir(d);
    }

    snprintf(path, sizeof(path), "/proc/%d/fd/0", c);

    ssize_t n = readlink(path, link, sizeof(link));

    int st = 0;
    waitpid(c, &st, 0);

    CHECK(entries <= 0 && n < 0, "zombie-fd", "a zombie lists %d descriptors and readlink() of its fd 0 returned %zd", entries, n);
}


/**
 * @brief Signals children as they exit, so that delivery races with the release of their signal handlers.
 */
static void test_kill_exit_race(void) {

    int bad = 0;

    for (int i = 0; i < 300; i++) {

        pid_t c = fork();

        if (c == 0)
            _exit(0);

        if (c < 0) {
            bad++;
            continue;
        }

        for (int k = 0; k < 100000 && kill(c, SIGUSR2) == 0; k++)
            ;

        int st = 0;

        if (waitpid(c, &st, 0) != c)
            bad++;
    }

    CHECK(bad == 0, "kill-exit-race", "%d of 300 rounds went wrong", bad);
}


/**
 * @brief Kills a parent blocked in wait4() and then lets its child exit, before the parent is reaped.
 */
static void test_wait_killed_parent(void) {

    pid_t p = fork();

    if (p == 0) {

        pid_t c = fork();

        if (c == 0) {
            sleep_ms(300);
            _exit(0);
        }

        int st = 0;
        waitpid(c, &st, 0);
        _exit(1);
    }

    sleep_ms(100);
    kill(p, SIGKILL);
    sleep_ms(500);


    int st  = 0;
    pid_t r = wait_timeout(p, &st, 0, 2000);

    CHECK(r == p && WIFSIGNALED(st) && WTERMSIG(st) == SIGKILL, "wait-killed-parent", "waitpid() returned %d with status 0x%x, expected %d killed by SIGKILL", r, st, p);
}


/**
 * @brief Does nothing, as a thread.
 *
 * @param arg Handed back.
 * @return The argument.
 */
static void* thread_noop(void* arg) {
    return arg;
}


/**
 * @brief Creates and joins threads, which must be reaped by nobody and never be reported by wait4().
 */
static void test_thread_reap(void) {

    long before = slab_kb();
    int made    = 0;

    for (int i = 0; i < 300; i++) {

        pthread_t t;

        if (pthread_create(&t, NULL, thread_noop, NULL) != 0)
            break;

        pthread_join(t, NULL);
        made++;
    }

    sleep_ms(50);

    long after = slab_kb();

    CHECK(made == 300 && after - before < LEAK_SLACK_KB, "thread-reap", "joined %d of 300 threads and the kernel heap grew by %ld kB", made, after - before);


    pid_t c = fork();

    if (c == 0) {

        for (int i = 0; i < 8; i++) {

            pthread_t t;

            if (pthread_create(&t, NULL, thread_noop, NULL) == 0)
                pthread_join(t, NULL);
        }

        _exit(5);
    }

    int st  = 0;
    pid_t r = waitpid(-1, &st, 0);

    CHECK(r == c && WIFEXITED(st) && WEXITSTATUS(st) == 5, "wait-skips-threads", "waitpid(-1) returned %d with status 0x%x, expected %d exiting with 5", r, st, c);

    if (r != c)
        waitpid(c, &st, 0);
}


/**
 * @brief Stops and continues a child, which wait4() must report once each, and kills it while stopped.
 */
static void test_stop_cont(void) {

    pid_t c = fork();

    if (c == 0) {
        for (;;)
            sleep_ms(10);
    }

    sleep_ms(50);


    int st = 0;

    kill(c, SIGSTOP);

    pid_t r = wait_timeout(c, &st, WUNTRACED, 2000);
    CHECK(r == c && WIFSTOPPED(st) && WSTOPSIG(st) == SIGSTOP, "stop-reported", "waitpid() returned %d with status 0x%x, expected %d stopped by SIGSTOP", r, st, c);

    r = waitpid(c, &st, WUNTRACED | WNOHANG);
    CHECK(r == 0, "stop-reported-once", "a second waitpid() returned %d with status 0x%x, expected 0", r, st);


    kill(c, SIGCONT);

    r = wait_timeout(c, &st, WCONTINUED, 2000);
    CHECK(r == c && WIFCONTINUED(st), "cont-reported", "waitpid() returned %d with status 0x%x, expected %d continued", r, st, c);

    sleep_ms(50);

    char s = proc_state(c);
    CHECK(s == 'S' || s == 'R', "cont-resumed", "the child is in state '%c' after SIGCONT", s);


    kill(c, SIGSTOP);
    wait_timeout(c, &st, WUNTRACED, 2000);
    kill(c, SIGKILL);

    r = wait_timeout(c, &st, 0, 2000);
    CHECK(r == c && WIFSIGNALED(st) && WTERMSIG(st) == SIGKILL, "kill-stopped", "waitpid() returned %d with status 0x%x, expected %d killed by SIGKILL", r, st, c);
}


static pid_t waited_child;
static pid_t waited_result;
static int waited_status;
static int waited_errno;


/**
 * @brief Waits for the child the main thread forked, as a thread.
 *
 * @param arg Unused.
 * @return NULL.
 */
static void* thread_wait(void* arg) {

    waited_result = waitpid(waited_child, &waited_status, 0);
    waited_errno  = errno;

    return arg;
}


/**
 * @brief Waits for a child from a thread other than the one that forked it.
 */
static void test_wait_any_thread(void) {

    pid_t c = fork();

    if (c == 0) {
        sleep_ms(100);
        _exit(3);
    }

    waited_child  = c;
    waited_result = -2;
    waited_status = 0;


    pthread_t t;

    if (pthread_create(&t, NULL, thread_wait, NULL) != 0) {
        CHECK(0, "wait-any-thread", "pthread_create(): %s", strerror(errno));
        waitpid(c, NULL, 0);
        return;
    }

    pthread_join(t, NULL);

    CHECK(waited_result == c && WIFEXITED(waited_status) && WEXITSTATUS(waited_status) == 3, "wait-any-thread", "waitpid() from a second thread returned %d (%s) with status 0x%x", waited_result,
          waited_result < 0 ? strerror(waited_errno) : "ok", waited_status);

    if (waited_result != c)
        waitpid(c, NULL, 0);
}


/**
 * @brief Sleeps while children exit, which must not cut the sleep short.
 */
static void test_nanosleep_spurious(void) {

    pid_t a = fork();

    if (a == 0) {
        sleep_ms(50);
        _exit(0);
    }

    pid_t b = fork();

    if (b == 0) {
        sleep_ms(300);
        _exit(0);
    }


    int st  = 0;
    pid_t r = waitpid(-1, &st, 0);

    uint64_t t0        = now_ms();
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 800 * 1000000L};

    int e     = nanosleep(&ts, NULL);
    int err   = errno;
    long took = (long)(now_ms() - t0);

    waitpid(r == a ? b : a, &st, 0);

    CHECK(r == a && e == 0 && took >= 790, "nanosleep-spurious", "waitpid(-1) returned %d, then nanosleep(800ms) returned %d (%s) after %ld ms", r, e, e ? strerror(err) : "ok", took);
}


/**
 * @brief Sends init the signals that would stop or kill it, which it must shrug off.
 */
static void test_kill_init(void) {

    int r1 = kill(1, SIGKILL);
    sleep_ms(100);

    int r2 = kill(1, SIGSTOP);
    sleep_ms(100);

    char s    = proc_state(1);
    int alive = kill(1, 0) == 0;

    CHECK(r1 == 0 && r2 == 0 && alive && s != 'T' && s != 'Z', "kill-init", "kill(1, SIGKILL) returned %d, kill(1, SIGSTOP) returned %d, and init is in state '%c'", r1, r2, s);
}


/**
 * @brief Forks children that call exit(), which goes through exit_group(), and checks that nothing is left behind.
 */
static void test_exit_group_leak(void) {

    long before = slab_kb();
    int bad     = 0;

    for (int i = 0; i < 300; i++) {

        pid_t c = fork();

        if (c == 0)
            exit(0);

        int st = 0;

        if (c < 0 || waitpid(c, &st, 0) != c)
            bad++;
    }

    sleep_ms(50);

    long after = slab_kb();

    CHECK(bad == 0 && after - before < LEAK_SLACK_KB, "exit-group-leak", "%d of 300 rounds failed and the kernel heap grew by %ld kB", bad, after - before);
}


/**
 * @brief Calls exit(7) from a thread that is not the main one.
 *
 * @param arg Unused.
 * @return Does not return.
 */
static void* thread_exit7(void* arg) {

    sleep_ms(20);
    exit(7);

    return arg;
}


/**
 * @brief Exits a process from a secondary thread, whose status must be the process's.
 */
static void test_exit_code_thread(void) {

    pid_t c = fork();

    if (c == 0) {

        pthread_t t;
        pthread_create(&t, NULL, thread_exit7, NULL);

        for (;;)
            sleep_ms(10);
    }

    int st  = 0;
    pid_t r = wait_timeout(c, &st, 0, 3000);

    CHECK(r == c && WIFEXITED(st) && WEXITSTATUS(st) == 7, "exit-code-thread", "waitpid() returned %d with status 0x%x, expected %d exiting with 7", r, st, c);
}


/**
 * @brief Outlives the main thread for a while, then exits the process with status 5.
 *
 * @param arg Unused.
 * @return Does not return.
 */
static void* thread_linger(void* arg) {

    sleep_ms(300);
    exit(5);

    return arg;
}


/**
 * @brief Exits the main thread while another carries on, which must keep the process from being reported.
 */
static void test_zombie_leader(void) {

    pid_t c = fork();

    if (c == 0) {

        pthread_t t;
        pthread_create(&t, NULL, thread_linger, NULL);

        pthread_exit(NULL);
    }

    sleep_ms(100);


    int st  = 0;
    pid_t r = waitpid(c, &st, WNOHANG);

    CHECK(r == 0, "leader-waits-for-group", "waitpid() returned %d with status 0x%x while a thread of the child was still running", r, st);

    if (r == c)
        return;

    r = wait_timeout(c, &st, 0, 3000);
    CHECK(r == c && WIFEXITED(st) && WEXITSTATUS(st) == 5, "leader-reaped-with-group", "waitpid() returned %d with status 0x%x, expected %d exiting with 5", r, st, c);
}


/**
 * @brief Spins forever, as a thread.
 *
 * @param arg Unused.
 * @return Does not return.
 */
static void* thread_spin(void* arg) {

    for (volatile unsigned long i = 0;; i++)
        ;

    return arg;
}


/**
 * @brief Runs a multithreaded process whose main thread calls exit() while the others spin on other cpus.
 */
static void test_mt_exit(void) {

    int bad = 0;

    for (int i = 0; i < 100; i++) {

        pid_t c = fork();

        if (c == 0) {

            for (int k = 0; k < 3; k++) {

                pthread_t t;
                pthread_create(&t, NULL, thread_spin, NULL);
            }

            sleep_ms(5);
            exit(0);
        }

        int st = 0;

        if (c < 0 || wait_timeout(c, &st, 0, 3000) != c)
            bad++;
    }

    CHECK(bad == 0, "mt-exit", "%d of 100 multithreaded exits were not reaped", bad);
}


static volatile sig_atomic_t chld_count;
static volatile sig_atomic_t usr1_count;


/**
 * @brief Counts SIGCHLD deliveries.
 *
 * @param signo Unused.
 */
static void on_chld(int signo) {

    (void)signo;
    chld_count++;
}


/**
 * @brief Counts SIGUSR1 deliveries.
 *
 * @param signo Unused.
 */
static void on_usr1(int signo) {

    (void)signo;
    usr1_count++;
}


/**
 * @brief Installs a handler with the given flags.
 *
 * @param signo The signal to catch.
 * @param fn The handler.
 * @param flags The sa_flags to install it with.
 * @return What sigaction() returned.
 */
static int catch_signal(int signo, void (*fn)(int), int flags) {

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = fn;
    sa.sa_flags   = flags;

    sigemptyset(&sa.sa_mask);

    return sigaction(signo, &sa, NULL);
}


/**
 * @brief Checks that a parent hears about its child's exit through SIGCHLD, also from inside sigsuspend().
 */
static void test_sigchld(void) {

    catch_signal(SIGCHLD, on_chld, 0);
    chld_count = 0;


    pid_t c = fork();

    if (c == 0) {
        sleep_ms(50);
        _exit(0);
    }

    uint64_t end = now_ms() + 1000;

    while (!chld_count && now_ms() < end)
        sleep_ms(10);

    int got = chld_count;
    int st  = 0;

    waitpid(c, &st, 0);

    CHECK(got >= 1, "sigchld", "the SIGCHLD handler ran %d times", got);


    sigset_t block;
    sigset_t old;
    sigset_t during;

    sigemptyset(&block);
    sigaddset(&block, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block, &old);

    during = old;
    sigdelset(&during, SIGCHLD);

    chld_count = 0;

    c = fork();

    if (c == 0) {
        sleep_ms(50);
        _exit(0);
    }

    errno = 0;

    int r = sigsuspend(&during);
    int e = errno;

    sigprocmask(SIG_SETMASK, &old, NULL);
    waitpid(c, &st, 0);

    CHECK(r == -1 && e == EINTR && chld_count >= 1, "sigsuspend", "sigsuspend() returned %d (%s) with the handler run %d times", r, strerror(e), (int)chld_count);

    signal(SIGCHLD, SIG_DFL);
}


/**
 * @brief Ignores SIGCHLD, which must spare the children from becoming zombies.
 */
static void test_sigchld_ignore(void) {

    signal(SIGCHLD, SIG_IGN);


    pid_t c = fork();

    if (c == 0)
        _exit(0);

    sleep_ms(200);


    int st = 0;

    errno = 0;

    pid_t r = waitpid(c, &st, WNOHANG);
    int e   = errno;

    signal(SIGCHLD, SIG_DFL);

    CHECK(r == -1 && e == ECHILD, "sigchld-ignore", "waitpid() on a child of a parent ignoring SIGCHLD returned %d (%s)", r, r < 0 ? strerror(e) : r ? "a zombie" : "still running");

    if (r == 0)
        waitpid(c, &st, 0);
}


/**
 * @brief Writes to a pipe and a socket nobody reads any more, which must raise SIGPIPE unless asked not to.
 */
static void test_sigpipe(void) {

    pid_t c = fork();

    if (c == 0) {

        int p[2];

        pipe(p);
        close(p[0]);
        write(p[1], "x", 1);

        _exit(0);
    }

    int st  = 0;
    pid_t r = wait_timeout(c, &st, 0, 2000);

    CHECK(r == c && WIFSIGNALED(st) && WTERMSIG(st) == SIGPIPE, "sigpipe", "a write to a pipe with no reader left status 0x%x, expected death by SIGPIPE", st);


    c = fork();

    if (c == 0) {

        int p[2];

        signal(SIGPIPE, SIG_IGN);

        pipe(p);
        close(p[0]);

        ssize_t n = write(p[1], "x", 1);

        _exit(n == -1 && errno == EPIPE ? 0 : 1);
    }

    r = wait_timeout(c, &st, 0, 2000);

    CHECK(r == c && WIFEXITED(st) && WEXITSTATUS(st) == 0, "sigpipe-ignored", "with SIGPIPE ignored the writer left status 0x%x, expected a clean exit", st);


    c = fork();

    if (c == 0) {

        int sv[2];

        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0)
            _exit(2);

        close(sv[1]);

        ssize_t n = send(sv[0], "x", 1, MSG_NOSIGNAL);

        _exit(n == -1 && errno == EPIPE ? 0 : 1);
    }

    r = wait_timeout(c, &st, 0, 2000);

    CHECK(r == c && WIFEXITED(st) && WEXITSTATUS(st) == 0, "sigpipe-nosignal", "send(MSG_NOSIGNAL) to a closed peer left status 0x%x, expected a clean exit", st);


    c = fork();

    if (c == 0) {

        int sv[2];

        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0)
            _exit(2);

        close(sv[1]);
        write(sv[0], "x", 1);

        _exit(0);
    }

    r = wait_timeout(c, &st, 0, 2000);

    CHECK(r == c && WIFSIGNALED(st) && WTERMSIG(st) == SIGPIPE, "sigpipe-socket", "a write to a closed socket peer left status 0x%x, expected death by SIGPIPE", st);
}


/**
 * @brief Blocks in read() while a child writes a byte and then signals, the handler running on top of the blocked read.
 *
 * @param flags The sa_flags the handler is installed with.
 * @return 0 if the byte arrived, 1 if it was lost, 2 if read() failed with EINTR under SA_RESTART.
 */
static int restart_round(int flags) {

    int p[2];

    if (pipe(p) < 0)
        return 1;

    catch_signal(SIGUSR1, on_usr1, flags);


    pid_t parent = getpid();
    pid_t c      = fork();

    if (c == 0) {

        close(p[0]);
        sleep_ms(20);

        write(p[1], "A", 1);
        kill(parent, SIGUSR1);

        _exit(0);
    }

    close(p[1]);


    char ch   = 0;
    ssize_t n = read(p[0], &ch, 1);
    int eintr = (n < 0 && errno == EINTR);

    if (eintr)
        n = read(p[0], &ch, 1);

    close(p[0]);
    waitpid(c, NULL, 0);
    sleep_ms(10);

    if (n != 1 || ch != 'A')
        return 1;

    return (flags & SA_RESTART) && eintr ? 2 : 0;
}


/**
 * @brief Delivers a handled signal to a task blocked in read() while data arrives, with and without SA_RESTART.
 */
static void test_handler_restart(void) {

    int lost    = 0;
    int eintr   = 0;
    int lost_sa = 0;

    for (int i = 0; i < 50; i++)
        lost += restart_round(0) == 1;

    for (int i = 0; i < 50; i++) {

        int r = restart_round(SA_RESTART);

        lost_sa += r == 1;
        eintr += r == 2;
    }

    signal(SIGUSR1, SIG_IGN);

    CHECK(lost == 0, "handler-restart", "a handled signal lost the byte being read in %d of 50 rounds", lost);
    CHECK(lost_sa == 0 && eintr == 0, "handler-restart-sa", "with SA_RESTART the byte was lost in %d and read() failed with EINTR in %d of 50 rounds", lost_sa, eintr);
}


/**
 * @brief Checks that read() restarted under SA_RESTART waits for data that arrives only after the handler returned.
 */
static void test_restart_waits(void) {

    int eintr = 0;
    int lost  = 0;

    for (int i = 0; i < 10; i++) {

        int p[2];

        if (pipe(p) < 0) {
            lost++;
            continue;
        }

        catch_signal(SIGUSR1, on_usr1, SA_RESTART);


        pid_t parent = getpid();
        pid_t c      = fork();

        if (c == 0) {

            close(p[0]);

            sleep_ms(50);
            kill(parent, SIGUSR1);

            sleep_ms(200);
            write(p[1], "B", 1);

            _exit(0);
        }

        close(p[1]);


        char ch   = 0;
        ssize_t n = read(p[0], &ch, 1);

        if (n < 0 && errno == EINTR) {
            eintr++;
            n = read(p[0], &ch, 1);
        }

        lost += n != 1 || ch != 'B';

        close(p[0]);
        waitpid(c, NULL, 0);
    }

    signal(SIGUSR1, SIG_IGN);

    CHECK(eintr == 0 && lost == 0, "restart-waits", "read() failed with EINTR in %d and lost the byte in %d of 10 rounds", eintr, lost);
}


/**
 * @brief Runs a case body in a child process, killing the child if it outlives its time.
 *
 * @param fn The body; what it returns becomes the child's exit status.
 * @param ms How long the child may run.
 * @return The child's exit status, 128 plus the signal that killed it, or -1 if it hung or could not be started.
 */
static int run_child(int (*fn)(void), unsigned ms) {

    pid_t pid = fork();

    if (pid < 0)
        return -1;

    if (pid == 0)
        _exit(fn());


    int status = 0;

    if (wait_timeout(pid, &status, 0, ms) != pid) {

        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);

        return -1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);

    return -1;
}


/**
 * @brief Forks a child that exits at once, and reaps it.
 *
 * @return 1 if the child ran and exited with status 0, 0 otherwise.
 */
static int child_forks(void) {

    pid_t pid = fork();

    if (pid < 0)
        return 0;

    if (pid == 0)
        _exit(0);

    int status = 0;

    return wait_timeout(pid, &status, 0, 2000) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}


/**
 * @brief Sleeps until time 0 on the monotonic and the realtime clock.
 *
 * @return 0 if both sleeps returned 0, or 1 or 2 for the clock whose sleep failed.
 */
static int abstime_past_child(void) {

    struct timespec zero = {0, 0};

    if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &zero, NULL) != 0)
        return 1;

    if (clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &zero, NULL) != 0)
        return 2;

    return 0;
}


/**
 * @brief Checks that an absolute sleep until a moment already past returns at once.
 */
static void test_sleep_abstime_past(void) {

    int r = run_child(abstime_past_child, 1000);

    CHECK(r == 0, "sleep-abstime-past", "clock_nanosleep(TIMER_ABSTIME) until time 0 ended with code %d (-1 hung, 1 CLOCK_MONOTONIC failed, 2 CLOCK_REALTIME failed)", r);
}


/**
 * @brief Sleeps 20 ms of process cpu time while a second thread spins.
 *
 * @return 0 if the sleep ended or was refused with ENOTSUP, 1 if it failed otherwise, 2 if the thread did not start.
 */
static int cputime_sleep_child(void) {

    pthread_t t;

    if (pthread_create(&t, NULL, thread_spin, NULL) != 0)
        return 2;

    struct timespec ts = {0, 20 * 1000000L};

    int e = clock_nanosleep(CLOCK_PROCESS_CPUTIME_ID, 0, &ts, NULL);

    return e == 0 || e == ENOTSUP ? 0 : 1;
}


/**
 * @brief Checks that a sleep on the process cpu clock, which another thread keeps running, does not hang.
 */
static void test_sleep_cputime(void) {

    int r = run_child(cputime_sleep_child, 3000);

    CHECK(r == 0, "sleep-cputime", "clock_nanosleep(CLOCK_PROCESS_CPUTIME_ID) ended with code %d (-1 hung, 1 failed, 2 no thread)", r);
}


/**
 * @brief Pins the calling process to the first cpu it may use, reads the mask back and forks.
 *
 * @return 0 on success, or the step that failed: 1 get, 2 empty mask, 3 set, 4 read back, 5 fork.
 */
static int affinity_child(void) {

    cpu_set_t set;
    CPU_ZERO(&set);

    if (sched_getaffinity(0, sizeof(set), &set) != 0)
        return 1;

    int cpu = -1;

    for (int i = 0; i < CPU_SETSIZE && cpu < 0; i++) {
        if (CPU_ISSET(i, &set))
            cpu = i;
    }

    if (cpu < 0)
        return 2;


    cpu_set_t one;
    CPU_ZERO(&one);
    CPU_SET(cpu, &one);

    if (sched_setaffinity(0, sizeof(one), &one) != 0)
        return 3;

    CPU_ZERO(&set);

    if (sched_getaffinity(0, sizeof(set), &set) != 0 || !CPU_EQUAL(&set, &one))
        return 4;

    return child_forks() ? 0 : 5;
}


/**
 * @brief Checks that sched_getaffinity() and sched_setaffinity() work with a cpu_set_t.
 */
static void test_affinity(void) {

    int r = run_child(affinity_child, 3000);

    CHECK(r == 0, "affinity", "the child ended with code %d (-1 hung, 1 get, 2 empty mask, 3 set, 4 read back, 5 fork)", r);
}


/**
 * @brief Asks for a mask holding only cpu 1000 through a 1024-byte buffer, then forks.
 *
 * @return 0 if the mask was refused with EINVAL and the fork worked, 1 if it was not refused, 2 if the fork failed.
 */
static int affinity_offline_child(void) {

    static unsigned long mask[1024 / sizeof(unsigned long)];

    mask[1000 / (8 * sizeof(unsigned long))] |= 1UL << (1000 % (8 * sizeof(unsigned long)));

    errno  = 0;
    long r = syscall(SYS_sched_setaffinity, 0, sizeof(mask), mask);
    int e  = errno;

    int forked = child_forks();

    if (r != -1 || e != EINVAL)
        return 1;

    return forked ? 0 : 2;
}


/**
 * @brief Checks that sched_setaffinity() refuses a mask with no cpu that exists, which would leave a child nowhere to run.
 */
static void test_affinity_offline(void) {

    int r = run_child(affinity_offline_child, 3000);

    CHECK(r == 0, "affinity-offline", "the child ended with code %d (-1 hung, 1 mask accepted, 2 fork failed)", r);
}


static struct {

    const char* name;
    void (*fn)(void);

} cases[] = {
    {"orphan", test_orphan},
    {"thread-reap", test_thread_reap},
    {"stop-cont", test_stop_cont},
    {"wait-any-thread", test_wait_any_thread},
    {"nanosleep-spurious", test_nanosleep_spurious},
    {"sleep-abstime-past", test_sleep_abstime_past},
    {"sleep-cputime", test_sleep_cputime},
    {"exit-group-leak", test_exit_group_leak},
    {"exit-code-thread", test_exit_code_thread},
    {"zombie-leader", test_zombie_leader},
    {"sigchld", test_sigchld},
    {"sigchld-ignore", test_sigchld_ignore},
    {"sigpipe", test_sigpipe},
    {"handler-restart", test_handler_restart},
    {"restart-waits", test_restart_waits},
    {"zombie-fd", test_zombie_fd},
    {"wait-killed-parent", test_wait_killed_parent},
    {"kill-exit-race", test_kill_exit_race},
    {"mt-exit", test_mt_exit},
    {"kill-init", test_kill_init},
    {"affinity", test_affinity},
    {"affinity-offline", test_affinity_offline},
};


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("lifecycle-test: starting\n");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {

        if (argc > 1) {

            int wanted = 0;

            for (int a = 1; a < argc; a++)
                wanted |= strcmp(argv[a], cases[i].name) == 0;

            if (!wanted)
                continue;
        }

        cases[i].fn();
    }

    printf("lifecycle-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
