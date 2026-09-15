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

/*
 * Stress test for the VFS dentry cache under concurrent path resolution.
 *
 * Both cases here race several processes through one directory at once, which is only
 * interesting on more than one CPU: with -smp 1 the kernel never has two lookups of the
 * same name in flight, and everything below passes trivially.
 *
 * The kernel used to panic on either case --
 *
 *   Assert failed on vfs_dcache_add() in fs/dcache.c:57:
 *       'hashmap_get(&parent->dcache, inode->name) == NULL'
 *
 * -- because a cache miss and the insertion that follows it were not one atomic step, so
 * two CPUs could both miss the same name and both go on to resolve and cache it.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


#define WORKERS 4
#define LOOKUPS 32
#define CREATES 192

/* limits.h only defines NAME_MAX/PATH_MAX behind a feature-test macro this build does not
   set, and a name out of readdir() cannot exceed d_name anyway. */
#define NAMELEN 256
#define PATHLEN 512

#define SCRATCH  "/tmp/dcache-test.d"
#define COLD_DIR "/bin"


static int failures = 0;


#define CHECK(cond, ...)                           \
    do {                                           \
        if (!(cond)) {                             \
            fprintf(stderr, "FAIL: " __VA_ARGS__); \
            fprintf(stderr, "\n");                 \
            failures++;                            \
        }                                          \
    } while (0)


/* Names are collected with readdir(), which resolves nothing: the cache is still cold for
   every one of them when the workers start, which is the state the race needs. */
static size_t collect(const char* path, char (*names)[NAMELEN], size_t max) {

    DIR* d = opendir(path);

    if (!d)
        return perror("opendir"), 0;


    size_t n = 0;
    struct dirent* e;

    while (n < max && (e = readdir(d)) != NULL) {

        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;

        strncpy(names[n], e->d_name, NAMELEN - 1);
        names[n][NAMELEN - 1] = '\0';
        n++;
    }

    closedir(d);
    return n;
}


/* Forks WORKERS children parked on a read() and releases them together, so they enter the
   directory at the same time instead of one after another. Returns the number of children
   that exited 0. */
static int race(void (*worker)(int), int* forked) {

    int gate[2];

    if (pipe(gate) < 0)
        return perror("pipe"), -1;


    pid_t pid[WORKERS];
    int n = 0;

    for (int i = 0; i < WORKERS; i++) {

        if ((pid[n] = fork()) < 0) {
            perror("fork");
            break;
        }

        if (pid[n] == 0) {

            char b;
            close(gate[1]);
            read(gate[0], &b, 1);
            close(gate[0]);

            //? Anything the parent tripped before the fork is its own; the exit status has
            //? to report only what this worker saw.
            failures = 0;

            worker(i);
            _exit(failures ? 1 : 0);
        }

        n++;
    }

    close(gate[0]);
    close(gate[1]);


    int ok = 0;

    for (int i = 0; i < n; i++) {

        int status = 0;
        waitpid(pid[i], &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            ok++;
        else
            fprintf(stderr, "FAIL: worker %d exited with status %d\n", i, status);
    }

    *forked = n;
    return ok;
}


static char cold[LOOKUPS][NAMELEN];
static size_t cold_count = 0;


/* Every worker walks the same names in the same order, so they stay bunched together on
   whichever name is currently uncached rather than spreading out over the directory. */
static void lookup_worker(int id) {

    (void)id;

    for (size_t i = 0; i < cold_count; i++) {

        char path[PATHLEN];
        snprintf(path, sizeof(path), "%s/%s", COLD_DIR, cold[i]);

        struct stat st;

        if (stat(path, &st) < 0)
            CHECK(0, "stat(%s): %s", path, strerror(errno));
    }
}


static void creat_worker(int id) {

    (void)id;

    for (int i = 0; i < CREATES; i++) {

        char path[PATHLEN];
        snprintf(path, sizeof(path), "%s/f%d", SCRATCH, i);

        int fd = open(path, O_CREAT | O_WRONLY, 0644);

        if (fd < 0)
            CHECK(0, "open(%s): %s", path, strerror(errno));
        else
            close(fd);
    }
}


/* A name created twice is the visible half of the same race: two CPUs both miss the cache,
   both decide the name is absent and both call into the filesystem to create it, leaving
   the directory with two entries for one name. */
static void check_no_duplicates(void) {

    DIR* d = opendir(SCRATCH);

    if (!d)
        return perror("opendir"), (void)failures++;


    static unsigned char seen[CREATES];
    struct dirent* e;
    int unexpected = 0;

    memset(seen, 0, sizeof(seen));

    while ((e = readdir(d)) != NULL) {

        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;

        int i = -1;

        if (sscanf(e->d_name, "f%d", &i) != 1 || i < 0 || i >= CREATES) {
            unexpected++;
            continue;
        }

        CHECK(seen[i] == 0, "%s/%s listed more than once", SCRATCH, e->d_name);
        seen[i]++;
    }

    closedir(d);

    CHECK(unexpected == 0, "%d unexpected entries in %s", unexpected, SCRATCH);

    for (int i = 0; i < CREATES; i++)
        CHECK(seen[i] == 1, "%s/f%d listed %d times, expected 1", SCRATCH, i, (int)seen[i]);
}


int main(void) {

    printf("dcache-test: %d workers\n", WORKERS);


    cold_count = collect(COLD_DIR, cold, LOOKUPS);
    CHECK(cold_count > 0, "no entries collected from %s", COLD_DIR);

    printf("dcache-test: racing lookups of %zu uncached names in %s\n", cold_count, COLD_DIR);

    int forked = 0;
    int ok     = race(lookup_worker, &forked);

    CHECK(forked == WORKERS, "only %d of %d lookup workers forked", forked, WORKERS);
    CHECK(ok == forked, "only %d of %d lookup workers succeeded", ok, forked);


    if (mkdir(SCRATCH, 0755) < 0 && errno != EEXIST)
        CHECK(0, "mkdir(%s): %s", SCRATCH, strerror(errno));

    printf("dcache-test: racing creation of %d names in %s\n", CREATES, SCRATCH);

    ok = race(creat_worker, &forked);

    CHECK(forked == WORKERS, "only %d of %d creat workers forked", forked, WORKERS);
    CHECK(ok == forked, "only %d of %d creat workers succeeded", ok, forked);

    check_no_duplicates();


    printf("dcache-test: %s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
