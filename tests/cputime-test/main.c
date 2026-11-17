/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2026 Antonino Natale
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

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


#define SPIN_NS  400000000LL
#define SLACK_NS 60000000LL


static int failures = 0;
static int total    = 0;

static int release[2];
static int ready[2];


#define CHECK(cond, name, fmt, ...)                                           \
    {                                                                         \
        total++;                                                              \
        if (cond) {                                                           \
            printf("cputime-test: PASS  %s\n", (name));                       \
        } else {                                                              \
            failures++;                                                       \
            printf("cputime-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                     \
    }


static int64_t clock_ns(clockid_t id) {

    struct timespec ts;

    if (clock_gettime(id, &ts) < 0) {
        return -1;
    }

    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static int64_t spin(int64_t ns) {

    int64_t start = clock_ns(CLOCK_THREAD_CPUTIME_ID);

    while (clock_ns(CLOCK_THREAD_CPUTIME_ID) - start < ns) {

        for (volatile int i = 0; i < 1000000; i++) {
        }
    }

    return clock_ns(CLOCK_THREAD_CPUTIME_ID);
}

static void* spin_and_exit(void* arg) {

    *(int64_t*)arg = spin(SPIN_NS);

    return NULL;
}

static void* spin_and_wait(void* arg) {

    *(int64_t*)arg = spin(SPIN_NS);

    char c = 'r';

    if (write(ready[1], &c, 1) != 1) {
        return NULL;
    }

    while (read(release[0], &c, 1) < 0 && errno == EINTR) {
    }

    return NULL;
}

static int stat_times_ns(int64_t* utime_ns, int64_t* stime_ns) {

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", getpid());

    FILE* fp = fopen(path, "r");

    if (!fp) {
        return -1;
    }

    char buf[1024] = {0};
    size_t n       = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);

    buf[n] = '\0';

    char* p = strrchr(buf, ')');

    if (!p) {
        return -1;
    }

    unsigned long utime = 0;
    unsigned long stime = 0;
    p += 2;

    for (int field = 3; field < 14; field++) {

        p = strchr(p, ' ');

        if (!p) {
            return -1;
        }

        p++;
    }

    if (sscanf(p, "%lu %lu", &utime, &stime) != 2) {
        return -1;
    }

    *utime_ns = (int64_t)utime * (1000000000LL / sysconf(_SC_CLK_TCK));
    *stime_ns = (int64_t)stime * (1000000000LL / sysconf(_SC_CLK_TCK));

    return 0;
}

static void thread_times_ns(int64_t* user, int64_t* system) {

    struct rusage ru;
    getrusage(RUSAGE_THREAD, &ru);

    *user   = (int64_t)ru.ru_utime.tv_sec * 1000000000LL + (int64_t)ru.ru_utime.tv_usec * 1000LL;
    *system = (int64_t)ru.ru_stime.tv_sec * 1000000000LL + (int64_t)ru.ru_stime.tv_usec * 1000LL;
}

static void check_split(void) {

    int64_t u0, s0, u1, s1, u2, s2;

    thread_times_ns(&u0, &s0);
    spin(SPIN_NS);
    thread_times_ns(&u1, &s1);

    printf("cputime-test: user spin: user %lld ms, system %lld ms\n", (long long)((u1 - u0) / 1000000), (long long)((s1 - s0) / 1000000));

    CHECK(u1 - u0 >= SPIN_NS - SLACK_NS, "user spin is charged as user time", "user %lld", (long long)(u1 - u0));
    CHECK(s1 - s0 <= SLACK_NS, "user spin is not charged as system time", "system %lld", (long long)(s1 - s0));


    int fd = open("/dev/zero", O_RDONLY);
    CHECK(fd >= 0, "/dev/zero opens", "errno %d", errno);

    if (fd < 0) {
        return;
    }

    size_t size = 1 << 20;
    char* buf   = malloc(size);

    int64_t start = clock_ns(CLOCK_THREAD_CPUTIME_ID);

    while (clock_ns(CLOCK_THREAD_CPUTIME_ID) - start < SPIN_NS) {

        if (read(fd, buf, size) < 0) {
            break;
        }
    }

    close(fd);
    free(buf);

    thread_times_ns(&u2, &s2);

    printf("cputime-test: /dev/zero reads: user %lld ms, system %lld ms\n", (long long)((u2 - u1) / 1000000), (long long)((s2 - s1) / 1000000));

    CHECK(s2 - s1 >= (s2 - s1 + u2 - u1) / 2, "kernel copies are charged as system time", "system %lld, user %lld", (long long)(s2 - s1), (long long)(u2 - u1));
}


static void check_times(void) {

    int64_t tick = 1000000000LL / sysconf(_SC_CLK_TCK);

    struct tms t0;
    clock_t r0 = times(&t0);

    CHECK(r0 != (clock_t)-1, "times() succeeds", "errno %d", errno);

    int64_t process = clock_ns(CLOCK_PROCESS_CPUTIME_ID);
    int64_t own     = ((int64_t)t0.tms_utime + (int64_t)t0.tms_stime) * tick;

    CHECK(own <= process && own >= process - SLACK_NS, "times() matches the process clock", "times %lld, process %lld", (long long)own, (long long)process);
    CHECK((int64_t)t0.tms_stime * tick >= SPIN_NS / 2 - SLACK_NS, "times() reports system time", "tms_stime %ld", (long)t0.tms_stime);

    pid_t child = fork();

    if (child == 0) {
        spin(SPIN_NS);
        _exit(0);
    }

    int status = 0;
    waitpid(child, &status, 0);

    struct tms t1;
    clock_t r1 = times(&t1);

    CHECK((int64_t)(t1.tms_cutime - t0.tms_cutime) * tick >= SPIN_NS - SLACK_NS, "times() adds a reaped child's user time", "tms_cutime %ld -> %ld", (long)t0.tms_cutime, (long)t1.tms_cutime);
    CHECK((int64_t)(t1.tms_cstime - t0.tms_cstime) * tick <= SLACK_NS, "times() keeps a user-only child out of tms_cstime", "tms_cstime %ld -> %ld", (long)t0.tms_cstime, (long)t1.tms_cstime);
    CHECK((int64_t)(r1 - r0) * tick >= SPIN_NS - SLACK_NS, "times() return value advances with the clock", "%ld -> %ld", (long)r0, (long)r1);
}


int main(void) {

    if (pipe(release) < 0 || pipe(ready) < 0) {
        perror("cputime-test: pipe");
        return 1;
    }


    int64_t exited   = 0;
    int64_t alive[2] = {0};

    pthread_t t;

    if (pthread_create(&t, NULL, spin_and_exit, &exited) != 0 || pthread_join(t, NULL) != 0) {
        perror("cputime-test: exited thread");
        return 1;
    }


    pthread_t waiters[2];

    for (int i = 0; i < 2; i++) {

        if (pthread_create(&waiters[i], NULL, spin_and_wait, &alive[i]) != 0) {
            perror("cputime-test: waiting thread");
            return 1;
        }
    }

    for (int i = 0; i < 2; i++) {

        char c;

        while (read(ready[0], &c, 1) < 0 && errno == EINTR) {
        }
    }

    usleep(50000);


    int64_t self    = clock_ns(CLOCK_THREAD_CPUTIME_ID);
    int64_t process = clock_ns(CLOCK_PROCESS_CPUTIME_ID);
    int64_t utime   = -1;
    int64_t stime   = -1;

    stat_times_ns(&utime, &stime);
    int64_t threads = exited + alive[0] + alive[1];

    printf("cputime-test: main %lld ms, threads %lld ms, process %lld ms, /proc stat %lld+%lld ms\n", (long long)(self / 1000000), (long long)(threads / 1000000), (long long)(process / 1000000), (long long)(utime / 1000000),
           (long long)(stime / 1000000));


    CHECK(process >= threads, "process clock covers every thread", "process %lld < threads %lld", (long long)process, (long long)threads);
    CHECK(process <= threads + self + SLACK_NS, "process clock is not inflated", "process %lld > %lld", (long long)process, (long long)(threads + self + SLACK_NS));
    CHECK(process >= exited + self, "process clock keeps an exited thread", "process %lld < %lld", (long long)process, (long long)(exited + self));
    CHECK(utime >= 0, "/proc/<pid>/stat parses", "utime %lld", (long long)utime);
    CHECK(utime + stime >= threads - SLACK_NS, "/proc/<pid>/stat covers every thread", "utime+stime %lld < threads %lld", (long long)(utime + stime), (long long)threads);
    CHECK(utime + stime <= process + SLACK_NS, "/proc/<pid>/stat matches the process clock", "utime+stime %lld > process %lld", (long long)(utime + stime), (long long)process);
    CHECK(stime <= SLACK_NS, "/proc/<pid>/stat stime stays small for user spins", "stime %lld", (long long)stime);


    char go[2] = {'x', 'x'};

    if (write(release[1], go, sizeof(go)) != (ssize_t)sizeof(go)) {
        perror("cputime-test: write");
    }

    for (int i = 0; i < 2; i++) {
        pthread_join(waiters[i], NULL);
    }


    check_split();


    int64_t utime_after = -1;
    int64_t stime_after = -1;

    stat_times_ns(&utime_after, &stime_after);

    CHECK(stime_after - stime >= SPIN_NS / 2 - SLACK_NS, "/proc/<pid>/stat stime grows with kernel work", "stime %lld -> %lld", (long long)stime, (long long)stime_after);


    check_times();


    printf("cputime-test: %d/%d passed\n", total - failures, total);

    return failures ? 1 : 0;
}
