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
 * Regression tests for the blocked-signal mask.
 *
 * A sigset_t is a bit array, and there are three ways to index into it wrongly that all look
 * plausible and all used to be live in this kernel: walking it by byte offset while subscripting
 * an array of words, testing bit N for signal N when signal N lives in bit N-1, and shifting a
 * plain int by more than 31 to reach the high signals. Each mistake reads a bit that belongs to
 * some other signal, so the mask appears to work for whichever signals happen to line up.
 *
 * The cases below pin down which bit belongs to which signal by blocking one signal and watching
 * what happens to its neighbours and to the signals in the upper half of the set.
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


/* How much of a sigset_t either side actually means: the type is 128 bytes, but every syscall
   carrying one also carries a length, and neither the kernel nor sigemptyset() touches more. */
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


/*
 * sigprocmask(), retried across interruption.
 *
 * Unblocking a signal that was held while blocked hands it over immediately, and this kernel runs
 * the handler on the way out of the very call that released it -- so without SA_RESTART the call
 * itself comes back EINTR, having already done its work. Linux never does this (rt_sigprocmask
 * does not block, so nothing interrupts it), but the mask is applied either way, and none of the
 * cases below are about that difference.
 */
static int sigprocmask_retry(int how, const sigset_t* set, sigset_t* old) {

    for (;;) {

        int r = sigprocmask(how, set, old);

        if (r == 0 || errno != EINTR)
            return r;
    }
}


/*
 * Install a catcher for one signal. Returns 0 on success.
 */
static int catch_signal(int signo) {

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);

    return sigaction(signo, &sa, NULL);
}


/*
 * Unblock everything. Tasks start with every signal blocked, so without this nothing below would
 * ever be delivered and every case would "pass" for the wrong reason.
 */
static int unblock_all(void) {

    sigset_t empty;
    sigemptyset(&empty);

    return sigprocmask_retry(SIG_SETMASK, &empty, NULL);
}


/*
 * The premise everything else rests on: with the signal unblocked and a handler installed, raising
 * it runs the handler. If this fails the rest of the file is not measuring what it claims to.
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


/*
 * A blocked signal is held, not dropped and not delivered. This is the case the off-by-one used
 * to break: blocking signal N set bit N-1, the kernel asked about bit N, found it clear, and
 * delivered the signal anyway.
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


    /* Held, not dropped: unblocking has to hand it over. */
    if (sigprocmask_retry(SIG_UNBLOCK, &block, NULL) < 0) {
        CHECK(0, "blocked-released", "sigprocmask() failed: %s", strerror(errno));
        return;
    }

    sleep_ms(100);

    CHECK(caught[SIGUSR1] >= 1, "blocked-released", "%s", "a signal held while blocked was never delivered after unblocking");
}


/*
 * Blocking one signal blocks exactly that one. An index that is off by one in either direction
 * catches a neighbour instead, which this notices and the single-signal case above does not.
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


/*
 * A signal in the upper half of the set. Reaching its bit means shifting past 31, which a plain
 * int cannot do: on x86 the count is masked to five bits, so bit 39 used to come out as bit 7 and
 * the signal was read as unblocked.
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


/* Filled in from inside a running handler, where the mask is not otherwise observable. */
static volatile sig_atomic_t inside_ran;
static sigset_t inside_mask;


static void mask_probe_handler(int signo) {

    inside_ran++;

    if (signo > 0 && signo < _NSIG)
        caught[signo]++;

    sigemptyset(&inside_mask);
    sigprocmask(SIG_SETMASK, NULL, &inside_mask);
}


/*
 * What a handler runs under. POSIX builds it by addition, not replacement: whatever was blocked
 * already, plus the handler's own sa_mask, plus the signal being delivered.
 *
 * This is where the sa_mask over-read showed: sa_mask is two words, and copying a whole 128-byte
 * sigset_t out of it pulled in the neighbouring entries of the action table and installed them as
 * the mask, so which signals a handler ran under was decided by whatever happened to sit next to
 * it in memory.
 */
static void test_handler_mask(void) {

    if (unblock_all() < 0) {
        CHECK(0, "handler-mask", "sigprocmask() failed: %s", strerror(errno));
        return;
    }


    /* Blocked before the handler is ever entered: an install that replaces rather than adds
       loses this one. */
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


    /* And the whole lot is handed back on the way out. */
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


/*
 * SA_NODEFER is the one case where the delivered signal is not added: the handler is willing to
 * be re-entered by its own signal.
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


/*
 * The mask reads back as it was written, and the old mask handed out on the way in is the one that
 * was in force. A word-versus-byte confusion in the walk shows up here as a mask that only ever
 * takes its first word.
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


/*
 * A size that is not a whole number of words describes a set neither side can walk, and one larger
 * than a sigset_t describes memory the caller does not have.
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


    /* And the size everyone actually sends still works. */
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


/*
 * With no argument every case runs. Naming one runs just that case.
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
