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
#include <sys/select.h>
#include <sys/wait.h>
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


/**
 * @brief Asks select() whether a descriptor is readable.
 *
 * @param fd The descriptor.
 * @param timeout_ms How long select() may wait, 0 for a probe.
 * @return 1 when readable, 0 when not, -1 when select() failed.
 */
static int readable(int fd, int timeout_ms) {

    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    struct timeval tv = {
        .tv_sec  = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000,
    };

    if (select(fd + 1, &rfds, NULL, NULL, &tv) < 0) {
        return -1;
    }

    return FD_ISSET(fd, &rfds) ? 1 : 0;
}


/**
 * @brief Checks that each end of a raw pty is readable only while the other end's bytes are waiting in it.
 *
 * @param master The master descriptor.
 * @param slave The slave descriptor.
 */
static void test_readiness(int master, int slave) {

    char buf[16];

    CHECK(readable(slave, 0) == 0, "slave-empty", "%s", "an empty slave is reported readable");
    CHECK(readable(master, 0) == 0, "master-empty", "%s", "an empty master is reported readable");

    if (write(master, "x", 1) != 1) {
        CHECK(0, "slave-input", "write() failed: %s", strerror(errno));
        return;
    }

    CHECK(readable(slave, 0) == 1, "slave-input", "%s", "the slave is not readable after the master wrote");
    CHECK(read(slave, buf, sizeof(buf)) == 1, "slave-input-read", "%s", "the slave did not return the byte");
    CHECK(readable(slave, 0) == 0, "slave-drained", "%s", "a drained slave is reported readable");

    if (write(slave, "y", 1) != 1) {
        CHECK(0, "master-output", "write() failed: %s", strerror(errno));
        return;
    }

    CHECK(readable(master, 0) == 1, "master-output", "%s", "the master is not readable after the slave wrote");
    CHECK(read(master, buf, sizeof(buf)) == 1, "master-output-read", "%s", "the master did not return the byte");
    CHECK(readable(master, 0) == 0, "master-drained", "%s", "a drained master is reported readable");
}


/**
 * @brief Checks that a select() sleeping on an empty slave wakes up when the master writes.
 *
 * @param master The master descriptor.
 * @param slave The slave descriptor.
 */
static void test_wakeup(int master, int slave) {

    pid_t pid = fork();

    if (pid < 0) {
        CHECK(0, "slave-wakeup", "fork() failed: %s", strerror(errno));
        return;
    }

    if (pid == 0) {
        usleep(200000);
        _exit(write(master, "z", 1) == 1 ? 0 : 1);
    }

    const int r = readable(slave, 3000);

    CHECK(r == 1, "slave-wakeup", "select() returned %d instead of waking up on input", r);

    waitpid(pid, NULL, 0);

    char buf[16];

    if (r == 1) {
        (void)read(slave, buf, sizeof(buf));
    }
}


/**
 * @brief Reads from the slave only when select() says it would not block.
 *
 * @param slave The slave descriptor.
 * @param buf The buffer.
 * @param size The size of the buffer.
 * @return What read() returned, or -2 when the slave was not readable.
 */
static ssize_t read_ready(int slave, char* buf, size_t size) {

    memset(buf, 0, size);

    if (readable(slave, 0) != 1) {
        return -2;
    }

    return read(slave, buf, size);
}


/**
 * @brief Checks that VEOF releases a partial line, reads as end-of-file on an empty one, and is plain data in raw mode.
 *
 * @param master The master descriptor.
 * @param slave The slave descriptor.
 */
static void test_eof(int master, int slave) {

    struct termios saved;

    if (tcgetattr(slave, &saved) < 0) {
        CHECK(0, "eof-setup", "tcgetattr() failed: %s", strerror(errno));
        return;
    }

    struct termios ios = saved;

    ios.c_lflag |= ICANON | ISIG;
    ios.c_lflag &= ~(ECHO | ECHONL);
    ios.c_cc[VEOF] = 0x04;

    if (tcsetattr(slave, TCSANOW, &ios) < 0) {
        CHECK(0, "eof-setup", "tcsetattr() failed: %s", strerror(errno));
        return;
    }


    char buf[16];
    ssize_t got;

    if (write(master, "abc\x04", 4) == 4) {
        got = read_ready(slave, buf, sizeof(buf));
        CHECK(got == 3 && memcmp(buf, "abc", 3) == 0, "eof-partial-line", "read() returned %zd instead of \"abc\"", got);
    }

    if (write(master, "\x04", 1) == 1) {
        got = read_ready(slave, buf, sizeof(buf));
        CHECK(got == 0, "eof-empty-line", "read() returned %zd instead of 0", got);
        CHECK(readable(slave, 0) == 0, "eof-consumed", "%s", "the slave is still readable after the end-of-file was read");
    }

    if (write(master, "line\n\x04", 6) == 6) {
        got = read_ready(slave, buf, sizeof(buf));
        CHECK(got == 5 && memcmp(buf, "line\n", 5) == 0, "eof-after-line", "read() returned %zd instead of the line", got);
        got = read_ready(slave, buf, sizeof(buf));
        CHECK(got == 0, "eof-after-line-eof", "read() returned %zd instead of 0", got);
    }

    if (write(master, "\x04", 1) == 1) {

        ios.c_lflag &= ~ICANON;
        ios.c_cc[VMIN]  = 1;
        ios.c_cc[VTIME] = 0;

        if (tcsetattr(slave, TCSANOW, &ios) == 0) {

            CHECK(readable(slave, 0) == 0, "eof-dropped-raw", "%s", "a pending end-of-file survived leaving canonical mode");

            if (write(master, "\x04", 1) == 1) {
                got = read_ready(slave, buf, sizeof(buf));
                CHECK(got == 1 && buf[0] == 0x04, "eof-raw-data", "read() returned %zd, buf[0]=%#x", got, (unsigned)buf[0]);
            }
        }
    }

    tcsetattr(slave, TCSANOW, &saved);
}


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


    if (master[0] >= 0) {
        test_readiness(master[0], slave[0]);
        test_wakeup(master[0], slave[0]);
        test_eof(master[0], slave[0]);
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
