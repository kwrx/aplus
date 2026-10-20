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
 * @brief Regression tests for the blocked-signal mask, signal frames, process-directed signals and futex waits.
 *
 * A sigset_t is a bit array, and every way of indexing into it wrongly used to be live in this kernel.
 */

#include <errno.h>
#include <fcntl.h>
#include <fenv.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
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


/**
 * @brief The futex operations the futex cases issue, which no userspace header here defines.
 */
#define FUTEX_OP_WAIT        0
#define FUTEX_OP_WAKE        1
#define FUTEX_OP_CMP_REQUEUE 4
#define FUTEX_OP_LOCK_PI     6


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
static void sleep_full_ms(unsigned ms) {

    uint64_t end = now_ms() + ms;

    for (uint64_t now = now_ms(); now < end; now = now_ms()) {

        struct timespec ts = {.tv_sec = (end - now) / 1000, .tv_nsec = (long)((end - now) % 1000) * 1000000L};

        nanosleep(&ts, NULL);
    }
}


/**
 * @brief Polls a child with WNOHANG until it is reported or time runs out.
 *
 * @param pid The child.
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

        sleep_full_ms(5);
    }
}


/**
 * @brief Waits for a child, killing it if it outlives its time.
 *
 * @param pid The child.
 * @param ms How long it may run.
 * @return The child's exit status, 128 plus the signal that killed it, or -1 if it hung.
 */
static int reap_child(pid_t pid, unsigned ms) {

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

    return reap_child(pid, ms);
}


/**
 * @brief Describes what became of a child run by run_child().
 *
 * @param r What run_child() returned.
 * @param codes What each failure code a body returns means, indexed by code.
 * @param ncodes How many entries @p codes has.
 * @return A description, in a buffer reused by the next call.
 */
static const char* child_result(int r, const char* const* codes, size_t ncodes) {

    static char buf[160];

    if (r < 0)
        snprintf(buf, sizeof(buf), "hung and was killed");
    else if (r >= 128)
        snprintf(buf, sizeof(buf), "died of signal %d", r - 128);
    else if ((size_t)r < ncodes && codes[r])
        snprintf(buf, sizeof(buf), "failed: %s", codes[r]);
    else
        snprintf(buf, sizeof(buf), "failed with code %d", r);

    return buf;
}


/**
 * @brief Filled in by nest_handler(), which runs once for the outer signal and once more nested inside it.
 */
static volatile sig_atomic_t nest_depth;
static volatile sig_atomic_t nest_max;
static volatile sig_atomic_t nest_inner_done;
static volatile sig_atomic_t nest_outer_ok;
static int nest_outer_sig;
static int nest_inner_sig;


/**
 * @brief Raises the inner signal from inside the outer handler and waits for it, then checks the outer frame survived.
 *
 * @param signo The signal being handled.
 */
static void nest_handler(int signo) {

    nest_depth++;

    if (nest_depth > nest_max)
        nest_max = nest_depth;

    if (nest_depth == 1 && signo == nest_outer_sig) {

        volatile uint64_t canary[16];
        volatile double scale = 1.25;

        for (int i = 0; i < 16; i++)
            canary[i] = 0xC0FFEE0000ULL + (uint64_t)i;

        raise(nest_inner_sig);

        for (uint64_t end = now_ms() + 500; !nest_inner_done && now_ms() < end;)
            ;

        int ok = nest_inner_done && scale == 1.25;

        for (int i = 0; i < 16; i++)
            ok = ok && canary[i] == 0xC0FFEE0000ULL + (uint64_t)i;

        nest_outer_ok = ok;

    } else {

        volatile uint64_t junk[64];

        for (int i = 0; i < 64; i++)
            junk[i] = ~0ULL;

        nest_inner_done = junk[63] == ~0ULL;
    }

    nest_depth--;
}


/**
 * @brief Takes the outer signal, whose handler takes the inner one nested, and checks every frame came back intact.
 *
 * @return 0 on success, or a failure code.
 */
static int nest_child(void) {

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = nest_handler;
    sa.sa_flags   = nest_outer_sig == nest_inner_sig ? SA_NODEFER : 0;
    sigemptyset(&sa.sa_mask);

    if (sigaction(nest_outer_sig, &sa, NULL) < 0 || sigaction(nest_inner_sig, &sa, NULL) < 0 || unblock_all() < 0)
        return 10;


    volatile uint64_t mine = 0xABCDEF0123ULL;

    raise(nest_outer_sig);

    for (uint64_t end = now_ms() + 1000; (nest_max == 0 || nest_depth != 0) && now_ms() < end;)
        ;

    if (nest_max < 2)
        return 2;

    if (!nest_outer_ok)
        return 3;

    if (mine != 0xABCDEF0123ULL)
        return 4;

    return 0;
}


/**
 * @brief What nest_child() failure codes mean.
 */
static const char* const nest_codes[] = {
    [2]  = "the inner handler never ran nested",
    [3]  = "the outer handler's frame did not survive the nested one",
    [4]  = "the interrupted code's frame did not survive",
    [10] = "setup failed",
};


/**
 * @brief Checks that a handler taking its own signal again under SA_NODEFER returns through both frames intact.
 */
static void test_nested(void) {

    nest_outer_sig = SIGUSR1;
    nest_inner_sig = SIGUSR1;

    int r = run_child(nest_child, 3000);

    CHECK(r == 0, "nested", "a handler nested in itself %s", child_result(r, nest_codes, sizeof(nest_codes) / sizeof(nest_codes[0])));
}


/**
 * @brief Checks that a handler interrupted by a different signal returns through both frames intact.
 */
static void test_nested_other(void) {

    nest_outer_sig = SIGUSR1;
    nest_inner_sig = SIGUSR2;

    int r = run_child(nest_child, 3000);

    CHECK(r == 0, "nested-other", "a SIGUSR2 handler nested in a SIGUSR1 handler %s", child_result(r, nest_codes, sizeof(nest_codes) / sizeof(nest_codes[0])));
}


#define FRAME_THREADS 4
#define FRAME_ROUNDS  200

/**
 * @brief Which frame_thread() a thread is, and what the handlers have seen.
 */
static __thread int frame_index;
static atomic_int frame_seen[FRAME_THREADS];
static atomic_int frame_bad;


/**
 * @brief Holds a pattern of its own thread on the handler's stack for a while, then checks it and the payload.
 *
 * @param signo The signal.
 * @param info Its details, whose value names the thread it was sent to.
 * @param ctx The interrupted context.
 */
static void frame_handler(int signo, siginfo_t* info, void* ctx) {

    (void)signo;
    (void)ctx;

    volatile uint64_t canary[32];

    for (int i = 0; i < 32; i++)
        canary[i] = ((uint64_t)frame_index << 32) | (uint64_t)i;

    for (volatile int spin = 0; spin < 200000; spin++)
        ;

    int ok = info->si_value.sival_int == frame_index;

    for (int i = 0; i < 32; i++)
        ok = ok && canary[i] == (((uint64_t)frame_index << 32) | (uint64_t)i);

    if (!ok)
        atomic_fetch_add(&frame_bad, 1);

    atomic_fetch_add(&frame_seen[frame_index], 1);
}


/**
 * @brief Signals its own thread over and over with its index as the payload, waiting for each to be handled.
 *
 * @param arg The thread's index.
 * @return NULL, or non-NULL if a signal could not be sent.
 */
static void* frame_thread(void* arg) {

    frame_index = (int)(intptr_t)arg;

    pid_t tid = (pid_t)syscall(SYS_gettid);

    for (int i = 0; i < FRAME_ROUNDS; i++) {

        siginfo_t si;

        memset(&si, 0, sizeof(si));

        si.si_signo           = SIGUSR1;
        si.si_code            = SI_QUEUE;
        si.si_pid             = getpid();
        si.si_uid             = getuid();
        si.si_value.sival_int = frame_index;

        if (syscall(SYS_rt_tgsigqueueinfo, getpid(), tid, SIGUSR1, &si) < 0)
            return (void*)1;

        for (uint64_t end = now_ms() + 500; atomic_load(&frame_seen[frame_index]) <= i && now_ms() < end;)
            ;
    }

    return NULL;
}


/**
 * @brief Runs frame_thread() on several threads at once, so that their handlers overlap.
 *
 * @return 0 on success, or a failure code.
 */
static int frame_child(void) {

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_sigaction = frame_handler;
    sa.sa_flags     = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) < 0 || unblock_all() < 0)
        return 10;


    pthread_t t[FRAME_THREADS];

    for (int i = 0; i < FRAME_THREADS; i++) {

        if (pthread_create(&t[i], NULL, frame_thread, (void*)(intptr_t)i) != 0)
            return 10;
    }

    int failed = 0;

    for (int i = 0; i < FRAME_THREADS; i++) {

        void* r = NULL;

        pthread_join(t[i], &r);

        failed |= r != NULL;
    }

    if (failed)
        return 11;

    for (int i = 0; i < FRAME_THREADS; i++) {

        if (atomic_load(&frame_seen[i]) != FRAME_ROUNDS)
            return 2;
    }

    return atomic_load(&frame_bad) ? 3 : 0;
}


/**
 * @brief Checks that threads taking signals at the same time each get a frame and a siginfo of their own.
 */
static void test_thread_frames(void) {

    static const char* const codes[] = {
        [2]  = "a thread's signal was not handled exactly once",
        [3]  = "a handler found its stack or its siginfo overwritten by another thread's",
        [10] = "setup failed",
        [11] = "rt_tgsigqueueinfo() failed",
    };

    int r = run_child(frame_child, 10000);

    CHECK(r == 0, "thread-frames", "%d threads taking signals at once %s", FRAME_THREADS, child_result(r, codes, sizeof(codes) / sizeof(codes[0])));
}


/**
 * @brief Filled in by ctx_handler() from the context it was handed.
 */
static volatile sig_atomic_t ctx_ran;
static volatile sig_atomic_t ctx_ok;


/**
 * @brief Checks the siginfo and the interrupted context a handler is handed.
 *
 * @param signo The signal.
 * @param info Its details.
 * @param ctx The interrupted context.
 */
static void ctx_handler(int signo, siginfo_t* info, void* ctx) {

    ucontext_t* uc = ctx;
    volatile char here;

    ctx_ok = signo == SIGUSR1 && info && info->si_signo == SIGUSR1 && uc && sigismember(&uc->uc_sigmask, SIGUSR2) == 1 && sigismember(&uc->uc_sigmask, SIGUSR1) == 0 && uc->uc_mcontext.gregs[REG_RIP] != 0 &&
             (uintptr_t)uc->uc_mcontext.gregs[REG_RSP] > (uintptr_t)&here;

    ctx_ran = 1;
}


/**
 * @brief Takes a signal with SIGUSR2 blocked and checks what the handler saw and what is in force after it.
 *
 * @return 0 on success, or a failure code.
 */
static int ctx_child(void) {

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_sigaction = ctx_handler;
    sa.sa_flags     = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) < 0)
        return 10;


    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, SIGUSR2);

    if (sigprocmask_retry(SIG_SETMASK, &block, NULL) < 0)
        return 10;

    raise(SIGUSR1);

    for (uint64_t end = now_ms() + 1000; !ctx_ran && now_ms() < end;)
        ;

    if (!ctx_ran)
        return 2;

    if (!ctx_ok)
        return 3;


    sigset_t after;
    sigemptyset(&after);

    if (sigprocmask_retry(SIG_SETMASK, NULL, &after) < 0 || sigismember(&after, SIGUSR2) != 1 || sigismember(&after, SIGUSR1) != 0)
        return 4;

    return 0;
}


/**
 * @brief Checks that an SA_SIGINFO handler is handed its siginfo and a context describing the interrupted code.
 */
static void test_ucontext(void) {

    static const char* const codes[] = {
        [2]  = "the handler never ran",
        [3]  = "the handler was handed a missing or wrong siginfo or context",
        [4]  = "the mask in force before the handler was not restored after it",
        [10] = "setup failed",
    };

    int r = run_child(ctx_child, 3000);

    CHECK(r == 0, "ucontext", "an SA_SIGINFO handler %s", child_result(r, codes, sizeof(codes) / sizeof(codes[0])));
}


/**
 * @brief Where resume_handler() sends the interrupted code instead of back to where it was.
 */
static void resume_escape(void) {
    _exit(42);
}


/**
 * @brief Rewrites the interrupted instruction pointer in the context it was handed.
 *
 * @param signo The signal.
 * @param info Its details.
 * @param ctx The interrupted context.
 */
static void resume_handler(int signo, siginfo_t* info, void* ctx) {

    ucontext_t* uc = ctx;

    (void)signo;
    (void)info;

    if (!uc)
        _exit(3);

    uc->uc_mcontext.gregs[REG_RIP] = (greg_t)(uintptr_t)resume_escape;
}


/**
 * @brief Takes a signal whose handler redirects the code it interrupted, and spins where it would otherwise resume.
 *
 * @return 42 once redirected, or a failure code.
 */
static int resume_child(void) {

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_sigaction = resume_handler;
    sa.sa_flags     = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) < 0 || unblock_all() < 0)
        return 10;

    raise(SIGUSR1);

    for (uint64_t end = now_ms() + 1000; now_ms() < end;)
        ;

    return 2;
}


/**
 * @brief Checks that rt_sigreturn resumes the context as the handler left it, which pthread_cancel() depends on.
 */
static void test_ucontext_resume(void) {

    static const char* const codes[] = {
        [2]  = "the handler's change to the interrupted context was ignored",
        [3]  = "the handler was handed no context",
        [10] = "setup failed",
    };

    int r = run_child(resume_child, 3000);

    CHECK(r == 42, "ucontext-resume", "a handler redirecting the interrupted code %s", child_result(r, codes, sizeof(codes) / sizeof(codes[0])));
}


/**
 * @brief Blocks in read() on a pipe nobody writes to.
 *
 * @param arg The descriptor to read.
 * @return NULL, unless cancelled.
 */
static void* cancel_read_thread(void* arg) {

    char c;

    read((int)(intptr_t)arg, &c, 1);

    return NULL;
}


/**
 * @brief Sleeps until cancelled.
 *
 * @param arg Unused.
 * @return Never, unless cancelled.
 */
static void* cancel_sleep_thread(void* arg) {

    (void)arg;

    for (;;)
        sleep(10);

    return NULL;
}


/**
 * @brief Cancels a thread once it is blocked, and checks it ends cancelled.
 *
 * @param fn What the thread runs.
 * @return 0 on success, or a failure code.
 */
static int cancel_child_run(void* (*fn)(void*)) {

    int p[2];

    if (pipe(p) < 0)
        return 10;


    pthread_t t;

    if (pthread_create(&t, NULL, fn, (void*)(intptr_t)p[0]) != 0)
        return 10;

    sleep_full_ms(100);

    if (pthread_cancel(t) != 0)
        return 11;


    void* r = NULL;

    if (pthread_join(t, &r) != 0)
        return 12;

    return r == PTHREAD_CANCELED ? 0 : 2;
}


/**
 * @brief Cancels a thread blocked in read(), which is restarted under the cancellation handler's SA_RESTART.
 *
 * @return 0 on success, or a failure code.
 */
static int cancel_read_child(void) {
    return cancel_child_run(cancel_read_thread);
}


/**
 * @brief Cancels a thread blocked in nanosleep(), which fails with EINTR instead of restarting.
 *
 * @return 0 on success, or a failure code.
 */
static int cancel_sleep_child(void) {
    return cancel_child_run(cancel_sleep_thread);
}


/**
 * @brief What cancel_child_run() failure codes mean.
 */
static const char* const cancel_codes[] = {
    [2]  = "the thread was not cancelled",
    [10] = "setup failed",
    [11] = "pthread_cancel() failed",
    [12] = "pthread_join() failed",
};


/**
 * @brief Checks that pthread_cancel() ends a thread blocked in a restartable read().
 */
static void test_cancel_read(void) {

    int r = run_child(cancel_read_child, 3000);

    CHECK(r == 0, "pthread-cancel-read", "cancelling a thread blocked in read() %s", child_result(r, cancel_codes, sizeof(cancel_codes) / sizeof(cancel_codes[0])));
}


/**
 * @brief Checks that pthread_cancel() ends a thread blocked in sleep().
 */
static void test_cancel_sleep(void) {

    int r = run_child(cancel_sleep_child, 3000);

    CHECK(r == 0, "pthread-cancel-sleep", "cancelling a thread blocked in sleep() %s", child_result(r, cancel_codes, sizeof(cancel_codes) / sizeof(cancel_codes[0])));
}


#define ALT_STACK_SIZE 65536

/**
 * @brief The alternate stacks the sigaltstack cases hand out, and where their handlers found themselves running.
 */
static char alt_stack[ALT_STACK_SIZE] __attribute__((aligned(16)));
static volatile uintptr_t alt_where;
static volatile int alt_flags;


/**
 * @brief Asks whether an address lies within alt_stack.
 *
 * @param p The address.
 * @return 1 if it does, 0 otherwise.
 */
static int on_alt_stack(uintptr_t p) {
    return p >= (uintptr_t)alt_stack && p < (uintptr_t)alt_stack + sizeof(alt_stack);
}


/**
 * @brief Records where it runs and what sigaltstack() reports from there.
 *
 * @param signo The signal.
 */
static void alt_handler(int signo) {

    volatile char here;
    stack_t cur;

    (void)signo;

    alt_flags = sigaltstack(NULL, &cur) == 0 ? cur.ss_flags : -1;
    alt_where = (uintptr_t)&here;
}


/**
 * @brief Raises a signal for alt_handler() and waits for it to run.
 */
static void alt_raise(void) {

    alt_where = 0;

    raise(SIGUSR1);

    for (uint64_t end = now_ms() + 1000; !alt_where && now_ms() < end;)
        ;
}


/**
 * @brief Takes a signal on an alternate stack, then with the stack disabled, and checks sigaltstack() along the way.
 *
 * @return 0 on success, or a failure code.
 */
static int alt_child(void) {

    stack_t ss = {.ss_sp = alt_stack, .ss_size = sizeof(alt_stack), .ss_flags = 0};

    if (sigaltstack(&ss, NULL) < 0)
        return 2;


    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = alt_handler;
    sa.sa_flags   = SA_ONSTACK;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) < 0 || unblock_all() < 0)
        return 10;

    alt_raise();

    if (!on_alt_stack(alt_where))
        return 3;

    if (alt_flags != SS_ONSTACK)
        return 4;


    stack_t cur;

    if (sigaltstack(NULL, &cur) < 0 || cur.ss_flags != 0 || cur.ss_sp != alt_stack || cur.ss_size != sizeof(alt_stack))
        return 5;


    ss.ss_flags = SS_DISABLE;

    if (sigaltstack(&ss, NULL) < 0)
        return 6;

    alt_raise();

    if (!alt_where || on_alt_stack(alt_where))
        return 7;


    ss.ss_flags = 0;
    ss.ss_size  = 1024;

    if (sigaltstack(&ss, NULL) == 0 || errno != ENOMEM)
        return 8;

    return 0;
}


/**
 * @brief Checks sigaltstack() and that SA_ONSTACK handlers run on the alternate stack.
 */
static void test_altstack(void) {

    static const char* const codes[] = {
        [2]  = "sigaltstack() failed",
        [3]  = "an SA_ONSTACK handler did not run on the alternate stack",
        [4]  = "sigaltstack() did not report SS_ONSTACK inside the handler",
        [5]  = "sigaltstack() did not read back the stack that was set",
        [6]  = "sigaltstack(SS_DISABLE) failed",
        [7]  = "a handler ran on a disabled alternate stack",
        [8]  = "a stack below MINSIGSTKSZ was not refused with ENOMEM",
        [10] = "setup failed",
    };

    int r = run_child(alt_child, 3000);

    CHECK(r == 0, "altstack", "an alternate signal stack %s", child_result(r, codes, sizeof(codes) / sizeof(codes[0])));
}


/**
 * @brief Where segv_handler() jumps back to.
 */
static sigjmp_buf segv_env;


/**
 * @brief Records where it runs and jumps out of the fault.
 *
 * @param signo The signal.
 */
static void segv_handler(int signo) {

    volatile char here;

    (void)signo;

    alt_where = (uintptr_t)&here;

    siglongjmp(segv_env, 1);
}


/**
 * @brief Faults twice with an SA_ONSTACK SIGSEGV handler that jumps out, checking it ran on the alternate stack.
 *
 * @return 0 on success, or a failure code.
 */
static int segv_child(void) {

    stack_t ss = {.ss_sp = alt_stack, .ss_size = sizeof(alt_stack), .ss_flags = 0};

    if (sigaltstack(&ss, NULL) < 0)
        return 2;


    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = segv_handler;
    sa.sa_flags   = SA_ONSTACK;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGSEGV, &sa, NULL) < 0)
        return 10;

    for (int round = 0; round < 2; round++) {

        alt_where = 0;

        if (sigsetjmp(segv_env, 1) == 0) {

            *(volatile int*)(uintptr_t)8 = 1;

            return 3;
        }

        if (!on_alt_stack(alt_where))
            return 4;
    }

    return 0;
}


/**
 * @brief Checks that a fault handled on the alternate stack can be left with siglongjmp() and taken again.
 */
static void test_altstack_segv(void) {

    static const char* const codes[] = {
        [2]  = "sigaltstack() failed",
        [3]  = "the faulting write did not fault",
        [4]  = "the SIGSEGV handler did not run on the alternate stack",
        [10] = "setup failed",
    };

    int r = run_child(segv_child, 3000);

    CHECK(r == 0, "altstack-segv", "a SIGSEGV handled on the alternate stack %s", child_result(r, codes, sizeof(codes) / sizeof(codes[0])));
}


/**
 * @brief How long fpu_work() runs, read on every call so that the compiler cannot fold the calls together.
 */
static volatile int fpu_terms = 200000;
static volatile sig_atomic_t fpu_hits;
static volatile int fpu_stop;


/**
 * @brief Sums a long series, which comes out bit for bit the same every time unless the FPU state is disturbed.
 *
 * @return The sum.
 */
static double fpu_work(void) {

    double s = 0.0;

    for (int i = 1; i < fpu_terms; i++)
        s += 1.0 / ((double)i * 1.0000001 + 0.5);

    return s;
}


/**
 * @brief Changes the rounding mode and does floating-point work of its own, restoring nothing.
 *
 * @param signo The signal.
 */
static void fpu_handler(int signo) {

    volatile double x = 0.0;

    (void)signo;

    fesetround(FE_TOWARDZERO);

    for (int i = 1; i < 1000; i++)
        x += 3.3 / (double)i;

    fpu_hits++;
}


/**
 * @brief Signals the main thread every few hundred microseconds until told to stop.
 *
 * @param arg The main thread.
 * @return NULL.
 */
static void* fpu_sender(void* arg) {

    pthread_t target = *(pthread_t*)arg;

    while (!fpu_stop) {

        struct timespec ts = {.tv_sec = 0, .tv_nsec = 200000L};

        pthread_kill(target, SIGUSR1);
        nanosleep(&ts, NULL);
    }

    return NULL;
}


/**
 * @brief Repeats fpu_work() while fpu_handler() keeps interrupting it, comparing every result with an undisturbed one.
 *
 * @return 0 on success, or a failure code.
 */
static int fpu_child(void) {

    double want = fpu_work();


    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = fpu_handler;
    sa.sa_flags   = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) < 0 || unblock_all() < 0)
        return 10;


    pthread_t self = pthread_self();
    pthread_t t;

    if (pthread_create(&t, NULL, fpu_sender, &self) != 0)
        return 10;

    int bad = 0;

    for (uint64_t end = now_ms() + 500; now_ms() < end;)
        bad += fpu_work() != want;

    fpu_stop = 1;
    pthread_join(t, NULL);

    if (fpu_hits < 10)
        return 2;

    return bad ? 3 : 0;
}


/**
 * @brief Checks that a handler's floating-point work and rounding mode do not leak into the code it interrupts.
 */
static void test_fpu_preserved(void) {

    static const char* const codes[] = {
        [2]  = "too few signals arrived to tell",
        [3]  = "the interrupted computation came out different",
        [10] = "setup failed",
    };

    int r = run_child(fpu_child, 5000);

    CHECK(r == 0, "fpu-preserved", "floating-point work under interruption %s", child_result(r, codes, sizeof(codes) / sizeof(codes[0])));
}


#define ONCE_THREADS 3
#define ONCE_ROUNDS  20

/**
 * @brief How often once_handler() ran, and when its threads should stop.
 */
static atomic_int once_count;
static volatile int once_stop;


/**
 * @brief Counts a delivery.
 *
 * @param signo The signal.
 */
static void once_handler(int signo) {

    (void)signo;

    atomic_fetch_add(&once_count, 1);
}


/**
 * @brief Idles until told to stop.
 *
 * @param arg Unused.
 * @return NULL.
 */
static void* once_thread(void* arg) {

    (void)arg;

    while (!once_stop) {

        struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000L};

        nanosleep(&ts, NULL);
    }

    return NULL;
}


/**
 * @brief Sends a multithreaded process signals addressed to the whole process, and counts how often the handler ran.
 *
 * @return 0 on success, or a failure code.
 */
static int once_child(void) {

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = once_handler;
    sa.sa_flags   = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) < 0 || unblock_all() < 0)
        return 10;


    pthread_t t[ONCE_THREADS];

    for (int i = 0; i < ONCE_THREADS; i++) {

        if (pthread_create(&t[i], NULL, once_thread, NULL) != 0)
            return 10;
    }

    sleep_full_ms(50);

    for (int i = 0; i < ONCE_ROUNDS; i++) {

        kill(getpid(), SIGUSR1);

        for (uint64_t end = now_ms() + 500; atomic_load(&once_count) <= i && now_ms() < end;)
            sleep_full_ms(1);
    }

    sleep_full_ms(100);

    once_stop = 1;

    for (int i = 0; i < ONCE_THREADS; i++)
        pthread_join(t[i], NULL);


    int n = atomic_load(&once_count);

    if (n != ONCE_ROUNDS) {

        printf("signal-test: kill-once: the handler ran %d times for %d signals\n", n, ONCE_ROUNDS);
        return 2;
    }

    return 0;
}


/**
 * @brief Checks that a signal sent to a process runs its handler once, not once per thread.
 */
static void test_kill_once(void) {

    static const char* const codes[] = {
        [2]  = "the handler did not run exactly once per signal",
        [10] = "setup failed",
    };

    int r = run_child(once_child, 10000);

    CHECK(r == 0, "kill-once", "kill() on a %d-thread process %s", ONCE_THREADS + 1, child_result(r, codes, sizeof(codes) / sizeof(codes[0])));
}


/**
 * @brief How often pick_handler() ran and on which thread, and the one thread that does not block its signal.
 */
static atomic_int pick_count;
static atomic_int pick_wrong;
static volatile pid_t pick_tid;
static volatile int pick_ready;
static volatile int pick_stop;


/**
 * @brief Counts a delivery, and whether it landed on a thread other than the chosen one.
 *
 * @param signo The signal.
 */
static void pick_handler(int signo) {

    (void)signo;

    atomic_fetch_add(&pick_count, 1);

    if ((pid_t)syscall(SYS_gettid) != pick_tid)
        atomic_fetch_add(&pick_wrong, 1);
}


/**
 * @brief Idles with SIGUSR2 blocked, unless it is the chosen thread, and unblocks it once told to stop.
 *
 * @param arg Non-NULL for the chosen thread.
 * @return NULL.
 */
static void* pick_thread(void* arg) {

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);

    if (arg) {

        pick_tid = (pid_t)syscall(SYS_gettid);

        pthread_sigmask(SIG_UNBLOCK, &set, NULL);

        pick_ready = 1;
    }

    while (!pick_stop) {

        struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000L};

        nanosleep(&ts, NULL);
    }

    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    sleep_full_ms(50);

    return NULL;
}


/**
 * @brief Sends a process signals only one of its threads accepts, then lets every thread accept them.
 *
 * @return 0 on success, or a failure code.
 */
static int pick_child(void) {

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = pick_handler;
    sa.sa_flags   = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR2, &sa, NULL) < 0)
        return 10;


    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);

    if (sigprocmask_retry(SIG_SETMASK, &set, NULL) < 0)
        return 10;


    pthread_t t[3];

    for (int i = 0; i < 3; i++) {

        if (pthread_create(&t[i], NULL, pick_thread, i == 1 ? (void*)1 : NULL) != 0)
            return 10;
    }

    for (uint64_t end = now_ms() + 1000; !pick_ready && now_ms() < end;)
        sleep_full_ms(1);

    for (int i = 0; i < 10; i++) {

        kill(getpid(), SIGUSR2);

        for (uint64_t end = now_ms() + 500; atomic_load(&pick_count) <= i && now_ms() < end;)
            sleep_full_ms(1);
    }

    pick_stop = 1;

    sigprocmask_retry(SIG_UNBLOCK, &set, NULL);
    sleep_full_ms(100);

    for (int i = 0; i < 3; i++)
        pthread_join(t[i], NULL);


    int n     = atomic_load(&pick_count);
    int wrong = atomic_load(&pick_wrong);

    if (n != 10 || wrong != 0) {

        printf("signal-test: kill-picks-unblocked: the handler ran %d times for 10 signals, %d of them on a thread that blocked it\n", n, wrong);
        return 2;
    }

    return 0;
}


/**
 * @brief Checks that a signal sent to a process goes to the one thread that does not block it, and only there.
 */
static void test_kill_picks_unblocked(void) {

    static const char* const codes[] = {
        [2]  = "the signals did not all go to the one thread accepting them",
        [10] = "setup failed",
    };

    int r = run_child(pick_child, 10000);

    CHECK(r == 0, "kill-picks-unblocked", "kill() on a process where one thread accepts the signal %s", child_result(r, codes, sizeof(codes) / sizeof(codes[0])));
}


/**
 * @brief Waits forever, with SIGTERM blocked unless told otherwise.
 *
 * @param arg Non-NULL to unblock SIGTERM.
 * @return Never.
 */
static void* fatal_thread(void* arg) {

    if (arg) {

        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGTERM);

        pthread_sigmask(SIG_UNBLOCK, &set, NULL);
    }

    for (;;)
        pause();

    return NULL;
}


/**
 * @brief Starts threads that all block SIGTERM but one, and waits forever.
 *
 * @return Never, unless setup fails.
 */
static int fatal_child(void) {

    signal(SIGTERM, SIG_DFL);


    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGTERM);

    if (sigprocmask_retry(SIG_SETMASK, &set, NULL) < 0)
        return 10;

    for (int i = 0; i < 3; i++) {

        pthread_t t;

        if (pthread_create(&t, NULL, fatal_thread, i == 2 ? (void*)1 : NULL) != 0)
            return 10;
    }

    for (;;)
        pause();

    return 0;
}


/**
 * @brief Checks that a fatal signal taken by one thread terminates the whole process, threads blocking it included.
 */
static void test_kill_fatal(void) {

    pid_t pid = fork();

    if (pid < 0) {
        CHECK(0, "kill-fatal", "fork() failed: %s", strerror(errno));
        return;
    }

    if (pid == 0)
        _exit(fatal_child());


    sleep_full_ms(200);

    kill(pid, SIGTERM);


    int status = 0;

    if (wait_timeout(pid, &status, 0, 2000) != pid) {

        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);

        CHECK(0, "kill-fatal", "%s", "SIGTERM left the process running, with only the thread that took it gone");
        return;
    }

    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM, "kill-fatal", "the process ended with status 0x%x, expected death by SIGTERM", status);
}


/**
 * @brief Writes a byte to a pipe every few milliseconds, forever.
 *
 * @param arg The descriptor to write to.
 * @return Never.
 */
static void* tick_thread(void* arg) {

    int fd = (int)(intptr_t)arg;

    for (;;) {

        write(fd, "t", 1);
        sleep_full_ms(5);
    }

    return NULL;
}


/**
 * @brief Reads everything waiting in a non-blocking pipe.
 *
 * @param fd The read end.
 * @return How many bytes were read.
 */
static int drain(int fd) {

    char buf[256];
    int total = 0;

    for (ssize_t n; (n = read(fd, buf, sizeof(buf))) > 0;)
        total += (int)n;

    return total;
}


/**
 * @brief Checks that SIGSTOP stops every thread of a process and SIGCONT resumes them all.
 */
static void test_kill_stop(void) {

    int p[2];

    if (pipe(p) < 0) {
        CHECK(0, "kill-stop", "pipe() failed: %s", strerror(errno));
        return;
    }

    pid_t pid = fork();

    if (pid < 0) {
        CHECK(0, "kill-stop", "fork() failed: %s", strerror(errno));
        return;
    }

    if (pid == 0) {

        close(p[0]);

        for (int i = 0; i < 3; i++) {

            pthread_t t;

            pthread_create(&t, NULL, tick_thread, (void*)(intptr_t)p[1]);
        }

        tick_thread((void*)(intptr_t)p[1]);
        _exit(0);
    }

    close(p[1]);
    fcntl(p[0], F_SETFL, O_NONBLOCK);

    sleep_full_ms(200);

    kill(pid, SIGSTOP);


    int status  = 0;
    int stopped = wait_timeout(pid, &status, WUNTRACED, 2000) == pid && WIFSTOPPED(status);

    sleep_full_ms(50);
    drain(p[0]);
    sleep_full_ms(200);

    int during = drain(p[0]);

    kill(pid, SIGCONT);
    sleep_full_ms(200);

    int after = drain(p[0]);

    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    close(p[0]);

    CHECK(stopped, "kill-stop", "%s", "waitpid(WUNTRACED) never reported the stop");
    CHECK(during == 0, "kill-stop-threads", "%d bytes were written by threads that should have been stopped", during);
    CHECK(after > 0, "kill-stop-cont", "%s", "no thread ran again after SIGCONT");
}


/**
 * @brief Checks that FUTEX_WAIT gives up with ETIMEDOUT once its timeout passes.
 *
 * @return 0 on success, or a failure code.
 */
static int futex_timeout_child(void) {

    uint32_t word      = 0;
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 50000000L};

    uint64_t t0 = now_ms();
    long r      = syscall(SYS_futex, &word, FUTEX_OP_WAIT, 0, &ts, NULL, 0);
    uint64_t dt = now_ms() - t0;

    if (r == 0)
        return 2;

    if (errno != ETIMEDOUT)
        return 3;

    if (dt < 45)
        return 4;

    return 0;
}


/**
 * @brief Checks the timeout of FUTEX_WAIT.
 */
static void test_futex_timeout(void) {

    static const char* const codes[] = {
        [2] = "FUTEX_WAIT reported a wakeup nobody sent when its timeout passed",
        [3] = "FUTEX_WAIT failed with an error other than ETIMEDOUT",
        [4] = "FUTEX_WAIT returned before its timeout",
    };

    int r = run_child(futex_timeout_child, 3000);

    CHECK(r == 0, "futex-timeout", "a 50 ms FUTEX_WAIT %s", child_result(r, codes, sizeof(codes) / sizeof(codes[0])));
}


/**
 * @brief The word futex_wait_thread() waits on, and what its FUTEX_WAIT returned.
 */
static uint32_t fw_word;
static volatile long fw_ret = 1;
static volatile int fw_errno;


/**
 * @brief Waits on fw_word with no timeout and records the outcome.
 *
 * @param arg Unused.
 * @return NULL.
 */
static void* futex_wait_thread(void* arg) {

    (void)arg;

    long r = syscall(SYS_futex, &fw_word, FUTEX_OP_WAIT, 0, NULL, NULL, 0);

    fw_errno = errno;
    fw_ret   = r;

    return NULL;
}


/**
 * @brief Does nothing, so that the signal it handles only interrupts.
 *
 * @param signo The signal.
 */
static void noop_handler(int signo) {
    (void)signo;
}


/**
 * @brief Interrupts a thread blocked in FUTEX_WAIT with a handler installed without SA_RESTART.
 *
 * @return 0 on success, or a failure code.
 */
static int futex_eintr_child(void) {

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = noop_handler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) < 0 || unblock_all() < 0)
        return 10;


    pthread_t t;

    if (pthread_create(&t, NULL, futex_wait_thread, NULL) != 0)
        return 10;

    sleep_full_ms(100);

    pthread_kill(t, SIGUSR1);

    for (uint64_t end = now_ms() + 500; fw_ret == 1 && now_ms() < end;)
        sleep_full_ms(1);

    if (fw_ret == 1) {

        fw_word = 1;
        syscall(SYS_futex, &fw_word, FUTEX_OP_WAKE, 1, NULL, NULL, 0);

        pthread_join(t, NULL);
        return 2;
    }

    pthread_join(t, NULL);

    if (fw_ret == 0)
        return 3;

    return fw_errno == EINTR ? 0 : 4;
}


/**
 * @brief Checks that a signal handled during FUTEX_WAIT makes it fail with EINTR.
 */
static void test_futex_eintr(void) {

    static const char* const codes[] = {
        [2]  = "FUTEX_WAIT did not return after the handler ran",
        [3]  = "FUTEX_WAIT reported a wakeup nobody sent",
        [4]  = "FUTEX_WAIT failed with an error other than EINTR",
        [10] = "setup failed",
    };

    int r = run_child(futex_eintr_child, 3000);

    CHECK(r == 0, "futex-eintr", "a signal during FUTEX_WAIT %s", child_result(r, codes, sizeof(codes) / sizeof(codes[0])));
}


/**
 * @brief Wakes a thread blocked in FUTEX_WAIT.
 *
 * @return 0 on success, or a failure code.
 */
static int futex_wake_child(void) {

    pthread_t t;

    if (pthread_create(&t, NULL, futex_wait_thread, NULL) != 0)
        return 10;

    sleep_full_ms(100);

    long n = syscall(SYS_futex, &fw_word, FUTEX_OP_WAKE, 1, NULL, NULL, 0);

    for (uint64_t end = now_ms() + 500; fw_ret == 1 && now_ms() < end;)
        sleep_full_ms(1);

    if (fw_ret == 1)
        return 2;

    pthread_join(t, NULL);

    if (n != 1)
        return 3;

    return fw_ret == 0 ? 0 : 4;
}


/**
 * @brief Checks that FUTEX_WAKE releases a waiter, which returns 0.
 */
static void test_futex_wake(void) {

    static const char* const codes[] = {
        [2]  = "the waiter never returned",
        [3]  = "FUTEX_WAKE did not report one waiter woken",
        [4]  = "the woken FUTEX_WAIT failed",
        [10] = "setup failed",
    };

    int r = run_child(futex_wake_child, 3000);

    CHECK(r == 0, "futex-wake", "FUTEX_WAKE on a waiter %s", child_result(r, codes, sizeof(codes) / sizeof(codes[0])));
}


/**
 * @brief The two words of the requeue case, and whether its waiter has returned.
 */
static uint32_t rq_from;
static uint32_t rq_to = 1;
static volatile long rq_ret;
static volatile int rq_done;


/**
 * @brief Waits on rq_from and records the outcome.
 *
 * @param arg Unused.
 * @return NULL.
 */
static void* requeue_thread(void* arg) {

    (void)arg;

    rq_ret  = syscall(SYS_futex, &rq_from, FUTEX_OP_WAIT, 0, NULL, NULL, 0);
    rq_done = 1;

    return NULL;
}


/**
 * @brief Moves a waiter to a word holding a different value, checks it stays asleep, then wakes it there.
 *
 * @return 0 on success, or a failure code.
 */
static int futex_requeue_child(void) {

    pthread_t t;

    if (pthread_create(&t, NULL, requeue_thread, NULL) != 0)
        return 10;

    sleep_full_ms(100);

    long n = syscall(SYS_futex, &rq_from, FUTEX_OP_CMP_REQUEUE, 0, 1L, &rq_to, 0);

    if (n != 1)
        return 2;

    sleep_full_ms(100);

    if (rq_done)
        return 3;


    long w = syscall(SYS_futex, &rq_to, FUTEX_OP_WAKE, 1, NULL, NULL, 0);

    for (uint64_t end = now_ms() + 500; !rq_done && now_ms() < end;)
        sleep_full_ms(1);

    if (!rq_done)
        return 4;

    pthread_join(t, NULL);

    if (w != 1)
        return 5;

    return rq_ret == 0 ? 0 : 6;
}


/**
 * @brief Checks that FUTEX_CMP_REQUEUE moves a waiter without waking it, and that it can then be woken where it went.
 */
static void test_futex_requeue(void) {

    static const char* const codes[] = {
        [2]  = "FUTEX_CMP_REQUEUE did not report one waiter moved",
        [3]  = "the moved waiter woke up without being woken",
        [4]  = "the moved waiter never returned after FUTEX_WAKE on its new word",
        [5]  = "FUTEX_WAKE on the new word did not find the moved waiter",
        [6]  = "the woken FUTEX_WAIT failed",
        [10] = "setup failed",
    };

    int r = run_child(futex_requeue_child, 3000);

    CHECK(r == 0, "futex-requeue", "requeueing a waiter %s", child_result(r, codes, sizeof(codes) / sizeof(codes[0])));
}


/**
 * @brief Tries to take a priority-inheritance lock that another thread's tid holds.
 *
 * @return 0 unless the lock was reported taken.
 */
static int futex_lock_pi_child(void) {

    uint32_t word = (uint32_t)(getpid() + 100000) & 0x3FFFFFFF;

    return syscall(SYS_futex, &word, FUTEX_OP_LOCK_PI, 0, NULL, NULL, 0) == 0 ? 2 : 0;
}


/**
 * @brief Checks that FUTEX_LOCK_PI never claims a lock another thread holds, by blocking or by failing.
 */
static void test_futex_lock_pi(void) {

    static const char* const codes[] = {
        [2] = "FUTEX_LOCK_PI reported success on a lock another thread holds",
    };

    int r = run_child(futex_lock_pi_child, 500);

    CHECK(r == 0 || r == -1, "futex-lock-pi", "FUTEX_LOCK_PI %s", child_result(r, codes, sizeof(codes) / sizeof(codes[0])));
}


/**
 * @brief Filled in by info_handler() from the siginfo of the last SIGUSR2.
 */
static volatile sig_atomic_t info_caught;
static volatile sig_atomic_t info_code;
static volatile sig_atomic_t info_pid;
static volatile sig_atomic_t info_uid;
static volatile sig_atomic_t info_value;


/**
 * @brief Records what a signal's siginfo carried.
 *
 * @param signo The signal.
 * @param si Its siginfo.
 * @param uc Unused.
 */
static void info_handler(int signo, siginfo_t* si, void* uc) {

    (void)signo;
    (void)uc;

    info_code  = si->si_code;
    info_pid   = si->si_pid;
    info_uid   = (sig_atomic_t)si->si_uid;
    info_value = si->si_value.sival_int;
    info_caught++;
}


/**
 * @brief Installs info_handler() for SIGUSR2 and forgets what it last recorded.
 *
 * @return 0 on success, or -1 with errno set.
 */
static int catch_info(void) {

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_sigaction = info_handler;
    sa.sa_flags     = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    info_caught = 0;
    info_code   = 0;
    info_pid    = 0;
    info_uid    = -1;
    info_value  = 0;

    return sigaction(SIGUSR2, &sa, NULL);
}


/**
 * @brief Waits for info_handler() to run.
 *
 * @param ms How long to wait.
 * @return 1 if it ran, 0 otherwise.
 */
static int wait_info(unsigned ms) {

    for (unsigned i = 0; i < ms && !info_caught; i += 5)
        sleep_ms(5);

    return info_caught != 0;
}


/**
 * @brief Forks a child that catches SIGUSR2 with info_handler() and waits for it.
 *
 * @param code The si_code the child expects.
 * @param value The si_value the child expects, or 0 not to check it.
 * @return The child's pid once it is ready, or -1.
 */
static pid_t fork_info_child(int code, int value) {

    int p[2];

    if (pipe(p) < 0)
        return -1;

    pid_t pid = fork();

    if (pid == 0) {

        close(p[0]);

        if (catch_info() < 0)
            _exit(10);

        char c = 1;
        write(p[1], &c, 1);

        if (!wait_info(2000))
            _exit(1);

        if (info_code != code)
            _exit(2);

        if (info_pid != getppid())
            _exit(3);

        if (info_uid != (sig_atomic_t)getuid())
            _exit(4);

        if (value && info_value != value)
            _exit(5);

        _exit(0);
    }

    close(p[1]);

    char c    = 0;
    ssize_t n = pid > 0 ? read(p[0], &c, 1) : -1;

    close(p[0]);

    return n == 1 ? pid : -1;
}


static const char* const info_codes[] = {
    [1]  = "the signal never arrived",
    [2]  = "si_code is wrong",
    [3]  = "si_pid is not the sender's",
    [4]  = "si_uid is not the sender's",
    [5]  = "si_value is wrong",
    [10] = "sigaction() failed",
};


/**
 * @brief Sends a signal to another process's thread with tkill(), which must arrive carrying the sender's pid and uid.
 */
static void test_tkill_other(void) {

    pid_t pid = fork_info_child(SI_TKILL, 0);

    if (pid < 0) {
        CHECK(0, "tkill-other", "starting the child failed: %s", strerror(errno));
        return;
    }

    errno  = 0;
    long r = syscall(SYS_tkill, pid, SIGUSR2);
    int e  = errno;

    int c = reap_child(pid, 3000);

    CHECK(r == 0 && c == 0, "tkill-other", "tkill() returned %ld (%s), and the child %s", r, r < 0 ? strerror(e) : "ok", c == 0 ? "got it" : child_result(c, info_codes, sizeof(info_codes) / sizeof(info_codes[0])));
}


/**
 * @brief Checks that tkill() refuses thread ids that name no single thread.
 */
static void test_tkill_invalid(void) {

    if (catch_info() < 0) {
        CHECK(0, "tkill-invalid", "sigaction() failed: %s", strerror(errno));
        return;
    }

    errno  = 0;
    long z = syscall(SYS_tkill, 0, SIGUSR2);
    int ze = errno;

    errno  = 0;
    long n = syscall(SYS_tkill, -1, SIGUSR2);
    int ne = errno;

    sleep_ms(50);

    CHECK(z < 0 && ze == EINVAL && n < 0 && ne == EINVAL, "tkill-invalid", "tkill(0) returned %ld (%s) and tkill(-1) returned %ld (%s)", z, z < 0 ? strerror(ze) : "ok", n, n < 0 ? strerror(ne) : "ok");
}


/**
 * @brief Checks that tkill() and tgkill() refuse a signal number past the last one.
 */
static void test_tkill_bad_signal(void) {

    pid_t tid = (pid_t)syscall(SYS_gettid);

    errno  = 0;
    long t = syscall(SYS_tkill, tid, _NSIG + 35);
    int te = errno;

    errno  = 0;
    long g = syscall(SYS_tgkill, getpid(), tid, _NSIG + 35);
    int ge = errno;

    CHECK(t < 0 && te == EINVAL && g < 0 && ge == EINVAL, "tkill-bad-signal", "tkill() returned %ld (%s) and tgkill() returned %ld (%s)", t, t < 0 ? strerror(te) : "ok", g, g < 0 ? strerror(ge) : "ok");
}


/**
 * @brief Sends the caller a signal with tgkill(), then names a thread outside the group given.
 */
static void test_tgkill(void) {

    if (catch_info() < 0) {
        CHECK(0, "tgkill", "sigaction() failed: %s", strerror(errno));
        return;
    }

    pid_t tid = (pid_t)syscall(SYS_gettid);

    errno  = 0;
    long r = syscall(SYS_tgkill, getpid(), tid, SIGUSR2);
    int e  = errno;

    int got = r == 0 && wait_info(500);

    CHECK(got && info_code == SI_TKILL && info_pid == getpid(), "tgkill", "tgkill() on the caller returned %ld (%s); handler ran %d, si_code %d, si_pid %d", r, r < 0 ? strerror(e) : "ok", (int)info_caught, (int)info_code, (int)info_pid);


    errno  = 0;
    long w = syscall(SYS_tgkill, getppid(), tid, SIGUSR2);
    e      = errno;

    CHECK(w < 0 && e == ESRCH, "tgkill-wrong-group", "tgkill() naming a thread outside the group returned %ld (%s)", w, w < 0 ? strerror(e) : "ok");
}


/**
 * @brief Queues a signal with a value on another process, then on pid 0, which names no process.
 */
static void test_sigqueue_other(void) {

    pid_t pid = fork_info_child(SI_QUEUE, 4242);

    if (pid < 0) {
        CHECK(0, "sigqueue-other", "starting the child failed: %s", strerror(errno));
        return;
    }

    errno = 0;
    int r = sigqueue(pid, SIGUSR2, (union sigval){.sival_int = 4242});
    int e = errno;

    int c = reap_child(pid, 3000);

    CHECK(r == 0 && c == 0, "sigqueue-other", "sigqueue() returned %d (%s), and the child %s", r, r < 0 ? strerror(e) : "ok", c == 0 ? "got it" : child_result(c, info_codes, sizeof(info_codes) / sizeof(info_codes[0])));


    errno = 0;
    int z = sigqueue(0, SIGUSR2, (union sigval){.sival_int = 1});
    e     = errno;

    CHECK(z < 0 && e == ESRCH, "sigqueue-bad-pid", "sigqueue(0) returned %d (%s)", z, z < 0 ? strerror(e) : "ok");
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
    {"nested", test_nested},
    {"nested-other", test_nested_other},
    {"thread-frames", test_thread_frames},
    {"ucontext", test_ucontext},
    {"ucontext-resume", test_ucontext_resume},
    {"pthread-cancel-read", test_cancel_read},
    {"pthread-cancel-sleep", test_cancel_sleep},
    {"altstack", test_altstack},
    {"altstack-segv", test_altstack_segv},
    {"fpu-preserved", test_fpu_preserved},
    {"kill-once", test_kill_once},
    {"kill-picks-unblocked", test_kill_picks_unblocked},
    {"kill-fatal", test_kill_fatal},
    {"kill-stop", test_kill_stop},
    {"futex-timeout", test_futex_timeout},
    {"futex-eintr", test_futex_eintr},
    {"futex-wake", test_futex_wake},
    {"futex-requeue", test_futex_requeue},
    {"futex-lock-pi", test_futex_lock_pi},
    {"tkill-other", test_tkill_other},
    {"tkill-invalid", test_tkill_invalid},
    {"tgkill", test_tgkill},
    {"tkill-bad-signal", test_tkill_bad_signal},
    {"sigqueue-other", test_sigqueue_other},
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
