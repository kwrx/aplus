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
 * Reports the terminal size the kernel holds for this tty -- what "stty size" would
 * print, which busybox here has no applet for.
 *
 * With no arguments it prints the size once. With -w it stays up for a while and prints
 * again on every SIGWINCH, which is how a resize of the window a shell is running in can
 * be shown to reach all the way down: aplus-terminal turns the window's new size into a
 * TIOCSWINSZ on the pty master, and drivers/tty/pty raises SIGWINCH on the foreground
 * process group from there.
 */


#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>


/* Reporting from inside the handler on purpose.
 *
 * The obvious shape -- set a flag, notice it in the main loop -- needs the loop to get
 * control back, and neither way of waiting is reliable here: a blocking read() interrupted
 * by a handled signal is restarted rather than failing with EINTR, and a nanosleep() loop
 * does not pace the way it should. Doing the work in the handler takes the main loop out
 * of the question entirely, so what this prints is exactly "the signal arrived".
 *
 * write() and ioctl() are both async-signal-safe; the number formatting below is hand
 * rolled for the same reason.
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


    /* Just park here. The handler does the reporting, so this loop has nothing to do but
       keep the process alive and offer a way out.
     *
     * Caveat worth knowing when reading the output: the kernel does raise SIGWINCH on a
     * TIOCSWINSZ -- it shows up as "sched: received signal(28)" -- but it does not break a
     * task out of a blocking read() to run the handler, so the line may not appear until
     * the read returns. Run winsize-test with no arguments after a resize to see the size
     * the kernel currently holds, which is the part that does not depend on any of this.
     */
    char c;

    while (read(STDIN_FILENO, &c, 1) < 0 && errno == EINTR) {
        continue;
    }

    printf("winsize-test: stopped after %d SIGWINCH(es)\n", (int)winches);

    return 0;
}
