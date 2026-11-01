#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


static int failures = 0;
static int total    = 0;

static const char* self = "/usr/bin/vfork-test";

static volatile int shared;
static volatile sig_atomic_t caught;


static long now_ms(void) {

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}


static void sleep_ms(long ms) {

    struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};

    while (nanosleep(&ts, &ts) < 0 && errno == EINTR)
        ;
}


static int reap(pid_t pid) {

    int status;

    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }

    return status;
}


static int exit_status(void) {

    pid_t pid = vfork();

    if (pid == 0)
        _exit(42);

    if (pid < 0)
        return 1;

    int status = reap(pid);

    return WIFEXITED(status) && WEXITSTATUS(status) == 42 ? 0 : 2;
}


static int shares_memory(void) {

    shared = 0;

    pid_t pid = vfork();

    if (pid == 0) {
        shared = 1234;
        _exit(0);
    }

    if (pid < 0)
        return 1;

    int seen = shared;

    reap(pid);

    return seen == 1234 ? 0 : 2;
}


static int parent_waits_for_exit(void) {

    shared = 0;

    long start = now_ms();

    pid_t pid = vfork();

    if (pid == 0) {
        sleep_ms(200);
        shared = 1;
        _exit(0);
    }

    if (pid < 0)
        return 1;

    long waited = now_ms() - start;
    int seen    = shared;

    reap(pid);

    if (seen != 1)
        return 2;

    return waited >= 150 ? 0 : 3;
}


static int clone_child(void* arg) {

    (void)arg;

    return 6;
}


static int clone_ignores_unused_tids(void) {

    void* const ptid = (void*)0x3UL;
    void* const ctid = (void*)0xFFFFFFFFUL;

    long pid = syscall(SYS_clone, (unsigned long)SIGCHLD, NULL, ptid, ctid, 0UL);

    if (pid == 0)
        _exit(5);

    if (pid < 0)
        return 1;

    int status = reap((pid_t)pid);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 5)
        return 2;

    static char stack[16384] __attribute__((aligned(16)));

    pid = clone(clone_child, stack + sizeof(stack), CLONE_VM | CLONE_VFORK | SIGCHLD, NULL, ptid, NULL, ctid);

    if (pid < 0)
        return 3;

    status = reap((pid_t)pid);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 6)
        return 4;

    errno = 0;
    pid   = syscall(SYS_clone, (unsigned long)(CLONE_PARENT_SETTID | SIGCHLD), NULL, ptid, NULL, 0UL);

    if (pid == 0)
        _exit(0);

    if (pid > 0) {
        reap((pid_t)pid);
        return 5;
    }

    return errno == EFAULT ? 0 : 6;
}


static int exec_releases_parent(void) {

    long start = now_ms();

    pid_t pid = vfork();

    if (pid == 0) {
        execl(self, self, "--sleep-exit", "500", "7", (char*)NULL);
        _exit(127);
    }

    if (pid < 0)
        return 1;

    long waited = now_ms() - start;
    int status  = reap(pid);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 7)
        return 2;

    return waited < 400 ? 0 : 3;
}


static int exec_failure_reported(void) {

    shared = 0;

    pid_t pid = vfork();

    if (pid == 0) {
        execl("/nonexistent/vfork-test", "vfork-test", (char*)NULL);
        shared = errno;
        _exit(127);
    }

    if (pid < 0)
        return 1;

    int seen   = shared;
    int status = reap(pid);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 127)
        return 2;

    return seen == ENOENT ? 0 : 3;
}


static void on_usr1(int signo) {

    (void)signo;
    caught++;
}


static int signal_held_while_parked(void) {

    struct sigaction sa = {0};
    sa.sa_handler       = on_usr1;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) < 0)
        return 1;

    caught = 0;
    shared = 0;

    pid_t pid = vfork();

    if (pid == 0) {
        kill(getppid(), SIGUSR1);
        sleep_ms(100);
        shared = caught ? 2 : 1;
        _exit(0);
    }

    if (pid < 0)
        return 2;

    int seen   = shared;
    int status = reap(pid);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return 3;

    if (seen != 1)
        return 4;

    return caught == 1 ? 0 : 5;
}


static int kill_parked_parent(void) {

    int fds[2];

    if (pipe(fds) < 0)
        return 1;

    pid_t parent = fork();

    if (parent == 0) {

        pid_t pid = vfork();

        if (pid == 0) {
            pid_t me = getpid();
            write(fds[1], &me, sizeof(me));
            sleep_ms(500);
            _exit(0);
        }

        _exit(5);
    }

    if (parent < 0)
        return 2;

    pid_t child;

    if (read(fds[0], &child, sizeof(child)) != sizeof(child))
        return 3;

    long start = now_ms();

    kill(parent, SIGKILL);

    int status = reap(parent);
    long took  = now_ms() - start;

    if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL)
        return 4;

    if (took >= 400)
        return 5;

    for (int i = 0; i < 300; i++) {

        if (kill(child, 0) < 0 && errno == ESRCH)
            return 0;

        sleep_ms(10);
    }

    return 6;
}


static int many_in_a_row(void) {

    for (int i = 0; i < 200; i++) {

        pid_t pid = vfork();

        if (pid == 0)
            _exit(i & 0x7F);

        if (pid < 0)
            return 1;

        int status = reap(pid);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != (i & 0x7F))
            return 2;
    }

    return 0;
}


static void run(const char* name, int (*test)(void), long timeout_ms) {

    total++;

    pid_t runner = fork();

    if (runner == 0)
        _exit(test());

    if (runner < 0) {
        failures++;
        printf("vfork-test: FAIL  %s: fork: %s\n", name, strerror(errno));
        return;
    }

    long deadline = now_ms() + timeout_ms;
    int status;
    pid_t r;

    while ((r = waitpid(runner, &status, WNOHANG)) == 0 && now_ms() < deadline)
        sleep_ms(10);

    if (r == 0) {
        kill(runner, SIGKILL);
        reap(runner);
        failures++;
        printf("vfork-test: FAIL  %s: timed out after %ld ms\n", name, timeout_ms);
        return;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("vfork-test: PASS  %s\n", name);
        return;
    }

    failures++;

    if (WIFSIGNALED(status))
        printf("vfork-test: FAIL  %s: killed by signal %d\n", name, WTERMSIG(status));
    else
        printf("vfork-test: FAIL  %s: step %d\n", name, WEXITSTATUS(status));
}


int main(int argc, char** argv) {

    if (argc == 4 && strcmp(argv[1], "--sleep-exit") == 0) {
        sleep_ms(atol(argv[2]));
        return atoi(argv[3]);
    }

    if (argc > 0 && argv[0][0] == '/')
        self = argv[0];

    setvbuf(stdout, NULL, _IONBF, 0);

    run("exit-status", exit_status, 5000);
    run("shares-memory", shares_memory, 5000);
    run("parent-waits-for-exit", parent_waits_for_exit, 5000);
    run("exec-releases-parent", exec_releases_parent, 5000);
    run("exec-failure-reported", exec_failure_reported, 5000);
    run("signal-held-while-parked", signal_held_while_parked, 5000);
    run("kill-parked-parent", kill_parked_parent, 8000);
    run("many-in-a-row", many_in_a_row, 20000);
    run("clone-ignores-unused-tids", clone_ignores_unused_tids, 5000);

    printf("vfork-test: %d/%d passed\n", total - failures, total);

    return failures ? 1 : 0;
}
