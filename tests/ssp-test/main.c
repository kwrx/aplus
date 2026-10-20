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
 * @brief Checks that a program built with the stack protector starts, and that AT_RANDOM points at fresh random bytes.
 *
 * musl seeds the stack canary by reading the 16 bytes AT_RANDOM points at, before main() runs.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/wait.h>
#include <unistd.h>


/**
 * @brief Where the test installs itself, so it can run itself again.
 */
#define SSP_TEST_PATH "/usr/bin/ssp-test"


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                      \
    {                                                                    \
        total++;                                                         \
        if (cond) {                                                      \
            printf("ssp-test: PASS  %s\n", (name));                      \
        } else {                                                         \
            failures++;                                                  \
            printf("ssp-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                \
    }


/**
 * @brief Copies the 16 bytes AT_RANDOM points at.
 *
 * @param out Receives them.
 * @return 0 on success, -1 when the entry is missing.
 */
static int read_at_random(unsigned char out[16]) {

    const unsigned char* p = (const unsigned char*)getauxval(AT_RANDOM);

    if (!p)
        return -1;

    memcpy(out, p, 16);

    return 0;
}


/**
 * @brief Runs the test again and reads back the AT_RANDOM bytes that run was given.
 *
 * @param out Receives them.
 * @return The child's wait status, or -1.
 */
static int exec_dump(unsigned char out[16]) {

    int fd[2];

    if (pipe(fd) < 0)
        return -1;

    pid_t pid = fork();

    if (pid < 0)
        return -1;

    if (pid == 0) {

        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);

        char* args[] = {"ssp-test", "--dump", NULL};

        execv(SSP_TEST_PATH, args);
        _exit(3);
    }

    close(fd[1]);

    size_t got = 0;

    while (got < 16) {

        ssize_t n = read(fd[0], out + got, 16 - got);

        if (n <= 0)
            break;

        got += (size_t)n;
    }

    close(fd[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    return got == 16 ? status : -1;
}


/**
 * @brief Checks that AT_RANDOM points at 16 readable bytes that are not all zero.
 */
static void test_at_random_present(void) {

    unsigned char bytes[16];

    int e = read_at_random(bytes);

    unsigned nonzero = 0;

    for (int i = 0; e == 0 && i < 16; i++)
        nonzero |= bytes[i];

    CHECK(e == 0 && nonzero, "at-random-present", "getauxval(AT_RANDOM) %s", e ? "is missing" : "points at 16 zero bytes");
}


/**
 * @brief Checks that two programs started one after the other get different AT_RANDOM bytes.
 */
static void test_at_random_per_exec(void) {

    unsigned char a[16], b[16];

    int sa = exec_dump(a);
    int sb = exec_dump(b);

    int started = sa >= 0 && sb >= 0 && WIFEXITED(sa) && WEXITSTATUS(sa) == 0 && WIFEXITED(sb) && WEXITSTATUS(sb) == 0;

    CHECK(started && memcmp(a, b, 16) != 0, "at-random-per-exec", "child statuses 0x%x and 0x%x, bytes %s", sa, sb, started ? "identical" : "not read");
}


int main(int argc, char** argv) {

    if (argc > 1 && strcmp(argv[1], "--dump") == 0) {

        unsigned char bytes[16];

        if (read_at_random(bytes) < 0)
            return 2;

        return write(STDOUT_FILENO, bytes, 16) == 16 ? 0 : 2;
    }

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("ssp-test: starting\n");

    test_at_random_present();
    test_at_random_per_exec();

    printf("ssp-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures == 0 ? 0 : 1;
}
