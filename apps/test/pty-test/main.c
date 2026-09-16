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
 * @brief Tests for pseudo-terminals, the case that matters most being more than one at a time.
 */

#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                              \
    {                                                            \
        total++;                                                 \
        if (cond) {                                              \
            printf("pty-test: PASS  %s\n", (name));               \
        } else {                                                 \
            failures++;                                          \
            printf("pty-test: FAIL  %s: " fmt "\n", (name), ##__VA_ARGS__); \
        }                                                        \
    }


#define PTY_MAX 4


int main(int argc, char** argv) {

    (void)argc;
    (void)argv;

    setvbuf(stdout, NULL, _IONBF, 0);


    int master[PTY_MAX];
    int slave[PTY_MAX];

    for (int i = 0; i < PTY_MAX; i++) {
        master[i] = -1;
        slave[i]  = -1;
    }


    for (int i = 0; i < PTY_MAX; i++) {

        char name[32];

        snprintf(name, sizeof(name), "openpty-%d", i);

        struct termios raw;

        memset(&raw, 0, sizeof(raw));
        cfmakeraw(&raw);

        raw.c_cc[VMIN]  = 1;
        raw.c_cc[VTIME] = 0;

        int e = openpty(&master[i], &slave[i], NULL, &raw, NULL);

        CHECK(e == 0, name, "openpty() failed: %s", strerror(errno));

        if (e != 0) {
            break;
        }
    }


    for (int i = 0; i < PTY_MAX; i++) {

        if (master[i] < 0) {
            continue;
        }


        char name[32];
        char buf[16] = {0};

        snprintf(name, sizeof(name), "roundtrip-%d", i);

        const char payload = (char)('a' + i);

        if (write(master[i], &payload, 1) != 1) {
            CHECK(0, name, "write() failed: %s", strerror(errno));
            continue;
        }

        ssize_t got = read(slave[i], buf, sizeof(buf));

        CHECK(got >= 1 && buf[0] == payload, name, "read() returned %zd, buf[0]=%#x", got, (unsigned)buf[0]);
    }


    for (int i = 0; i < PTY_MAX; i++) {

        if (master[i] >= 0) {
            close(master[i]);
        }

        if (slave[i] >= 0) {
            close(slave[i]);
        }
    }


    printf("pty-test: %d/%d passed\n", total - failures, total);

    return failures ? 1 : 0;
}
