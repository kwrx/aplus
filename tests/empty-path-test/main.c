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
 * @brief Tests that an empty path names nothing: ENOENT from every path syscall, except under AT_EMPTY_PATH.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


static int failures = 0;
static int total    = 0;


/**
 * @brief Checks that a call failed with ENOENT.
 *
 * @param name The call being checked.
 * @param r What it returned.
 */
static void expect_enoent(const char* name, long r) {

    total++;

    if (r < 0 && errno == ENOENT) {
        printf("empty-path-test: PASS  %s\n", name);
    } else {
        failures++;
        printf("empty-path-test: FAIL  %s: returned %ld, errno %d (%s)\n", name, r, errno, strerror(errno));
    }
}


/**
 * @brief Checks that a call succeeded.
 *
 * @param name The call being checked.
 * @param r What it returned.
 */
static void expect_ok(const char* name, long r) {

    total++;

    if (r >= 0) {
        printf("empty-path-test: PASS  %s\n", name);
    } else {
        failures++;
        printf("empty-path-test: FAIL  %s: %s\n", name, strerror(errno));
    }
}


int main(void) {

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("empty-path-test: starting\n");

    if (chdir("/tmp") < 0) {
        printf("empty-path-test: chdir /tmp: %s\n", strerror(errno));
        return 1;
    }

    struct stat st;
    char buf[64];

    const int dfd = open("/tmp", O_RDONLY | O_DIRECTORY);
    expect_ok("open /tmp", dfd);

    errno = 0;
    expect_enoent("open(\"\")", open("", O_RDONLY));
    errno = 0;
    expect_enoent("open(\"\", O_CREAT)", open("", O_RDWR | O_CREAT, 0644));
    errno = 0;
    expect_enoent("creat(\"\")", creat("", 0644));
    errno = 0;
    expect_enoent("openat(dfd, \"\")", openat(dfd, "", O_RDONLY));

    errno = 0;
    FILE* f = fopen("", "r");
    expect_enoent("fopen(\"\")", f ? 0 : -1);
    if (f) {
        fclose(f);
    }

    errno = 0;
    expect_enoent("stat(\"\")", stat("", &st));
    errno = 0;
    expect_enoent("lstat(\"\")", lstat("", &st));
    errno = 0;
    expect_enoent("access(\"\")", access("", F_OK));
    errno = 0;
    expect_enoent("chdir(\"\")", chdir(""));
    errno = 0;
    expect_enoent("mkdir(\"\")", mkdir("", 0755));
    errno = 0;
    expect_enoent("unlink(\"\")", unlink(""));
    errno = 0;
    expect_enoent("rmdir(\"\")", rmdir(""));
    errno = 0;
    expect_enoent("chmod(\"\")", chmod("", 0644));
    errno = 0;
    expect_enoent("truncate(\"\")", truncate("", 0));
    errno = 0;
    expect_enoent("readlink(\"\")", readlink("", buf, sizeof(buf)));
    errno = 0;
    expect_enoent("execve(\"\")", execve("", (char* const[]){"", NULL}, (char* const[]){NULL}));

    expect_ok("fstatat(dfd, \"\", AT_EMPTY_PATH)", fstatat(dfd, "", &st, AT_EMPTY_PATH));
    expect_ok("open(\".\")", open(".", O_RDONLY));

    printf("empty-path-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
