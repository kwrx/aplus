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


#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>


static void show_usage(int argc, char** argv) {
    printf("Use: iobench [options]... DEVICE...\n"
           "Read or write to a DEVICE in benchmark mode.\n"
           "   -w                          write mode\n"
           "   -b, --block-size=SIZE       set block size\n"
           "   -t, --time-limit=SECONDS    set time limit\n"
           "       --help                  show this help\n"
           "       --version               print version info and exit\n");

    exit(0);
}

static void show_version(int argc, char** argv) {
    printf("%s (aplus coreutils) 0.1\n"
           "Copyright (c) %s Antonino Natale.\n"
           "Built with gcc %s (%s)\n",

           argv[0], &__DATE__[7], __VERSION__, __TIMESTAMP__);

    exit(0);
}



int main(int argc, char** argv) {


    static struct option long_options[] = {
        {"block-size", required_argument, NULL, 'b'},
        {"time-limit", required_argument, NULL, 't'},
        {"help",       no_argument,       NULL, 'h'},
        {"version",    no_argument,       NULL, 'q'},
        {NULL,         0,                 NULL, 0  }
    };



    int mode    = 0;
    int blksize = (4096 << 10);
    int limit   = 5;

    int c, idx;
    while ((c = getopt_long(argc, argv, "wb:t:", long_options, &idx)) != -1) {
        switch (c) {
            case 'w':
                mode = 1;
                break;
            case 'b':
                blksize = atoi(optarg) << 10;
                break;
            case 't':
                limit = atoi(optarg);
                break;
            case 'q':
                show_version(argc, argv);
                break;
            case 'h':
            case '?':
                show_usage(argc, argv);
                break;
            default:
                abort();
        }
    }


    if (optind >= argc)
        show_usage(argc, argv);


    char* device = argv[optind++];


    int fd = open(device, mode ? O_WRONLY : O_RDONLY);

    if (fd < 0) {
        perror("open");
        return -1;
    }


    char* buf = malloc(blksize);

    if (!buf) {
        perror("malloc");
        return -1;
    }

    for (int i = 0; i < blksize; i++) {
        buf[i] = i;
    }



    struct stat st;

    if (fstat(fd, &st) < 0) {
        perror("fstat");
        goto fail;
    }

    printf("%s: %ld KiB\n", device, st.st_size >> 10);


    ssize_t bytes = 0;
    ssize_t speed = 0;


    time_t curr  = time(NULL);
    time_t start = curr;
    time_t end   = start + limit;


    for (size_t i = 0; i + blksize < st.st_size; i += blksize) {

        ssize_t r = mode ? write(fd, buf, blksize) : read(fd, buf, blksize);

        if (r < 0) {
            perror(mode ? "write" : "read");
            goto fail;
        }

        bytes += r;
        speed += r;


        if (time(NULL) != curr) {

            fprintf(stderr, "%s: %ld MiB in %ld seconds (%ld MiB/s)\n", device, bytes >> 20, time(NULL) - start, speed >> 20);

            curr  = time(NULL);
            speed = 0;
        }

        if (time(NULL) >= end)
            break;
    }

    free(buf);

    fprintf(stderr, "%s: %s a total of %ld MiB in %ld seconds\n", mode ? "wrote" : "read", device, bytes >> 20, time(NULL) - start);

    return 0;


fail:
    return free(buf), 1;
}
