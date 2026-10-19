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

/**
 * @brief Tests for execve() of #! scripts; with SCRIPT_EXEC_TEST_DUMP set it prints its arguments, serving as the interpreter.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>


#define SELF    "/usr/bin/script-exec-test"
#define SCRATCH "/tmp/script-exec-test.d"
#define DUMP    "SCRIPT_EXEC_TEST_DUMP"


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                               \
    {                                                                             \
        total++;                                                                  \
        if (cond) {                                                               \
            printf("script-exec-test: PASS  %s\n", (name));                       \
        } else {                                                                  \
            failures++;                                                           \
            printf("script-exec-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                         \
    }


/**
 * @brief Writes a script with some mode.
 *
 * @param path The script.
 * @param text Its content.
 * @param mode Its mode.
 */
static void script(const char* path, const char* text, mode_t mode) {

    unlink(path);

    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);

    if (fd < 0) {
        printf("script-exec-test: open(%s): %s\n", path, strerror(errno));
        return;
    }

    if (write(fd, text, strlen(text)) != (ssize_t)strlen(text)) {
        printf("script-exec-test: write(%s): %s\n", path, strerror(errno));
    }

    close(fd);
    chmod(path, mode);
}


/**
 * @brief Runs execv() in a child and collects what it prints, or the errno execv() failed with.
 *
 * @param path The file to execute.
 * @param argv Its arguments.
 * @param out Receives the output, or "errno=N" when execv() fails.
 * @param size The size of @p out.
 */
static void run(const char* path, char* const argv[], char* out, size_t size) {

    int fds[2];

    memset(out, 0, size);

    if (pipe(fds) < 0) {
        snprintf(out, size, "pipe: %s", strerror(errno));
        return;
    }

    const pid_t pid = fork();

    if (pid == 0) {

        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);

        execv(path, argv);

        printf("errno=%d", errno);
        fflush(stdout);
        _exit(127);
    }

    close(fds[1]);

    size_t n = 0;
    ssize_t r;

    while (n < size - 1 && (r = read(fds[0], out + n, size - 1 - n)) > 0) {
        n += r;
    }

    close(fds[0]);

    while (n > 0 && out[n - 1] == '\n') {
        out[--n] = '\0';
    }

    int status;
    waitpid(pid, &status, 0);
}


/**
 * @brief The arguments the interpreter gets: interpreter, optional argument, script path, then the arguments after the first.
 */
static void test_argv(void) {

    char out[512];

    script(SCRATCH "/dump", "#!" SELF " --dump\necho never\n", 0755);

    run(SCRATCH "/dump", (char*[]){"name", "a b", "c", NULL}, out, sizeof(out));
    CHECK(strcmp(out, SELF "|--dump|" SCRATCH "/dump|a b|c") == 0, "interpreter arguments", "'%s'", out);

    run(SCRATCH "/dump", (char*[]){NULL}, out, sizeof(out));
    CHECK(strcmp(out, SELF "|--dump|" SCRATCH "/dump") == 0, "empty argv", "'%s'", out);

    script(SCRATCH "/blanks", "#!  \t" SELF "   --dump with  space \t \n", 0755);

    run(SCRATCH "/blanks", (char*[]){"name", "x", NULL}, out, sizeof(out));
    CHECK(strcmp(out, SELF "|--dump with  space|" SCRATCH "/blanks|x") == 0, "blanks around and inside the argument", "'%s'", out);

    script(SCRATCH "/noarg", "#!" SELF "\n", 0755);

    run(SCRATCH "/noarg", (char*[]){"name", "--dump", "y", NULL}, out, sizeof(out));
    CHECK(strcmp(out, SELF "|" SCRATCH "/noarg|--dump|y") == 0, "no interpreter argument", "'%s'", out);

    script(SCRATCH "/noeol", "#!" SELF " --dump", 0755);

    run(SCRATCH "/noeol", (char*[]){"name", NULL}, out, sizeof(out));
    CHECK(strcmp(out, SELF "|--dump|" SCRATCH "/noeol") == 0, "no newline at the end", "'%s'", out);
}


/**
 * @brief A shell script runs, and a script may name another script as its interpreter.
 */
static void test_shell(void) {

    char out[512];

    script(SCRATCH "/sh", "#!/bin/sh\necho \"shell $0 $1\"\n", 0755);

    run(SCRATCH "/sh", (char*[]){"sh-script", "arg", NULL}, out, sizeof(out));
    CHECK(strcmp(out, "shell " SCRATCH "/sh arg") == 0, "shell script", "'%s'", out);

    script(SCRATCH "/inner", "#!" SELF " --dump\n", 0755);
    script(SCRATCH "/outer", "#!" SCRATCH "/inner\n", 0755);

    run(SCRATCH "/outer", (char*[]){"outer", "z", NULL}, out, sizeof(out));
    CHECK(strcmp(out, SELF "|--dump|" SCRATCH "/inner|" SCRATCH "/outer|z") == 0, "nested scripts", "'%s'", out);
}


/**
 * @brief Malformed #! lines, missing or non-executable interpreters, loops and overlong lines.
 */
static void test_errors(void) {

    char out[512];
    char expect[32];
    char line[400];

    script(SCRATCH "/empty", "#!\n", 0755);
    run(SCRATCH "/empty", (char*[]){"x", NULL}, out, sizeof(out));
    snprintf(expect, sizeof(expect), "errno=%d", ENOEXEC);
    CHECK(strcmp(out, expect) == 0, "empty #! line", "'%s'", out);

    script(SCRATCH "/blank", "#!   \t \n", 0755);
    run(SCRATCH "/blank", (char*[]){"x", NULL}, out, sizeof(out));
    CHECK(strcmp(out, expect) == 0, "blank #! line", "'%s'", out);

    memset(line, 0, sizeof(line));
    strcpy(line, "#!" SELF " --dump ");
    memset(line + strlen(line), 'a', 300);
    script(SCRATCH "/long", line, 0755);
    run(SCRATCH "/long", (char*[]){"x", NULL}, out, sizeof(out));
    CHECK(strcmp(out, expect) == 0, "#! line longer than 256 bytes", "'%s'", out);

    script(SCRATCH "/missing", "#!/nonexistent/interpreter\n", 0755);
    run(SCRATCH "/missing", (char*[]){"x", NULL}, out, sizeof(out));
    snprintf(expect, sizeof(expect), "errno=%d", ENOENT);
    CHECK(strcmp(out, expect) == 0, "missing interpreter", "'%s'", out);

    script(SCRATCH "/plain", "not a program\n", 0644);
    script(SCRATCH "/noexec", "#!" SCRATCH "/plain\n", 0755);
    run(SCRATCH "/noexec", (char*[]){"x", NULL}, out, sizeof(out));
    snprintf(expect, sizeof(expect), "errno=%d", EACCES);
    CHECK(strcmp(out, expect) == 0, "interpreter without execute permission", "'%s'", out);

    script(SCRATCH "/locked", "#!" SELF " --dump\n", 0644);
    run(SCRATCH "/locked", (char*[]){"x", NULL}, out, sizeof(out));
    CHECK(strcmp(out, expect) == 0, "script without execute permission", "'%s'", out);

    script(SCRATCH "/loop", "#!" SCRATCH "/loop\n", 0755);
    run(SCRATCH "/loop", (char*[]){"x", NULL}, out, sizeof(out));
    snprintf(expect, sizeof(expect), "errno=%d", ELOOP);
    CHECK(strcmp(out, expect) == 0, "script that is its own interpreter", "'%s'", out);
}


/**
 * @brief Prints the arguments joined by '|' when serving as an interpreter, or runs every test.
 *
 * @param argc The number of arguments.
 * @param argv The arguments.
 * @return 0 when every test passes, 1 otherwise.
 */
int main(int argc, char** argv) {

    if (getenv(DUMP)) {

        for (int i = 0; i < argc; i++) {
            printf("%s%s", i ? "|" : "", argv[i]);
        }

        printf("\n");
        return 0;
    }


    setvbuf(stdout, NULL, _IONBF, 0);

    system("rm -rf " SCRATCH);

    if (mkdir(SCRATCH, 0755) < 0) {
        printf("script-exec-test: mkdir(%s): %s\n", SCRATCH, strerror(errno));
        return 1;
    }

    setenv(DUMP, "1", 1);

    printf("script-exec-test: starting\n");

    test_argv();
    test_shell();
    test_errors();

    system("rm -rf " SCRATCH);

    printf("script-exec-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
