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

#include <errno.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
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


int main(int argc, char** argv) {

    (void)argc;
    (void)argv;

    printf("vmm-test: starting\n");

    test_kernel_pointer_rejected();
    test_mprotect_foreign_range_rejected();
    test_unsupported_mmap_flags_refused();
    test_huge_mmap_refused();
    test_fresh_anonymous_memory_is_zero();
    test_fresh_brk_memory_is_zero();
    test_fork_memory_is_private();
    test_munmap_reclaims();
    test_fork_exit_loop_returns_memory();
    test_getdents_respects_buffer();

    printf("vmm-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures == 0 ? 0 : 1;
}
