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


#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>


#define SELF    "/usr/bin/path-resolve-test"
#define SCRATCH "/tmp/path-resolve-test.d"


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                               \
    {                                                                             \
        total++;                                                                  \
        if (cond) {                                                               \
            printf("path-resolve-test: PASS  %s\n", (name));                      \
        } else {                                                                  \
            failures++;                                                           \
            printf("path-resolve-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                         \
    }


static int fill_fds(int* fds, int max) {

    int n = 0;

    while (n < max) {

        const int fd = open("/dev/null", O_RDONLY);

        if (fd < 0)
            break;

        fds[n++] = fd;
    }

    return n;
}


static void release_fds(int* fds, int n) {

    for (int i = 0; i < n; i++)
        close(fds[i]);
}


static void run(const char* path, int full, char* out, size_t size) {

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

        if (full) {
            static int spare[4096];
            fill_fds(spare, 4096);
        }

        execv(path, (char*[]){"x", "--hello", NULL});

        printf("errno=%d", errno);
        fflush(stdout);
        _exit(127);
    }

    close(fds[1]);

    size_t n = 0;
    ssize_t r;

    while (n < size - 1 && (r = read(fds[0], out + n, size - 1 - n)) > 0)
        n += r;

    close(fds[0]);

    while (n > 0 && out[n - 1] == '\n')
        out[--n] = '\0';

    int status;
    waitpid(pid, &status, 0);
}


static void test_mount(void) {

    int r;

    mkdir(SCRATCH "/mnt", 0755);
    mkdir(SCRATCH "/full", 0755);
    mkdir(SCRATCH "/target", 0755);
    symlink(SCRATCH "/target", SCRATCH "/link");
    close(open(SCRATCH "/file", O_WRONLY | O_CREAT, 0644));

    r = mount("none", SCRATCH "/mnt", "tmpfs", 0, NULL);
    CHECK(r == 0, "mount tmpfs on a directory", "%d errno %d (%s)", r, errno, strerror(errno));

    r = close(open(SCRATCH "/mnt/inside", O_WRONLY | O_CREAT, 0644));
    struct stat st;
    CHECK(r == 0 && stat(SCRATCH "/mnt/inside", &st) == 0, "create a file in the mounted tmpfs", "errno %d (%s)", errno, strerror(errno));

    errno = 0;
    r     = mount("none", SCRATCH "/file", "tmpfs", 0, NULL);
    CHECK(r < 0 && errno == ENOTDIR, "mount on a regular file is ENOTDIR", "%d errno %d (%s)", r, errno, strerror(errno));

    errno = 0;
    r     = mount("none", SCRATCH "/missing", "tmpfs", 0, NULL);
    CHECK(r < 0 && errno == ENOENT, "mount on a missing path is ENOENT", "%d errno %d (%s)", r, errno, strerror(errno));

    errno = 0;
    r     = mount("none", "", "tmpfs", 0, NULL);
    CHECK(r < 0 && errno == ENOENT, "mount on an empty path is ENOENT", "%d errno %d (%s)", r, errno, strerror(errno));

    errno = 0;
    r     = mount("/dev/missing", SCRATCH "/target", "ext2", 0, NULL);
    CHECK(r < 0 && errno == ENOENT, "mount of a missing device is ENOENT", "%d errno %d (%s)", r, errno, strerror(errno));

    r = mount("none", SCRATCH "/link", "tmpfs", 0, NULL);
    CHECK(r == 0, "mount through a symlink to a directory", "%d errno %d (%s)", r, errno, strerror(errno));

    static int fds[4096];
    const int n = fill_fds(fds, 4096);

    errno = 0;
    r     = mount("none", SCRATCH "/full", "tmpfs", 0, NULL);
    const int e = errno;

    release_fds(fds, n);

    CHECK(r == 0, "mount with a full descriptor table", "%d errno %d (%s), %d fds open", r, e, strerror(e), n);
}


static void test_exec(void) {

    char out[256];

    run(SELF, 0, out, sizeof(out));
    CHECK(strcmp(out, "hello") == 0, "execve of the test itself", "'%s'", out);

    symlink(SELF, SCRATCH "/self");
    run(SCRATCH "/self", 0, out, sizeof(out));
    CHECK(strcmp(out, "hello") == 0, "execve through a symlink", "'%s'", out);

    run(SELF, 1, out, sizeof(out));
    CHECK(strcmp(out, "hello") == 0, "execve with a full descriptor table", "'%s'", out);

    char expect[32];

    run(SCRATCH, 0, out, sizeof(out));
    snprintf(expect, sizeof(expect), "errno=%d", ENOEXEC);
    CHECK(strcmp(out, expect) == 0, "execve of a directory", "'%s'", out);

    run(SCRATCH "/file", 0, out, sizeof(out));
    CHECK(strcmp(out, expect) == 0, "execve of an empty file", "'%s'", out);

    run(SCRATCH "/nothing", 0, out, sizeof(out));
    snprintf(expect, sizeof(expect), "errno=%d", ENOENT);
    CHECK(strcmp(out, expect) == 0, "execve of a missing file", "'%s'", out);

    run("", 0, out, sizeof(out));
    CHECK(strcmp(out, expect) == 0, "execve of an empty path", "'%s'", out);

    const int fd = open(SCRATCH "/plain", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    write(fd, "data\n", 5);
    close(fd);
    chmod(SCRATCH "/plain", 0644);

    run(SCRATCH "/plain", 0, out, sizeof(out));
    snprintf(expect, sizeof(expect), "errno=%d", EACCES);
    CHECK(strcmp(out, expect) == 0, "execve without execute permission", "'%s'", out);
}


int main(int argc, char** argv) {

    if (argc > 1 && strcmp(argv[1], "--hello") == 0) {
        printf("hello\n");
        return 0;
    }


    setvbuf(stdout, NULL, _IONBF, 0);

    system("rm -rf " SCRATCH);

    if (mkdir(SCRATCH, 0755) < 0) {
        printf("path-resolve-test: mkdir(%s): %s\n", SCRATCH, strerror(errno));
        return 1;
    }

    printf("path-resolve-test: starting\n");

    test_mount();
    test_exec();

    printf("path-resolve-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
