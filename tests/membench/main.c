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

/**
 * @brief Times a growing heap, one band at a time.
 *
 * A heap that costs more per megabyte the larger it gets is the shape this looks for: the
 * bands are all the same size, so a rising time is the allocator answering a fixed request
 * more slowly, not the program asking for more.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


#define MEMBENCH_BLOCK   4096
#define MEMBENCH_BAND    (16UL * 1024 * 1024)
#define MEMBENCH_DEFAULT (512UL * 1024 * 1024)


/**
 * @brief Reads the monotonic clock in nanoseconds.
 *
 * @return The reading, or 0 when the clock is unavailable.
 */
static uint64_t membench_now(void) {

    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        return 0;
    }

    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}


/**
 * @brief Prints how much memory the system has left, either side of the run.
 *
 * @param when A word naming the moment, printed against every line.
 */
static void membench_meminfo(const char* when) {

    FILE* fp = fopen("/proc/meminfo", "r");

    if (!fp) {
        return;
    }

    char line[256];

    while (fgets(line, sizeof(line), fp)) {

        if (strncmp(line, "Slab", 4) != 0 && strncmp(line, "MemFree", 7) != 0) {
            continue;
        }

        line[strcspn(line, "\n")] = '\0';

        printf("membench: %-6s %s\n", when, line);
    }

    fclose(fp);
}


int main(int argc, char** argv) {

    size_t total = MEMBENCH_DEFAULT;

    if (argc > 1) {

        long mib = atol(argv[1]);

        if (mib <= 0) {
            fprintf(stderr, "Use: membench [MiB]\n");
            return 1;
        }

        total = (size_t)mib * 1024 * 1024;
    }


    const size_t bands  = total / MEMBENCH_BAND;
    const size_t blocks = MEMBENCH_BAND / MEMBENCH_BLOCK;

    printf("membench: %lu MiB in %lu-byte blocks, %lu bands of %lu MiB\n", (unsigned long)(total >> 20), (unsigned long)MEMBENCH_BLOCK, (unsigned long)bands, (unsigned long)(MEMBENCH_BAND >> 20));

    membench_meminfo("start");


    const uint64_t began = membench_now();

    for (size_t band = 0; band < bands; band++) {

        const uint64_t mark = membench_now();

        for (size_t i = 0; i < blocks; i++) {

            volatile char* p = malloc(MEMBENCH_BLOCK);

            if (!p) {
                fprintf(stderr, "membench: out of memory after %lu MiB\n", (unsigned long)((band * MEMBENCH_BAND) >> 20));
                return 1;
            }

            p[0]                  = (char)i;
            p[MEMBENCH_BLOCK - 1] = (char)i;
        }

        printf("membench: band %2lu  %4lu MiB reached  %10lu us\n", (unsigned long)band, (unsigned long)(((band + 1) * MEMBENCH_BAND) >> 20), (unsigned long)((membench_now() - mark) / 1000));
    }

    printf("membench: total %lu us\n", (unsigned long)((membench_now() - began) / 1000));

    membench_meminfo("end");

    return 0;
}
