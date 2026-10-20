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
 * @brief Reports the terminal size the kernel holds for this tty, once, or on every SIGWINCH with -w.
 */


#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>


/**
 * @brief Formats an unsigned number, by hand so that the SIGWINCH handler can report from inside itself.
 *
 * @param p The buffer to write into.
 * @param v The value to format.
 * @return One past the last digit written.
 */

static char* u32_to_dec(char* p, unsigned int v) {

    char tmp[12];
    int n = 0;

    do {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    } while (v);

    while (n--) {
        *p++ = tmp[n];
    }

    return p;
}


static int report_to(int fd, const char* prefix) {

    struct winsize ws;

    memset(&ws, 0, sizeof(ws));

    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) < 0) {
        return -1;
    }


    char line[128];
    char* p = line;

    while (*prefix) {
        *p++ = *prefix++;
    }

    p    = u32_to_dec(p, ws.ws_row);
    *p++ = ' ';
    p    = u32_to_dec(p, ws.ws_col);
    *p++ = ' ';
    *p++ = '(';
    p    = u32_to_dec(p, ws.ws_xpixel);
    *p++ = 'x';
    p    = u32_to_dec(p, ws.ws_ypixel);
    *p++ = ' ';

    for (const char* t = "pixels)\n"; *t; t++) {
        *p++ = *t;
    }

    if (write(fd, line, (size_t)(p - line)) < 0) {
        return -1;
    }

    return 0;
}


static volatile sig_atomic_t winches = 0;


static void on_winch(int signum) {

    (void)signum;

    winches++;

    report_to(STDOUT_FILENO, "winsize-test: SIGWINCH -> ");
}


int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, 0);

    const int watch = (argc > 1 && strcmp(argv[1], "-w") == 0);

    if (report_to(STDOUT_FILENO, "winsize-test: ") < 0) {
        fprintf(stderr, "winsize-test: ioctl(TIOCGWINSZ) failed: %s\n", strerror(errno));
        return 1;
    }

    if (!watch) {
        return 0;
    }


    signal(SIGWINCH, on_winch);

    printf("winsize-test: watching for SIGWINCH, press a key to stop\n");


    char c;

    while (read(STDIN_FILENO, &c, 1) < 0 && errno == EINTR) {
        continue;
    }

    printf("winsize-test: stopped after %d SIGWINCH(es)\n", (int)winches);

    return 0;
}
