/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
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

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


#define HANDLER_EXIT_CODE 42


/**
 * @brief A null pointer the compiler cannot constant-fold, so the store below survives optimization.
 */
static int* volatile null_pointer = NULL;


/**
 * @brief Writes through a null pointer, raising a page fault.
 */
static void fault_null(void) {

    int* p = null_pointer;

    __asm__ __volatile__("" : "+r"(p));

    *p = 0x1234;
}


/**
 * @brief Reads sixteen bytes with movdqa from an address that is 8 modulo 16, raising a general protection fault.
 */
static void fault_movdqa(void) {

    static unsigned char buffer[64];

    unsigned char* p = buffer + ((16UL - ((uintptr_t)buffer & 15UL)) & 15UL) + 8UL;

    __asm__ __volatile__("movdqa (%0), %%xmm0" : : "r"(p) : "xmm0", "memory");
}


/**
 * @brief Blocks and ignores every signal it can, then faults, so that only forced delivery can end the process.
 */
static void fault_blocked(void) {

    sigset_t set;

    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, NULL);

    signal(SIGSEGV, SIG_IGN);
    signal(SIGBUS, SIG_IGN);
    signal(SIGILL, SIG_IGN);

    fault_null();
}


/**
 * @brief Leaves the process with a known exit code, to show the fault reached a user handler.
 *
 * @param signo The signal being delivered.
 */
static void fault_handler(int signo) {

    (void)signo;

    _exit(HANDLER_EXIT_CODE);
}


/**
 * @brief Installs a SIGSEGV handler and faults, so a handled fault is still delivered to userspace.
 */
static void fault_handled(void) {

    signal(SIGSEGV, fault_handler);

    fault_null();
}


/**
 * @brief Runs one fault case in a child and checks how the kernel ended it.
 *
 * @param name The case to report under.
 * @param fn The function that faults.
 * @param expect_exit The exit code to require, or -1 to require death by signal.
 * @return 0 when the child ended as expected, 1 otherwise.
 */
static int run_case(const char* name, void (*fn)(void), int expect_exit) {

    fflush(stdout);

    pid_t pid = fork();

    if (pid < 0) {

        fprintf(stderr, "fault-test: %-10s fork failed\n", name);
        return 1;
    }

    if (pid == 0) {

        fn();
        _exit(0);
    }


    int status = 0;

    if (waitpid(pid, &status, 0) != pid) {

        fprintf(stderr, "fault-test: %-10s waitpid failed\n", name);
        return 1;
    }


    if (expect_exit < 0) {

        if (WIFSIGNALED(status)) {

            printf("fault-test: %-10s ok, pid(%d) killed by signal %d%s\n", name, pid, WTERMSIG(status), WCOREDUMP(status) ? " (core dumped)" : "");
            return 0;
        }

        printf("fault-test: %-10s FAILED, pid(%d) exited with %d, expected death by signal\n", name, pid, WEXITSTATUS(status));
        return 1;
    }


    if (WIFEXITED(status) && WEXITSTATUS(status) == expect_exit) {

        printf("fault-test: %-10s ok, pid(%d) handled the fault and exited with %d\n", name, pid, expect_exit);
        return 0;
    }

    if (WIFSIGNALED(status)) {

        printf("fault-test: %-10s FAILED, pid(%d) killed by signal %d, expected exit %d\n", name, pid, WTERMSIG(status), expect_exit);
        return 1;
    }

    printf("fault-test: %-10s FAILED, pid(%d) exited with %d, expected %d\n", name, pid, WEXITSTATUS(status), expect_exit);
    return 1;
}


int main(int argc, char** argv) {

    if (argc > 1) {

        if (strcmp(argv[1], "null") == 0) {
            fault_null();
        } else if (strcmp(argv[1], "movdqa") == 0) {
            fault_movdqa();
        } else if (strcmp(argv[1], "blocked") == 0) {
            fault_blocked();
        } else if (strcmp(argv[1], "handled") == 0) {
            fault_handled();
        } else {
            fprintf(stderr, "usage: %s [null|movdqa|blocked|handled]\n", argv[0]);
            return 2;
        }

        return 0;
    }


    int failures = 0;

    failures += run_case("null", fault_null, -1);
    failures += run_case("movdqa", fault_movdqa, -1);
    failures += run_case("blocked", fault_blocked, -1);
    failures += run_case("handled", fault_handled, HANDLER_EXIT_CODE);

    printf("fault-test: %s (%d failure%s)\n", failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");

    return failures ? 1 : 0;
}
