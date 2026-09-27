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
 * @brief Regression tests for the virtual memory manager.
 *
 * A pass means the syscall now refuses, or that the memory handed over is what it should be.
 */

#include <elf.h>
#include <errno.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                      \
    {                                                                    \
        total++;                                                         \
        if (cond) {                                                      \
            printf("vmm-test: PASS  %s\n", (name));                      \
        } else {                                                         \
            failures++;                                                  \
            printf("vmm-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                \
    }


/**
 * @brief Where the kernel direct-maps all of physical memory; a user pointer into it must be refused.
 */
#define KERNEL_HEAP_AREA 0xFFFF800000000000ULL

/**
 * @brief Identity-mapped device MMIO that lives in the low half alongside user memory.
 */
#define LAPIC_BASE 0xFEE00000ULL


/**
 * @brief Checks that a kernel pointer handed to a syscall as a buffer is refused, not dereferenced.
 */
static void test_kernel_pointer_rejected(void) {

    int fd = open("/etc/motd", O_RDONLY);

    if (fd < 0) {
        printf("vmm-test: SKIP  kernel-pointer-rejected (cannot open /etc/motd)\n");
        return;
    }

    errno         = 0;
    ssize_t n     = read(fd, (void*)(uintptr_t)(KERNEL_HEAP_AREA + 0x100000), 64);
    int saved     = errno;

    close(fd);

    CHECK(n < 0 && saved == EFAULT, "kernel-pointer-rejected", "read() returned %ld errno %d, expected -1/EFAULT", (long)n, saved);
}


/**
 * @brief Checks that mprotect() cannot reach outside the caller's own regions.
 */
static void test_mprotect_foreign_range_rejected(void) {

    errno     = 0;
    int e     = mprotect((void*)(uintptr_t)LAPIC_BASE, 0x1000, PROT_READ | PROT_WRITE);
    int saved = errno;

    CHECK(e != 0, "mprotect-mmio-rejected", "mprotect(LAPIC) returned %d errno %d, expected failure", e, saved);
}


/**
 * @brief Checks that MAP_SHARED and MAP_FIXED are refused rather than hitting a PANIC_ASSERT.
 */
static void test_unsupported_mmap_flags_refused(void) {

    errno      = 0;
    void* p    = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    int saved  = errno;

    CHECK(p == MAP_FAILED, "mmap-shared-refused", "mmap(MAP_SHARED) returned %p errno %d, expected MAP_FAILED", p, saved);

    if (p != MAP_FAILED)
        munmap(p, 4096);
}


/**
 * @brief Checks that an absurd length comes back as ENOMEM rather than exhausting physical memory.
 */
static void test_huge_mmap_refused(void) {

    errno      = 0;
    void* p    = mmap(NULL, (size_t)1 << 46, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    int saved  = errno;

    CHECK(p == MAP_FAILED, "mmap-huge-refused", "mmap(64TiB) returned %p errno %d, expected MAP_FAILED", p, saved);

    if (p != MAP_FAILED)
        munmap(p, (size_t)1 << 46);
}


/**
 * @brief Checks that freshly mapped anonymous memory is zero.
 */
static void test_fresh_anonymous_memory_is_zero(void) {

    const size_t len = 64 * 4096;

    unsigned char* p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (p == MAP_FAILED) {
        CHECK(0, "fresh-anon-is-zero", "mmap failed errno %d", errno);
        return;
    }

    size_t nonzero = 0;

    for (size_t i = 0; i < len; i++) {
        if (p[i] != 0)
            nonzero++;
    }

    munmap(p, len);

    CHECK(nonzero == 0, "fresh-anon-is-zero", "%zu of %zu bytes were not zero", nonzero, len);
}


/**
 * @brief Checks that fresh heap memory is zero, which brk(2) maps through a different path.
 */
static void test_fresh_brk_memory_is_zero(void) {

    const size_t len = 256 * 1024;

    unsigned char* p = sbrk(len);

    if (p == (void*)-1) {
        CHECK(0, "fresh-brk-is-zero", "sbrk failed errno %d", errno);
        return;
    }

    size_t nonzero = 0;

    for (size_t i = 0; i < len; i++) {
        if (p[i] != 0)
            nonzero++;
    }

    CHECK(nonzero == 0, "fresh-brk-is-zero", "%zu of %zu bytes were not zero", nonzero, len);
}


/**
 * @brief Checks that a child sees nothing the parent writes after the fork.
 */
static void test_fork_memory_is_private(void) {

    volatile unsigned char* p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (p == MAP_FAILED) {
        CHECK(0, "fork-memory-private", "mmap failed errno %d", errno);
        return;
    }

    p[0] = 0x42;

    pid_t pid = fork();

    if (pid == 0) {

        for (volatile int i = 0; i < 2000000; i++)
            ;

        _exit(p[0] == 0x42 ? 0 : 1);
    }

    p[0] = 0x99;

    int status = 0;
    waitpid(pid, &status, 0);

    munmap((void*)p, 4096);

    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "fork-memory-private", "child saw the parent's post-fork write (status %d)", status);
}


/**
 * @brief Checks that a map and unmap loop can run indefinitely without running the mmap cursor out.
 */
static void test_munmap_reclaims(void) {

    int ok = 1;

    for (int i = 0; i < 512; i++) {

        void* p = mmap(NULL, 64 * 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (p == MAP_FAILED) {
            ok = 0;
            printf("vmm-test:       iteration %d: mmap failed errno %d\n", i, errno);
            break;
        }

        *(volatile unsigned char*)p = 1;

        if (munmap(p, 64 * 4096) != 0) {
            ok = 0;
            printf("vmm-test:       iteration %d: munmap failed errno %d\n", i, errno);
            break;
        }
    }

    CHECK(ok, "munmap-reclaims", "%s", "map/unmap loop did not survive 512 iterations");
}


/**
 * @brief Checks that repeated fork and exit returns memory.
 */
static void test_fork_exit_loop_returns_memory(void) {

    int ok = 1;

    for (int i = 0; i < 200; i++) {

        pid_t pid = fork();

        if (pid < 0) {
            ok = 0;
            printf("vmm-test:       iteration %d: fork failed errno %d\n", i, errno);
            break;
        }

        if (pid == 0)
            _exit(0);

        int status = 0;
        waitpid(pid, &status, 0);
    }

    CHECK(ok, "fork-exit-loop", "%s", "200 fork/exit cycles exhausted memory");
}


/**
 * @brief Checks that getdents64 never writes past the caller's buffer, over a sweep of buffer sizes.
 */
static void test_getdents_respects_buffer(void) {

    int intact = 1;
    long seen  = 0;

    for (size_t cap = 128; cap <= 1024 && intact; cap += 8) {

        int fd = open("/bin", O_RDONLY | O_DIRECTORY);

        if (fd < 0) {
            printf("vmm-test: SKIP  getdents-respects-buffer (cannot open /bin)\n");
            return;
        }


        static unsigned char area[1024 + 256];

        memset(area, 0xA5, sizeof(area));

        for (;;) {

            long n = syscall(SYS_getdents64, fd, area, cap);

            if (n <= 0)
                break;

            seen += n;

            if (n > (long)cap) {
                intact = 0;
                printf("vmm-test:       getdents64 reported %ld bytes for a %zu-byte buffer\n", n, cap);
                break;
            }

            for (size_t i = cap; i < sizeof(area); i++) {

                if (area[i] != 0xA5) {
                    intact = 0;
                    printf("vmm-test:       wrote %zu bytes past the end of a %zu-byte buffer\n", i - cap + 1, cap);
                    break;
                }
            }

            if (!intact)
                break;

            memset(area, 0xA5, sizeof(area));
        }

        close(fd);
    }

    CHECK(intact && seen > 0, "getdents-respects-buffer", "%s", "getdents64 wrote past the caller's buffer");
}


/**
 * @brief Where the test installs itself, so the execve cases can run it again in a child mode.
 */
#define VMM_TEST_PATH "/usr/bin/vmm-test"

#define PAGE 4096UL

/**
 * @brief Threads that keep writing to a page while the main thread takes it away.
 */
#define STALE_WRITERS 3

#define STALE_ROUNDS 40
#define STALE_PAGES  64
#define STALE_MARK   0x5A


static volatile unsigned char* volatile stale_target = NULL;
static volatile int stale_round                      = 0;
static volatile int stale_stop                       = 0;
static volatile int stale_armed[STALE_WRITERS];
static volatile int stale_faults = 0;

static __thread sigjmp_buf stale_jmp;


/**
 * @brief Leaves a write that faulted, once the page it targeted is really gone.
 *
 * @param sig The signal number.
 */
static void stale_segv(int sig) {

    (void)sig;

    stale_faults++;
    siglongjmp(stale_jmp, 1);
}


/**
 * @brief Writes the mark into the current target page until it faults or the round moves on.
 *
 * @param arg The writer's index.
 * @return NULL.
 */
static void* stale_writer(void* arg) {

    const int id = (int)(intptr_t)arg;
    int seen     = 0;

    while (!stale_stop) {

        const int round                  = stale_round;
        volatile unsigned char* volatile p = stale_target;

        if (!p || round == seen)
            continue;

        if (sigsetjmp(stale_jmp, 1) == 0) {

            while (stale_round == round) {

                p[0]            = STALE_MARK;
                p[PAGE / 2]     = STALE_MARK;
                stale_armed[id] = round;
            }
        }

        stale_armed[id] = round;
        seen            = round;
    }

    return NULL;
}


/**
 * @brief Waits until every writer has touched the page of the given round.
 *
 * @param round The round to wait for.
 * @return 1 if all of them did within a second, 0 otherwise.
 */
static int stale_wait_armed(int round) {

    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (;;) {

        int armed = 0;

        for (int i = 0; i < STALE_WRITERS; i++)
            armed += stale_armed[i] == round;

        if (armed == STALE_WRITERS)
            return 1;

        struct timespec t1;
        clock_gettime(CLOCK_MONOTONIC, &t1);

        if (t1.tv_sec - t0.tv_sec >= 2)
            return 0;
    }
}


/**
 * @brief Counts the bytes of a region that carry the writers' mark.
 *
 * @param p The region.
 * @param len Its length.
 * @return The number of marked bytes.
 */
static size_t count_marks(const volatile unsigned char* p, size_t len) {

    size_t n = 0;

    for (size_t i = 0; i < len; i++)
        n += p[i] == STALE_MARK;

    return n;
}


/**
 * @brief Busy-waits, so the caller stays on its CPU.
 *
 * @param ms Milliseconds to wait.
 */
static void spin_ms(long ms) {

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    do {
        clock_gettime(CLOCK_MONOTONIC, &t1);
    } while ((t1.tv_sec - t0.tv_sec) * 1000L + (t1.tv_nsec - t0.tv_nsec) / 1000000L < ms);
}


/**
 * @brief Maps two pages for a round of the stale-writer cases, the second keeping the first's hole from being reused.
 *
 * @param ctx Receives the page that plugs the hole.
 * @return The page the writers target, or NULL.
 */
static void* stale_map_anon(void** ctx) {

    unsigned char* p = mmap(NULL, 2 * PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (p == MAP_FAILED)
        return NULL;

    *ctx = p + PAGE;

    return p;
}


/**
 * @brief Unmaps the targeted page of a round, leaving the plug.
 *
 * @param p The page.
 * @param ctx The plug.
 */
static void stale_unmap_anon(void* p, void* ctx) {

    (void)ctx;

    munmap(p, PAGE);
}


/**
 * @brief Attaches a removed System V segment for a round, so detaching it frees its memory, and plugs the space after it.
 *
 * @param ctx Receives the plug.
 * @return The attachment, or NULL.
 */
static void* stale_map_shm(void** ctx) {

    int id = shmget(IPC_PRIVATE, PAGE, IPC_CREAT | 0600);

    if (id < 0)
        return NULL;

    void* p = shmat(id, NULL, 0);

    shmctl(id, IPC_RMID, NULL);

    if (p == (void*)-1)
        return NULL;

    void* plug = mmap(NULL, PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    *ctx = plug == MAP_FAILED ? NULL : plug;

    return p;
}


/**
 * @brief Detaches the segment of a round, leaving the plug.
 *
 * @param p The attachment.
 * @param ctx The plug.
 */
static void stale_unmap_shm(void* p, void* ctx) {

    (void)ctx;

    shmdt(p);
}


/**
 * @brief Unmaps the plug of a round.
 *
 * @param ctx The plug, or NULL.
 */
static void stale_unplug(void* ctx) {

    if (ctx)
        munmap(ctx, PAGE);
}


/**
 * @brief Takes a page away while other threads write to it, and checks the writes never land in memory handed out afterwards.
 *
 * @param name The case name.
 * @param map Provides the page of a round.
 * @param unmap Takes it away.
 */
static void stale_writer_case(const char* name, void* (*map)(void**), void (*unmap)(void*, void*)) {

    struct sigaction sa, old;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = stale_segv;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, &old);


    pthread_t tid[STALE_WRITERS];

    stale_stop   = 0;
    stale_round  = 0;
    stale_target = NULL;
    stale_faults = 0;

    for (int i = 0; i < STALE_WRITERS; i++) {
        stale_armed[i] = 0;
        pthread_create(&tid[i], NULL, stale_writer, (void*)(intptr_t)i);
    }


    int corrupted    = 0;
    int rounds       = 0;
    int unarmed      = 0;
    int inconclusive = 0;

    for (int r = 1; r <= STALE_ROUNDS; r++) {

        void* ctx = NULL;
        void* p   = map(&ctx);

        if (!p)
            break;

        stale_target = p;
        stale_round  = r;

        if (!stale_wait_armed(r)) {
            unarmed++;
            stale_target = NULL;
            stale_round  = -r;
            unmap(p, ctx);
            stale_unplug(ctx);
            continue;
        }

        unmap(p, ctx);


        unsigned char* q = mmap(NULL, STALE_PAGES * PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (q == MAP_FAILED)
            break;

        spin_ms(3);

        if ((unsigned char*)p >= q && (unsigned char*)p < q + STALE_PAGES * PAGE)
            inconclusive++;
        else if (count_marks(q, STALE_PAGES * PAGE))
            corrupted++;
        else
            rounds++;

        stale_target = NULL;
        stale_round  = -r;

        munmap(q, STALE_PAGES * PAGE);
        stale_unplug(ctx);
    }


    stale_stop = 1;

    for (int i = 0; i < STALE_WRITERS; i++)
        pthread_join(tid[i], NULL);

    sigaction(SIGSEGV, &old, NULL);


    printf("vmm-test:       %s: %d clean rounds, %d unarmed, %d inconclusive, %d writer faults\n", name, rounds, unarmed, inconclusive, stale_faults);

    CHECK(rounds + corrupted > 0 && corrupted == 0, name, "%d of %d rounds found the writers' mark in freshly mapped memory", corrupted, rounds + corrupted);
}


/**
 * @brief Checks that writes through another CPU's stale TLB entry never reach a frame munmap() gave back.
 */
static void test_tlb_munmap(void) {
    stale_writer_case("tlb-munmap", stale_map_anon, stale_unmap_anon);
}


/**
 * @brief Checks the same for the last detach of a removed shared memory segment.
 */
static void test_tlb_shmdt(void) {
    stale_writer_case("tlb-shmdt", stale_map_shm, stale_unmap_shm);
}


static volatile int upgrade_round = 0;
static volatile int upgrade_go    = 0;
static volatile int upgrade_done[STALE_WRITERS];
static volatile int upgrade_faults = 0;


/**
 * @brief Counts a write refused on a page that is already writable.
 *
 * @param sig The signal number.
 */
static void upgrade_segv(int sig) {

    (void)sig;

    upgrade_faults++;
    siglongjmp(stale_jmp, 1);
}


/**
 * @brief Reads the target page until told to write to it.
 *
 * @param arg The reader's index.
 * @return NULL.
 */
static void* upgrade_reader(void* arg) {

    const int id = (int)(intptr_t)arg;

    while (!stale_stop) {

        const int round                  = upgrade_round;
        volatile unsigned char* volatile p = stale_target;

        if (!p || upgrade_done[id] == round)
            continue;

        unsigned sum = 0;

        while (upgrade_go != round && !stale_stop) {
            sum += p[0];
            stale_armed[id] = round;
        }

        if (sigsetjmp(stale_jmp, 1) == 0)
            p[0] = (unsigned char)(sum + id);

        upgrade_done[id] = round;
    }

    return NULL;
}


/**
 * @brief Checks that a page made writable by one thread can be written at once by threads on other CPUs.
 */
static void test_tlb_mprotect_upgrade(void) {

    struct sigaction sa, old;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = upgrade_segv;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, &old);


    pthread_t tid[STALE_WRITERS];

    stale_stop     = 0;
    stale_target   = NULL;
    upgrade_round  = 0;
    upgrade_go     = 0;
    upgrade_faults = 0;

    for (int i = 0; i < STALE_WRITERS; i++) {
        stale_armed[i]  = 0;
        upgrade_done[i] = 0;
        pthread_create(&tid[i], NULL, upgrade_reader, (void*)(intptr_t)i);
    }


    int rounds = 0;

    for (int r = 1; r <= STALE_ROUNDS; r++) {

        unsigned char* p = mmap(NULL, PAGE, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (p == MAP_FAILED)
            break;

        stale_target  = p;
        upgrade_round = r;

        if (!stale_wait_armed(r)) {
            upgrade_go   = r;
            stale_target = NULL;
            spin_ms(5);
            munmap(p, PAGE);
            continue;
        }

        mprotect(p, PAGE, PROT_READ | PROT_WRITE);

        upgrade_go = r;

        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        for (;;) {

            int done = 0;

            for (int i = 0; i < STALE_WRITERS; i++)
                done += upgrade_done[i] == r;

            clock_gettime(CLOCK_MONOTONIC, &t1);

            if (done == STALE_WRITERS || t1.tv_sec - t0.tv_sec >= 2)
                break;
        }

        stale_target = NULL;
        munmap(p, PAGE);

        rounds++;
    }


    stale_stop = 1;

    for (int i = 0; i < STALE_WRITERS; i++)
        pthread_join(tid[i], NULL);

    sigaction(SIGSEGV, &old, NULL);

    CHECK(rounds > 0 && upgrade_faults == 0, "tlb-mprotect-upgrade", "%d writes faulted on a page already made writable, over %d rounds", upgrade_faults, rounds);
}


/**
 * @brief Reads a line of /proc/meminfo.
 *
 * @param key The field, with its colon.
 * @return The value in kB, or -1.
 */
static long meminfo_kb(const char* key) {

    FILE* f = fopen("/proc/meminfo", "r");

    if (!f)
        return -1;


    char line[128];
    long kb = -1;

    while (kb < 0 && fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, strlen(key)) == 0)
            kb = strtol(line + strlen(key), NULL, 10);
    }

    fclose(f);

    return kb;
}


/**
 * @brief Checks that memory unmapped while sibling threads run on other CPUs is still given back.
 */
static void test_munmap_frees_memory(void) {

    struct sigaction sa, old;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = stale_segv;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, &old);


    unsigned char* anchor = mmap(NULL, PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    pthread_t tid[STALE_WRITERS];

    stale_stop   = 0;
    stale_target = anchor;
    stale_round  = 1;

    for (int i = 0; i < STALE_WRITERS; i++) {
        stale_armed[i] = 0;
        pthread_create(&tid[i], NULL, stale_writer, (void*)(intptr_t)i);
    }

    stale_wait_armed(1);


    long before = meminfo_kb("MemFree:");
    int ok      = 1;

    for (int i = 0; i < 200 && ok; i++) {

        unsigned char* q = mmap(NULL, 256 * PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (q == MAP_FAILED) {
            ok = 0;
            break;
        }

        q[0] = 1;
        munmap(q, 256 * PAGE);
    }


    stale_stop  = 1;
    stale_round = 2;

    for (int i = 0; i < STALE_WRITERS; i++)
        pthread_join(tid[i], NULL);

    sigaction(SIGSEGV, &old, NULL);
    munmap(anchor, PAGE);


    long after = before;

    for (int i = 0; i < 50; i++) {

        after = meminfo_kb("MemFree:");

        if (before - after < 1024)
            break;

        usleep(10000);
    }

    CHECK(ok && before > 0 && before - after < 1024, "munmap-frees-memory", "MemFree went from %ld kB to %ld kB over 200 x 1 MiB mmap/munmap with threads running", before, after);
}


/**
 * @brief Checks that an mmap() after a munmap() does not land on a shared memory attachment placed in between.
 */
static void test_mmap_after_shmat(void) {

    int ok = 1;

    for (int round = 0; round < 8 && ok; round++) {

        void* a = mmap(NULL, 4 * PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        int id = shmget(IPC_PRIVATE, 4 * PAGE, IPC_CREAT | 0600);

        unsigned char* s = id >= 0 ? shmat(id, NULL, 0) : (void*)-1;

        if (a == MAP_FAILED || s == (void*)-1) {
            printf("vmm-test:       mmap-after-shmat: setup failed errno %d\n", errno);
            ok = 0;
            break;
        }

        memset(s, 0x77, 4 * PAGE);
        munmap(a, 4 * PAGE);


        unsigned char* b = mmap(NULL, 32 * PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (b == MAP_FAILED) {
            printf("vmm-test:       round %d: mmap after shmat/munmap failed errno %d\n", round, errno);
            ok = 0;
        } else {
            memset(b, 0x11, 32 * PAGE);

            for (size_t i = 0; i < 4 * PAGE && ok; i++) {
                if (s[i] != 0x77) {
                    printf("vmm-test:       round %d: the new mapping overwrote the attachment\n", round);
                    ok = 0;
                }
            }

            munmap(b, 32 * PAGE);
        }

        shmdt(s);
        shmctl(id, IPC_RMID, NULL);
    }

    CHECK(ok, "mmap-after-shmat", "%s", "mmap failed or overlapped an attachment");
}


/**
 * @brief Checks that a hole left by munmap() is used again.
 */
static void test_mmap_reuses_holes(void) {

    unsigned char* x = mmap(NULL, PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    unsigned char* y = mmap(NULL, PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    munmap(x, PAGE);

    unsigned char* z = mmap(NULL, PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    CHECK(x != MAP_FAILED && y != MAP_FAILED && z != MAP_FAILED && z < y, "mmap-reuses-holes", "freed %p below %p, next mmap went to %p", (void*)x, (void*)y, (void*)z);

    munmap(y, PAGE);
    munmap(z, PAGE);
}


/**
 * @brief Maps and unmaps regions and attachments of random sizes, checking none ever overlaps another.
 */
static void test_mmap_churn(void) {

    enum { SLOTS = 48 };

    struct {
        unsigned char* p;
        size_t len;
        int shm;
        unsigned char fill;
    } slot[SLOTS];

    memset(slot, 0, sizeof(slot));

    unsigned seed = 12345;
    int ok        = 1;
    int attached  = 0;

    for (int it = 0; it < 3000 && ok; it++) {

        seed = seed * 1103515245 + 12345;

        int i = (int)((seed >> 16) % SLOTS);

        if (slot[i].p) {

            for (size_t k = 0; k < slot[i].len; k += 512) {
                if (slot[i].p[k] != slot[i].fill) {
                    printf("vmm-test:       iteration %d: region %p lost its contents\n", it, (void*)slot[i].p);
                    ok = 0;
                    break;
                }
            }

            if (slot[i].shm) {
                shmdt(slot[i].p);
                attached--;
            } else {
                munmap(slot[i].p, slot[i].len);
            }

            slot[i].p = NULL;
            continue;
        }


        size_t len = (1 + (seed >> 8) % 24) * PAGE;
        int shm    = attached < 8 && ((seed >> 4) & 7) == 0;

        unsigned char* p;

        if (shm) {

            int id = shmget(IPC_PRIVATE, len, IPC_CREAT | 0600);
            p      = id >= 0 ? shmat(id, NULL, 0) : (void*)-1;

            if (id >= 0)
                shmctl(id, IPC_RMID, NULL);

            if (p == (void*)-1)
                p = MAP_FAILED;
            else
                attached++;

        } else {

            p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }

        if (p == MAP_FAILED) {
            printf("vmm-test:       iteration %d: %s of %zu bytes failed errno %d\n", it, shm ? "shmat" : "mmap", len, errno);
            ok = 0;
            break;
        }

        slot[i].p    = p;
        slot[i].len  = len;
        slot[i].shm  = shm;
        slot[i].fill = (unsigned char)(1 + it % 250);

        memset(p, slot[i].fill, len);
    }

    for (int i = 0; i < SLOTS; i++) {

        if (!slot[i].p)
            continue;

        if (slot[i].shm)
            shmdt(slot[i].p);
        else
            munmap(slot[i].p, slot[i].len);
    }

    CHECK(ok, "mmap-churn", "%s", "a mapping failed or was overwritten by another");
}


/**
 * @brief Checks that a file mapping starts at its offset and leaves the file position alone.
 */
static void test_mmap_file_offset(void) {

    const char* path = "/tmp/vmm-test-map";

    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);

    if (fd < 0) {
        printf("vmm-test: SKIP  mmap-file-offset (cannot create %s)\n", path);
        return;
    }

    static unsigned char page[PAGE];

    for (int i = 0; i < 4; i++) {
        memset(page, 'A' + i, PAGE);
        write(fd, page, PAGE);
    }

    lseek(fd, 0, SEEK_SET);


    unsigned char* p = mmap(NULL, PAGE, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 2 * PAGE);
    off_t pos        = lseek(fd, 0, SEEK_CUR);

    CHECK(p != MAP_FAILED && p[0] == 'C' && p[PAGE - 1] == 'C' && pos == 0, "mmap-file-offset", "mmap at offset 2 pages read '%c', file position now %ld", p != MAP_FAILED ? p[0] : '?', (long)pos);

    if (p != MAP_FAILED)
        munmap(p, PAGE);


    unsigned char* q = mmap(NULL, 2 * PAGE, PROT_READ, MAP_PRIVATE, fd, 3 * PAGE);

    CHECK(q != MAP_FAILED && q[0] == 'D' && q[PAGE] == 0, "mmap-file-tail", "mmap past the end returned %p errno %d", (void*)q, q == MAP_FAILED ? errno : 0);

    if (q != MAP_FAILED)
        munmap(q, 2 * PAGE);

    close(fd);
    unlink(path);
}


/**
 * @brief Fills an argument with the pattern the argv child mode expects.
 *
 * @param buf Where the argument goes.
 * @param i Its index.
 * @param len Its length.
 */
static void argv_pattern(char* buf, int i, int len) {

    memset(buf, 'a' + i % 26, (size_t)len);
    buf[len] = '\0';
}


/**
 * @brief Runs the test again through execve(), with a child mode and extra arguments.
 *
 * @param count Pattern arguments to add.
 * @param len Length of each.
 * @return The child's wait status, or -1.
 */
static int exec_pattern(int count, int len) {

    pid_t pid = fork();

    if (pid < 0)
        return -1;

    if (pid == 0) {

        char** args = calloc((size_t)count + 6, sizeof(char*));
        char* pool  = malloc((size_t)count * (size_t)(len + 1) + 64);

        if (!args || !pool)
            _exit(4);

        char cnt[16], ln[16];
        snprintf(cnt, sizeof(cnt), "%d", count);
        snprintf(ln, sizeof(ln), "%d", len);

        args[0] = "vmm-test";
        args[1] = "--exec-child";
        args[2] = "argv";
        args[3] = cnt;
        args[4] = ln;

        for (int i = 0; i < count; i++) {
            args[5 + i] = pool + (size_t)i * (size_t)(len + 1);
            argv_pattern(args[5 + i], i, len);
        }

        args[5 + count] = NULL;

        execv(VMM_TEST_PATH, args);
        _exit(errno == E2BIG ? 7 : 3);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    return status;
}


/**
 * @brief Checks that a 100 KiB argument list reaches the new program intact.
 */
static void test_exec_big_argv(void) {

    int status = exec_pattern(1000, 100);

    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "exec-big-argv", "child status 0x%x", status);
}


/**
 * @brief Checks that an argument list over the limit fails with E2BIG instead of reaching the kernel stack.
 */
static void test_exec_e2big(void) {

    int status = exec_pattern(1024, 1024);

    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 7, "exec-e2big", "child status 0x%x, expected exit 7 (E2BIG)", status);
}


/**
 * @brief Checks that an argv array crossing into a page whose frame is not the next one is read correctly.
 */
static void test_exec_argv_straddle(void) {

    pid_t pid = fork();

    if (pid == 0) {

        void* single[64];

        for (int i = 0; i < 64; i++)
            single[i] = mmap(NULL, PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        for (int i = 0; i < 64; i += 2)
            munmap(single[i], PAGE);


        char* q = mmap(NULL, 2 * PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (q == MAP_FAILED)
            _exit(4);

        static char strings[6][9];

        for (int i = 0; i < 6; i++)
            argv_pattern(strings[i], i, 8);


        char** args = (char**)(q + PAGE - 3 * sizeof(char*));

        args[0] = "vmm-test";
        args[1] = "--exec-child";
        args[2] = "argv";
        args[3] = "6";
        args[4] = "8";

        for (int i = 0; i < 6; i++)
            args[5 + i] = strings[i];

        args[11] = NULL;

        execv(VMM_TEST_PATH, args);
        _exit(3);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "exec-argv-straddle", "child status 0x%x", status);
}


/**
 * @brief Writes a minimal static ELF image with the given program headers.
 *
 * @param path Where to write it.
 * @param ph The program headers.
 * @param n How many.
 * @return 0 on success, -1 on failure.
 */
static int write_elf(const char* path, const Elf64_Phdr* ph, int n) {

    Elf64_Ehdr eh;
    memset(&eh, 0, sizeof(eh));

    memcpy(eh.e_ident, ELFMAG, SELFMAG);

    eh.e_ident[EI_CLASS]   = ELFCLASS64;
    eh.e_ident[EI_DATA]    = ELFDATA2LSB;
    eh.e_ident[EI_VERSION] = EV_CURRENT;
    eh.e_type              = ET_EXEC;
    eh.e_machine           = EM_X86_64;
    eh.e_version           = EV_CURRENT;
    eh.e_entry             = 0x10000000;
    eh.e_phoff             = sizeof(eh);
    eh.e_ehsize            = sizeof(eh);
    eh.e_phentsize         = sizeof(Elf64_Phdr);
    eh.e_phnum             = (Elf64_Half)n;

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0755);

    if (fd < 0)
        return -1;

    write(fd, &eh, sizeof(eh));
    write(fd, ph, sizeof(*ph) * (size_t)n);

    static unsigned char pad[256];
    write(fd, pad, sizeof(pad));

    close(fd);

    return 0;
}


/**
 * @brief Runs a broken image and checks execve() fails and hands the caller back its own program.
 *
 * @param name The case name.
 * @param ph The image's program headers.
 * @param n How many.
 */
static void exec_broken(const char* name, const Elf64_Phdr* ph, int n) {

    const char* path = "/tmp/vmm-test-bad-elf";

    if (write_elf(path, ph, n) < 0) {
        printf("vmm-test: SKIP  %s (cannot write %s)\n", name, path);
        return;
    }


    static volatile int alive = 0x1234;

    pid_t pid = fork();

    if (pid == 0) {

        char* args[] = {"bad-elf", NULL};

        execv(path, args);

        int saved = errno;

        _exit(alive == 0x1234 && saved != 0 ? 0 : 5);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    unlink(path);

    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, name, "child status 0x%x, expected execve() to fail and return", status);
}


/**
 * @brief Checks that an image whose segment runs past the end of the file is refused.
 */
static void test_exec_truncated_elf(void) {

    Elf64_Phdr ph;
    memset(&ph, 0, sizeof(ph));

    ph.p_type   = PT_LOAD;
    ph.p_flags  = PF_R | PF_X;
    ph.p_offset = 0;
    ph.p_vaddr  = 0x10000000;
    ph.p_filesz = 0x40000;
    ph.p_memsz  = 0x40000;
    ph.p_align  = PAGE;

    exec_broken("exec-truncated-elf", &ph, 1);
}


/**
 * @brief Checks that an image whose segments overlap is refused.
 */
static void test_exec_overlap_elf(void) {

    Elf64_Phdr ph[2];
    memset(ph, 0, sizeof(ph));

    ph[0].p_type   = PT_LOAD;
    ph[0].p_flags  = PF_R | PF_X;
    ph[0].p_vaddr  = 0x10000000;
    ph[0].p_filesz = 0x100;
    ph[0].p_memsz  = 0x2000;
    ph[0].p_align  = PAGE;

    ph[1]         = ph[0];
    ph[1].p_vaddr = 0x10001000;

    exec_broken("exec-overlap-elf", ph, 2);
}


/**
 * @brief Checks that a dynamically linked image is refused rather than crashing the kernel.
 */
static void test_exec_dynamic_elf(void) {

    Elf64_Phdr ph[2];
    memset(ph, 0, sizeof(ph));

    ph[0].p_type   = PT_LOAD;
    ph[0].p_flags  = PF_R | PF_X;
    ph[0].p_vaddr  = 0x10000000;
    ph[0].p_filesz = 0x100;
    ph[0].p_memsz  = 0x1000;
    ph[0].p_align  = PAGE;

    ph[1].p_type   = PT_DYNAMIC;
    ph[1].p_flags  = PF_R;
    ph[1].p_vaddr  = 0x10000000;
    ph[1].p_filesz = 0x10;
    ph[1].p_memsz  = 0x10;
    ph[1].p_align  = 8;

    exec_broken("exec-dynamic-elf", ph, 2);
}


/**
 * @brief A handler that exists only to be reset by execve().
 *
 * @param sig The signal number.
 */
static void noop_handler(int sig) {
    (void)sig;
}


/**
 * @brief Checks that an ignored signal stays ignored across execve() while a caught one is reset.
 */
static void test_exec_keeps_sig_ign(void) {

    pid_t pid = fork();

    if (pid == 0) {

        signal(SIGUSR1, SIG_IGN);
        signal(SIGUSR2, noop_handler);

        char* args[] = {"vmm-test", "--exec-child", "sigign", NULL};

        execv(VMM_TEST_PATH, args);
        _exit(3);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "exec-keeps-sig-ign", "child status 0x%x", status);
}


/**
 * @brief Writes a byte down a pipe every couple of milliseconds, for as long as it lives.
 *
 * @param arg The write end.
 * @return Never.
 */
static void* ticker(void* arg) {

    const int fd = (int)(intptr_t)arg;

    for (;;) {
        write(fd, "x", 1);
        usleep(2000);
    }

    return NULL;
}


/**
 * @brief Counts what a non-blocking pipe holds.
 *
 * @param fd The read end.
 * @return The bytes read.
 */
static long drain(int fd) {

    char buf[256];
    long n = 0;

    for (;;) {

        ssize_t r = read(fd, buf, sizeof(buf));

        if (r <= 0)
            break;

        n += r;
    }

    return n;
}


/**
 * @brief Checks that execve() ends the caller's other threads.
 */
static void test_exec_kills_threads(void) {

    int data[2], note[2];

    if (pipe(data) < 0 || pipe(note) < 0) {
        printf("vmm-test: SKIP  exec-kills-threads (pipe failed)\n");
        return;
    }

    pid_t pid = fork();

    if (pid == 0) {

        close(data[0]);
        close(note[0]);

        fcntl(data[1], F_SETFD, FD_CLOEXEC);

        pthread_t t;
        pthread_create(&t, NULL, ticker, (void*)(intptr_t)data[1]);

        usleep(20000);

        char fd[16];
        snprintf(fd, sizeof(fd), "%d", note[1]);

        char* args[] = {"vmm-test", "--exec-child", "notify", fd, NULL};

        execv(VMM_TEST_PATH, args);
        _exit(3);
    }

    close(data[1]);
    close(note[1]);

    fcntl(data[0], F_SETFL, O_NONBLOCK);


    char c = 0;
    ssize_t n = read(note[0], &c, 1);

    usleep(100000);
    drain(data[0]);
    usleep(100000);

    long late = drain(data[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    close(data[0]);
    close(note[0]);

    CHECK(n == 1 && late == 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0, "exec-kills-threads", "a thread of the old image wrote %ld bytes after the exec (child status 0x%x)", late, status);
}


/**
 * @brief The program run by the execve cases, checking what the kernel handed over.
 *
 * @param argc The argument count.
 * @param argv The arguments.
 * @return 0 when everything matched.
 */
static int exec_child(int argc, char** argv) {

    const char* mode = argv[2];

    if (strcmp(mode, "argv") == 0) {

        if (argc < 5)
            return 2;

        int count = atoi(argv[3]);
        int len   = atoi(argv[4]);

        if (argc != 5 + count)
            return 2;

        char* want = malloc((size_t)len + 1);

        for (int i = 0; i < count; i++) {

            argv_pattern(want, i, len);

            if (strcmp(argv[5 + i], want) != 0)
                return 2;
        }

        return 0;
    }

    if (strcmp(mode, "sigign") == 0) {

        struct sigaction sa;

        sigaction(SIGUSR1, NULL, &sa);

        if (sa.sa_handler != SIG_IGN)
            return 2;

        sigaction(SIGUSR2, NULL, &sa);

        if (sa.sa_handler != SIG_DFL)
            return 2;

        return 0;
    }

    if (strcmp(mode, "notify") == 0 && argc > 3) {

        write(atoi(argv[3]), "E", 1);
        usleep(300000);

        return 0;
    }

    return 2;
}


static struct {

    const char* name;
    void (*fn)(void);

} cases[] = {
    {"kernel-pointer-rejected", test_kernel_pointer_rejected},
    {"mprotect-mmio-rejected", test_mprotect_foreign_range_rejected},
    {"mmap-shared-refused", test_unsupported_mmap_flags_refused},
    {"mmap-huge-refused", test_huge_mmap_refused},
    {"fresh-anon-is-zero", test_fresh_anonymous_memory_is_zero},
    {"fresh-brk-is-zero", test_fresh_brk_memory_is_zero},
    {"fork-memory-private", test_fork_memory_is_private},
    {"munmap-reclaims", test_munmap_reclaims},
    {"fork-exit-loop", test_fork_exit_loop_returns_memory},
    {"getdents-respects-buffer", test_getdents_respects_buffer},
    {"mmap-after-shmat", test_mmap_after_shmat},
    {"mmap-reuses-holes", test_mmap_reuses_holes},
    {"mmap-churn", test_mmap_churn},
    {"mmap-file-offset", test_mmap_file_offset},
    {"exec-keeps-sig-ign", test_exec_keeps_sig_ign},
    {"exec-kills-threads", test_exec_kills_threads},
    {"exec-truncated-elf", test_exec_truncated_elf},
    {"exec-overlap-elf", test_exec_overlap_elf},
    {"exec-argv-straddle", test_exec_argv_straddle},
    {"tlb-mprotect-upgrade", test_tlb_mprotect_upgrade},
    {"munmap-frees-memory", test_munmap_frees_memory},
    {"exec-big-argv", test_exec_big_argv},
    {"exec-e2big", test_exec_e2big},
    {"exec-dynamic-elf", test_exec_dynamic_elf},
    {"tlb-munmap", test_tlb_munmap},
    {"tlb-shmdt", test_tlb_shmdt},
};


int main(int argc, char** argv) {

    if (argc > 2 && strcmp(argv[1], "--exec-child") == 0)
        return exec_child(argc, argv);

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("vmm-test: starting\n");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {

        if (argc > 1 && strcmp(argv[1], cases[i].name) != 0)
            continue;

        cases[i].fn();
    }

    printf("vmm-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures == 0 ? 0 : 1;
}
