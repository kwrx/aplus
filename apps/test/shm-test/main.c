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
 * @brief Regression tests for System V shared memory.
 *
 * Checks what separates a segment from an ordinary mapping: sharing, inheritance, and who frees the frames.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                       \
    {                                                                     \
        total++;                                                          \
        if (cond) {                                                       \
            printf("shm-test: PASS  %s\n", (name));                       \
        } else {                                                          \
            failures++;                                                   \
            printf("shm-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                 \
    }


#define SHM_TEST_SIZE (256 * 1024)


/**
 * @brief Checks that a segment can be created, attached, written through and read back, and starts zeroed.
 */
static void test_attach_roundtrip(void) {

    int id = shmget(IPC_PRIVATE, SHM_TEST_SIZE, IPC_CREAT | 0600);

    if (id < 0) {
        CHECK(0, "attach-roundtrip", "shmget() failed: %s", strerror(errno));
        return;
    }


    uint32_t* p = (uint32_t*)shmat(id, NULL, 0);

    if (p == (uint32_t*)-1) {
        CHECK(0, "attach-roundtrip", "shmat() failed: %s", strerror(errno));
        shmctl(id, IPC_RMID, NULL);
        return;
    }


    int dirty = 0;

    for (size_t i = 0; i < SHM_TEST_SIZE / sizeof(uint32_t); i++) {

        if (p[i] != 0) {
            dirty++;
        }
    }

    CHECK(dirty == 0, "zero-filled", "%d of %zu words were not zero", dirty, SHM_TEST_SIZE / sizeof(uint32_t));


    for (size_t i = 0; i < SHM_TEST_SIZE / sizeof(uint32_t); i++) {
        p[i] = (uint32_t)i ^ 0xA5A5A5A5u;
    }

    int bad = 0;

    for (size_t i = 0; i < SHM_TEST_SIZE / sizeof(uint32_t); i++) {

        if (p[i] != ((uint32_t)i ^ 0xA5A5A5A5u)) {
            bad++;
        }
    }

    CHECK(bad == 0, "attach-roundtrip", "%d words read back wrong", bad);


    CHECK(shmdt(p) == 0, "detach", "shmdt() failed: %s", strerror(errno));
    CHECK(shmctl(id, IPC_RMID, NULL) == 0, "remove", "shmctl(IPC_RMID) failed: %s", strerror(errno));
}


/**
 * @brief Checks that two attachments of one segment are two windows onto the same frames.
 */
static void test_two_attachments_alias(void) {

    int id = shmget(IPC_PRIVATE, SHM_TEST_SIZE, IPC_CREAT | 0600);

    if (id < 0) {
        CHECK(0, "alias", "shmget() failed: %s", strerror(errno));
        return;
    }


    volatile uint32_t* a = (volatile uint32_t*)shmat(id, NULL, 0);
    volatile uint32_t* b = (volatile uint32_t*)shmat(id, NULL, 0);

    if (a == (volatile uint32_t*)-1 || b == (volatile uint32_t*)-1) {
        CHECK(0, "alias", "shmat() failed: %s", strerror(errno));
        shmctl(id, IPC_RMID, NULL);
        return;
    }

    CHECK(a != b, "alias-distinct", "both attachments landed on %p", (void*)a);


    a[0]                                      = 0xDEADBEEFu;
    a[(SHM_TEST_SIZE / sizeof(uint32_t)) - 1] = 0xFEEDFACEu;

    CHECK(b[0] == 0xDEADBEEFu, "alias-first-page", "read %#x", b[0]);
    CHECK(b[(SHM_TEST_SIZE / sizeof(uint32_t)) - 1] == 0xFEEDFACEu, "alias-last-page", "read %#x", b[(SHM_TEST_SIZE / sizeof(uint32_t)) - 1]);

    shmdt((void*)a);
    shmdt((void*)b);
    shmctl(id, IPC_RMID, NULL);
}


/**
 * @brief Checks that a child and its parent see each other's writes through an inherited attachment.
 */
static void test_shared_across_fork(void) {

    int id = shmget(IPC_PRIVATE, SHM_TEST_SIZE, IPC_CREAT | 0600);

    if (id < 0) {
        CHECK(0, "fork-shared", "shmget() failed: %s", strerror(errno));
        return;
    }


    volatile uint32_t* p = (volatile uint32_t*)shmat(id, NULL, 0);

    if (p == (volatile uint32_t*)-1) {
        CHECK(0, "fork-shared", "shmat() failed: %s", strerror(errno));
        shmctl(id, IPC_RMID, NULL);
        return;
    }

    p[0] = 0x11111111u;
    p[1] = 0;


    pid_t pid = fork();

    if (pid == 0) {

        _exit(p[0] == 0x11111111u ? ((p[1] = 0x22222222u), 0) : 1);
    }

    if (pid < 0) {
        CHECK(0, "fork-shared", "fork() failed: %s", strerror(errno));
        shmdt((void*)p);
        shmctl(id, IPC_RMID, NULL);
        return;
    }


    int status = 0;

    waitpid(pid, &status, 0);

    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "fork-child-sees-parent", "child exited with status %#x", status);
    CHECK(p[1] == 0x22222222u, "fork-parent-sees-child", "read %#x", p[1]);

    shmdt((void*)p);
    shmctl(id, IPC_RMID, NULL);
}


/**
 * @brief Checks that a segment outlives the process that created it, as long as somebody holds it.
 */
static void test_survives_creator(void) {

    int id = shmget(IPC_PRIVATE, SHM_TEST_SIZE, IPC_CREAT | 0600);

    if (id < 0) {
        CHECK(0, "survives-creator", "shmget() failed: %s", strerror(errno));
        return;
    }

    volatile uint32_t* p = (volatile uint32_t*)shmat(id, NULL, 0);

    if (p == (volatile uint32_t*)-1) {
        CHECK(0, "survives-creator", "shmat() failed: %s", strerror(errno));
        shmctl(id, IPC_RMID, NULL);
        return;
    }

    p[0] = 0x5A5A5A5Au;


    pid_t pid = fork();

    if (pid == 0) {

        volatile uint32_t* q = (volatile uint32_t*)shmat(id, NULL, 0);

        if (q == (volatile uint32_t*)-1)
            _exit(1);

        q[2] = 0x3C3C3C3Cu;

        _exit(q[0] == 0x5A5A5A5Au ? 0 : 1);
    }

    if (pid < 0) {
        CHECK(0, "survives-creator", "fork() failed: %s", strerror(errno));
        shmdt((void*)p);
        shmctl(id, IPC_RMID, NULL);
        return;
    }


    int status = 0;

    waitpid(pid, &status, 0);

    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "second-process-attach", "child exited with status %#x", status);
    CHECK(p[2] == 0x3C3C3C3Cu, "second-process-write", "read %#x", p[2]);


    struct shmid_ds ds;

    memset(&ds, 0, sizeof(ds));

    if (shmctl(id, IPC_STAT, &ds) < 0) {
        CHECK(0, "stat", "shmctl(IPC_STAT) failed: %s", strerror(errno));
    } else {
        CHECK(ds.shm_segsz >= SHM_TEST_SIZE, "stat-size", "reported %zu bytes", (size_t)ds.shm_segsz);
        CHECK(ds.shm_nattch == 1, "stat-nattch", "reported %lu attachments after the child exited", (unsigned long)ds.shm_nattch);
    }

    shmdt((void*)p);
    shmctl(id, IPC_RMID, NULL);
}


/**
 * @brief Checks that removing an attached segment defers the destruction and blocks any new attachment.
 */
static void test_deferred_removal(void) {

    int id = shmget(IPC_PRIVATE, SHM_TEST_SIZE, IPC_CREAT | 0600);

    if (id < 0) {
        CHECK(0, "deferred-removal", "shmget() failed: %s", strerror(errno));
        return;
    }

    volatile uint32_t* p = (volatile uint32_t*)shmat(id, NULL, 0);

    if (p == (volatile uint32_t*)-1) {
        CHECK(0, "deferred-removal", "shmat() failed: %s", strerror(errno));
        shmctl(id, IPC_RMID, NULL);
        return;
    }

    p[0] = 0x0BADC0DEu;

    CHECK(shmctl(id, IPC_RMID, NULL) == 0, "rmid-while-attached", "shmctl(IPC_RMID) failed: %s", strerror(errno));

    CHECK(p[0] == 0x0BADC0DEu, "rmid-keeps-mapping", "read %#x through an attachment that outlived IPC_RMID", p[0]);

    CHECK(shmat(id, NULL, 0) == (void*)-1 && errno == EIDRM, "rmid-blocks-attach", "a removed segment was still attachable (errno %d)", errno);

    CHECK(shmdt((void*)p) == 0, "rmid-final-detach", "shmdt() failed: %s", strerror(errno));


    struct shmid_ds ds;

    CHECK(shmctl(id, IPC_STAT, &ds) < 0, "rmid-id-gone", "the id still resolved after the last detach%s", "");
}


/**
 * @brief Checks that a key resolves to its segment and that a stale id resolves to nothing.
 */
static void test_keys_and_stale_ids(void) {

    const key_t key = 0x61706C75;

    int old = shmget(key, 0, 0);

    if (old >= 0) {
        shmctl(old, IPC_RMID, NULL);
    }


    CHECK(shmget(key, SHM_TEST_SIZE, 0) < 0 && errno == ENOENT, "key-missing", "a key that was never created resolved (errno %d)", errno);

    int a = shmget(key, SHM_TEST_SIZE, IPC_CREAT | 0600);

    if (a < 0) {
        CHECK(0, "key-create", "shmget() failed: %s", strerror(errno));
        return;
    }

    CHECK(shmget(key, SHM_TEST_SIZE, 0) == a, "key-lookup", "the same key gave a different id%s", "");
    CHECK(shmget(key, SHM_TEST_SIZE, IPC_CREAT | IPC_EXCL | 0600) < 0 && errno == EEXIST, "key-excl", "IPC_EXCL did not refuse an existing key (errno %d)", errno);
    CHECK(shmget(key, SHM_TEST_SIZE * 8, 0) < 0 && errno == EINVAL, "key-too-small", "a segment smaller than requested was handed back (errno %d)", errno);

    CHECK(shmctl(a, IPC_RMID, NULL) == 0, "key-remove", "shmctl(IPC_RMID) failed: %s", strerror(errno));


    int b = shmget(IPC_PRIVATE, SHM_TEST_SIZE, IPC_CREAT | 0600);

    if (b < 0) {
        CHECK(0, "stale-id", "shmget() failed: %s", strerror(errno));
        return;
    }

    CHECK(b != a, "stale-id", "a new segment reused the id %d", a);
    CHECK(shmat(a, NULL, 0) == (void*)-1, "stale-id-attach", "a destroyed id was still attachable%s", "");

    shmctl(b, IPC_RMID, NULL);
}


/**
 * @brief Checks that everything a segment holds comes back, over enough cycles to exhaust the ceiling if not.
 */
static void test_no_leak(void) {

    const size_t size = 4 * 1024 * 1024;

    for (int i = 0; i < 64; i++) {

        int id = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);

        if (id < 0) {
            CHECK(0, "no-leak", "shmget() failed on cycle %d: %s", i, strerror(errno));
            return;
        }


        void* p = shmat(id, NULL, 0);

        if (p == (void*)-1) {
            CHECK(0, "no-leak", "shmat() failed on cycle %d: %s", i, strerror(errno));
            shmctl(id, IPC_RMID, NULL);
            return;
        }

        *(volatile uint32_t*)p                       = (uint32_t)i;
        *((volatile uint32_t*)((char*)p + size) - 1) = (uint32_t)i;

        shmdt(p);
        shmctl(id, IPC_RMID, NULL);
    }

    CHECK(1, "no-leak", "%s", "");
}


static const struct {
    const char* name;
    void (*fn)(void);
} cases[] = {
    {"roundtrip",     test_attach_roundtrip     },
    {"alias",         test_two_attachments_alias},
    {"fork",          test_shared_across_fork   },
    {"survives",      test_survives_creator     },
    {"deferred-rmid", test_deferred_removal     },
    {"keys",          test_keys_and_stale_ids   },
    {"no-leak",       test_no_leak              },
};


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("shm-test: starting\n");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {

        if (argc > 1 && strcmp(argv[1], cases[i].name) != 0)
            continue;

        cases[i].fn();
    }

    printf("shm-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
