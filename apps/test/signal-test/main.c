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
 * @brief Regression tests for the blocked-signal mask.
 *
 * A sigset_t is a bit array, and every way of indexing into it wrongly used to be live in this kernel.
 */

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                         \
    {                                                                       \
        total++;                                                            \
        if (cond) {                                                         \
            printf("signal-test: PASS  %s\n", (name));                      \
        } else {                                                            \
            failures++;                                                     \
            printf("signal-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                   \
    }


/**
 * @brief How much of a sigset_t either side actually means, which is what the length beside it carries.
 */
#define SIGMASK_BYTES (_NSIG / 8)


static volatile sig_atomic_t caught[_NSIG];


static void handler(int signo) {

    if (signo > 0 && signo < _NSIG)
        caught[signo]++;
}


static void sleep_ms(unsigned ms) {

    struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (long)(ms % 1000) * 1000000L};

    nanosleep(&ts, NULL);
}


/**
 * @brief Changes the blocked mask, retrying across the interruption a released signal causes here.
 *
 * @param how SIG_BLOCK, SIG_UNBLOCK or SIG_SETMASK.
 * @param set The mask to apply.
 * @param old Receives the mask that was in force, or NULL.
 * @return 0 on success, or -1 with errno set.
 */
static int sigprocmask_retry(int how, const sigset_t* set, sigset_t* old) {

    for (;;) {

        int r = sigprocmask(how, set, old);

        if (r == 0 || errno != EINTR)
            return r;
    }
}


/**
 * @brief Installs a catcher for one signal.
 *
 * @param signo The signal to catch.
 * @return 0 on success, or -1 with errno set.
 */
static int catch_signal(int signo) {

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);

    return sigaction(signo, &sa, NULL);
}


/**
 * @brief Unblocks everything, which tasks do not start out as.
 *
 * @return 0 on success, or -1 with errno set.
 */
static int unblock_all(void) {

    sigset_t empty;
    sigemptyset(&empty);

    return sigprocmask_retry(SIG_SETMASK, &empty, NULL);
}


/**
 * @brief Checks the premise everything else rests on: an unblocked signal with a handler runs it.
 */
static void test_delivery(void) {

    if (unblock_all() < 0) {
        CHECK(0, "delivery", "sigprocmask() failed: %s", strerror(errno));
        return;
    }

    if (catch_signal(SIGUSR1) < 0) {
        CHECK(0, "delivery", "sigaction() failed: %s", strerror(errno));
        return;
    }


    caught[SIGUSR1] = 0;

    raise(SIGUSR1);
    sleep_ms(100);

    CHECK(caught[SIGUSR1] == 1, "delivery", "handler ran %d times, expected 1", (int)caught[SIGUSR1]);
}


/**
 * @brief Checks that a blocked signal is held rather than dropped or delivered.
 */
static void test_blocked_is_held(void) {

    if (unblock_all() < 0 || catch_signal(SIGUSR1) < 0) {
        CHECK(0, "blocked", "setup failed: %s", strerror(errno));
        return;
    }


    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, SIGUSR1);

    if (sigprocmask_retry(SIG_BLOCK, &block, NULL) < 0) {
        CHECK(0, "blocked", "sigprocmask() failed: %s", strerror(errno));
        return;
    }


    caught[SIGUSR1] = 0;

    raise(SIGUSR1);
    sleep_ms(100);

    CHECK(caught[SIGUSR1] == 0, "blocked", "a blocked signal was delivered anyway (handler ran %d times)", (int)caught[SIGUSR1]);


    if (sigprocmask_retry(SIG_UNBLOCK, &block, NULL) < 0) {
        CHECK(0, "blocked-released", "sigprocmask() failed: %s", strerror(errno));
        return;
    }

    sleep_ms(100);

    CHECK(caught[SIGUSR1] >= 1, "blocked-released", "%s", "a signal held while blocked was never delivered after unblocking");
}


/**
 * @brief Checks that blocking one signal blocks exactly that one, and not a neighbour.
 */
static void test_neighbours_unaffected(void) {

    if (unblock_all() < 0 || catch_signal(SIGUSR1) < 0 || catch_signal(SIGUSR2) < 0) {
        CHECK(0, "neighbour", "setup failed: %s", strerror(errno));
        return;
    }


    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, SIGUSR1);

    if (sigprocmask_retry(SIG_BLOCK, &block, NULL) < 0) {
        CHECK(0, "neighbour", "sigprocmask() failed: %s", strerror(errno));
        return;
    }


    caught[SIGUSR2] = 0;

    raise(SIGUSR2);
    sleep_ms(100);

    CHECK(caught[SIGUSR2] == 1, "neighbour", "blocking SIGUSR1 also withheld SIGUSR2 (handler ran %d times, expected 1)", (int)caught[SIGUSR2]);

    sigprocmask_retry(SIG_UNBLOCK, &block, NULL);
}


/**
 * @brief Checks a signal in the upper half of the set, whose bit cannot be reached by shifting an int.
 */
static void test_high_signal(void) {

    int signo = SIGRTMIN + 7;

    if (signo >= _NSIG || signo <= 32) {
        printf("signal-test: SKIP  high-signal (no realtime signal above 32 available)\n");
        return;
    }

    if (unblock_all() < 0 || catch_signal(signo) < 0) {
        CHECK(0, "high-signal", "setup failed: %s", strerror(errno));
        return;
    }


    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, signo);

    if (sigprocmask_retry(SIG_BLOCK, &block, NULL) < 0) {
        CHECK(0, "high-signal", "sigprocmask() failed: %s", strerror(errno));
        return;
    }


    caught[signo] = 0;

    raise(signo);
    sleep_ms(100);

    CHECK(caught[signo] == 0, "high-signal", "blocked signal %d was delivered anyway (handler ran %d times)", signo, (int)caught[signo]);


    if (sigprocmask_retry(SIG_UNBLOCK, &block, NULL) < 0) {
        CHECK(0, "high-signal-released", "sigprocmask() failed: %s", strerror(errno));
        return;
    }

    sleep_ms(100);

    CHECK(caught[signo] >= 1, "high-signal-released", "signal %d was held while blocked but never delivered after unblocking", signo);
}


/**
 * @brief Filled in from inside a running handler, where the mask is not otherwise observable.
 */
static volatile sig_atomic_t inside_ran;
static sigset_t inside_mask;


static void mask_probe_handler(int signo) {

    inside_ran++;

    if (signo > 0 && signo < _NSIG)
        caught[signo]++;

    sigemptyset(&inside_mask);
    sigprocmask(SIG_SETMASK, NULL, &inside_mask);
}


/**
 * @brief Checks what a handler runs under: whatever was blocked already, plus sa_mask, plus the signal.
 */
static void test_handler_mask(void) {

    if (unblock_all() < 0) {
        CHECK(0, "handler-mask", "sigprocmask() failed: %s", strerror(errno));
        return;
    }


    sigset_t pre;
    sigemptyset(&pre);
    sigaddset(&pre, SIGCHLD);

    if (sigprocmask_retry(SIG_BLOCK, &pre, NULL) < 0) {
        CHECK(0, "handler-mask", "sigprocmask() failed: %s", strerror(errno));
        return;
    }


    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = mask_probe_handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGUSR2);

    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        CHECK(0, "handler-mask", "sigaction() failed: %s", strerror(errno));
        return;
    }


    inside_ran = 0;

    raise(SIGUSR1);
    sleep_ms(100);

    if (inside_ran == 0) {
        CHECK(0, "handler-mask", "%s", "the handler never ran, so the mask inside it was never sampled");
        return;
    }


    CHECK(sigismember(&inside_mask, SIGUSR2) == 1, "handler-mask-samask", "%s", "sa_mask was not applied for the duration of the handler");
    CHECK(sigismember(&inside_mask, SIGUSR1) == 1, "handler-mask-self", "%s", "the signal being handled was not blocked inside its own handler");
    CHECK(sigismember(&inside_mask, SIGCHLD) == 1, "handler-mask-kept", "%s", "a signal blocked before the handler was unblocked by entering it");
    CHECK(sigismember(&inside_mask, SIGALRM) == 0, "handler-mask-clean", "%s", "a signal in neither the old mask nor sa_mask was blocked inside the handler");


    sigset_t after;
    sigemptyset(&after);

    if (sigprocmask_retry(SIG_SETMASK, NULL, &after) < 0) {
        CHECK(0, "handler-mask-restored", "sigprocmask() failed: %s", strerror(errno));
        return;
    }

    CHECK(sigismember(&after, SIGUSR2) == 0, "handler-mask-restored", "%s", "sa_mask was left in force after the handler returned");
    CHECK(sigismember(&after, SIGCHLD) == 1, "handler-mask-restored-pre", "%s", "the mask in force before the handler was not restored after it");

    unblock_all();
}


/**
 * @brief Checks that SA_NODEFER leaves the delivered signal out of the handler's mask.
 */
static void test_nodefer(void) {

    if (unblock_all() < 0) {
        CHECK(0, "nodefer", "sigprocmask() failed: %s", strerror(errno));
        return;
    }


    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = mask_probe_handler;
    sa.sa_flags   = SA_NODEFER;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        CHECK(0, "nodefer", "sigaction() failed: %s", strerror(errno));
        return;
    }


    inside_ran = 0;

    raise(SIGUSR1);
    sleep_ms(100);

    if (inside_ran == 0) {
        CHECK(0, "nodefer", "%s", "the handler never ran, so the mask inside it was never sampled");
        return;
    }

    CHECK(sigismember(&inside_mask, SIGUSR1) == 0, "nodefer", "%s", "SA_NODEFER was ignored and the signal was blocked inside its own handler");

    unblock_all();
}


/**
 * @brief Checks that the mask reads back as it was written, and that the old mask handed out is the one in force.
 */
static void test_mask_roundtrip(void) {

    if (unblock_all() < 0) {
        CHECK(0, "roundtrip", "sigprocmask() failed: %s", strerror(errno));
        return;
    }


    sigset_t want, got, old;

    sigemptyset(&want);
    sigaddset(&want, SIGUSR1);
    sigaddset(&want, SIGUSR2);
    sigaddset(&want, SIGALRM);

    if (sigprocmask_retry(SIG_SETMASK, &want, &old) < 0) {
        CHECK(0, "roundtrip", "sigprocmask() failed: %s", strerror(errno));
        return;
    }

    sigemptyset(&got);

    if (sigprocmask_retry(SIG_SETMASK, NULL, &got) < 0) {
        CHECK(0, "roundtrip", "sigprocmask() failed: %s", strerror(errno));
        return;
    }

    CHECK(memcmp(&want, &got, SIGMASK_BYTES) == 0, "roundtrip", "%s", "the mask did not read back as it was set");

    CHECK(sigismember(&got, SIGUSR1) == 1, "roundtrip-usr1", "%s", "SIGUSR1 is missing from the mask that was just set");
    CHECK(sigismember(&got, SIGALRM) == 1, "roundtrip-alrm", "%s", "SIGALRM is missing from the mask that was just set");
    CHECK(sigismember(&got, SIGCHLD) == 0, "roundtrip-chld", "%s", "SIGCHLD is in a mask it was never added to");

    unblock_all();
}


/**
 * @brief Checks that a sigsetsize that is not a whole number of words, or larger than a sigset_t, is refused.
 */
static void test_bad_sigsetsize(void) {

    sigset_t set;
    sigemptyset(&set);

    errno = 0;

    long r = syscall(SYS_rt_sigprocmask, SIG_SETMASK, &set, NULL, (size_t)3);

    CHECK(r < 0, "bad-size", "rt_sigprocmask with a 3-byte set returned %ld, expected a failure", r);


    errno = 0;

    r = syscall(SYS_rt_sigprocmask, SIG_SETMASK, &set, NULL, sizeof(sigset_t) + 8);

    CHECK(r < 0, "bad-size-large", "rt_sigprocmask with an oversized set returned %ld, expected a failure", r);


    errno = 0;

    r = syscall(SYS_rt_sigprocmask, SIG_SETMASK, &set, NULL, (size_t)SIGMASK_BYTES);

    CHECK(r == 0, "good-size", "rt_sigprocmask with a %d-byte set returned %ld (%s), expected 0", (int)SIGMASK_BYTES, r, strerror(errno));
}


static struct {

    const char* name;
    void (*fn)(void);

} cases[] = {
    {"delivery", test_delivery},
    {"blocked", test_blocked_is_held},
    {"neighbour", test_neighbours_unaffected},
    {"high-signal", test_high_signal},
    {"handler-mask", test_handler_mask},
    {"nodefer", test_nodefer},
    {"roundtrip", test_mask_roundtrip},
    {"bad-size", test_bad_sigsetsize},
};


/**
 * @brief Runs every case, or the one named on the command line.
 *
 * @param argc The argument count.
 * @param argv The arguments; an optional case name.
 * @return 0 when every case that ran passed.
 */
int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("signal-test: starting\n");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {

        if (argc > 1 && strcmp(argv[1], cases[i].name) != 0)
            continue;

        cases[i].fn();
    }

    printf("signal-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
