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
 * @brief Tests for controlling terminals: TIOCSCTTY, TIOCNOTTY, /dev/tty, inheritance across fork() and setsid(), and Ctrl-C/Ctrl-Z through the line discipline. Run without a controlling terminal.
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>


static int failures = 0;
static int total    = 0;


#define CHECK(cond, name, fmt, ...)                                        \
    {                                                                      \
        total++;                                                           \
        if (cond) {                                                        \
            printf("ctty-test: PASS  %s\n", (name));                       \
        } else {                                                           \
            failures++;                                                    \
            printf("ctty-test: FAIL  %s: " fmt "\n", (name), __VA_ARGS__); \
        }                                                                  \
    }


static int master = -1;
static int slave  = -1;


/**
 * @brief Tells whether the caller has a controlling terminal, by opening /dev/tty.
 *
 * @return true if /dev/tty opened.
 */
static bool has_ctty(void) {

    int fd = open("/dev/tty", O_RDWR | O_NOCTTY);

    if (fd < 0)
        return false;

    close(fd);

    return true;
}

/**
 * @brief Runs a function in a child and reports how the child ended.
 *
 * @param fn The function; its return value is the child's exit status.
 * @return The exit status, or 100 plus the signal that killed the child.
 */
static int run(int (*fn)(void)) {

    pid_t pid = fork();

    if (pid == 0)
        _exit(fn());

    int status = 0;

    waitpid(pid, &status, 0);

    return WIFEXITED(status) ? WEXITSTATUS(status) : 100 + WTERMSIG(status);
}


/**
 * @brief A forked member of the session drops the inherited terminal with TIOCNOTTY, which must not hang up the foreground group it belongs to.
 *
 * @return 0, or the number of the step that failed.
 */
static int member_notty(void) {

    if (!has_ctty())
        return 1;

    int fd = open("/dev/tty", O_RDWR | O_NOCTTY);

    if (fd < 0 || ioctl(fd, TIOCNOTTY, 0) < 0)
        return 2;

    close(fd);

    return has_ctty() ? 3 : 0;
}

/**
 * @brief A forked member that starts its own session loses the inherited terminal.
 *
 * @return 0, or the number of the step that failed.
 */
static int member_setsid(void) {

    if (setsid() < 0)
        return 1;

    return has_ctty() ? 2 : 0;
}

/**
 * @brief A new session takes a terminal its previous session gave up.
 *
 * @return 0, or the number of the step that failed.
 */
static int member_reclaim(void) {

    if (setsid() < 0)
        return 1;

    if (ioctl(slave, TIOCSCTTY, 0) < 0)
        return 2;

    return has_ctty() ? 0 : 3;
}

/**
 * @brief A session leader acquires, keeps and gives up a controlling terminal under Linux's rules, and its children inherit it.
 *
 * @return 0, or the number of the step that failed.
 */
static int session_leader(void) {

    errno = 0;

    if (ioctl(slave, TIOCSCTTY, 0) == 0 || errno != EPERM)
        return 1;

    if (has_ctty())
        return 2;

    if (setsid() < 0)
        return 3;

    if (ioctl(slave, TIOCSCTTY, 0) < 0 || !has_ctty())
        return 4;

    if (tcgetpgrp(slave) != getpgrp())
        return 5;

    if (ioctl(slave, TIOCSCTTY, 0) < 0)
        return 6;


    int m2, s2;

    if (openpty(&m2, &s2, NULL, NULL, NULL) < 0)
        return 7;

    errno = 0;

    if (ioctl(s2, TIOCSCTTY, 0) == 0 || errno != EPERM)
        return 8;

    errno = 0;

    if (ioctl(s2, TIOCNOTTY, 0) == 0 || errno != ENOTTY)
        return 9;

    close(s2);
    close(m2);


    if (run(member_notty) != 0)
        return 10;

    if (!has_ctty())
        return 11;

    if (run(member_setsid) != 0)
        return 12;


    signal(SIGHUP, SIG_IGN);

    int fd = open("/dev/tty", O_RDWR | O_NOCTTY);

    if (fd < 0 || ioctl(fd, TIOCNOTTY, 0) < 0 || has_ctty())
        return 13;

    close(fd);

    if (run(member_reclaim) != 0)
        return 14;

    return 0;
}


/**
 * @brief The shell side of an aplus-terminal session: its own process group, made the foreground one, then waiting for a key to act on it.
 *
 * @return Only returns, with 50 plus the step, when the setup fails or no signal arrives.
 */
static int terminal_shell(void) {

    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);

    pid_t pgrp = getpid();

    if (setpgid(0, 0) < 0 || ioctl(slave, TIOCSPGRP, &pgrp) < 0)
        return 51;

    if (!has_ctty())
        return 52;

    write(slave, "R", 1);

    for (int i = 0; i < 10; i++)
        sleep(1);

    return 53;
}

/**
 * @brief The terminal side: a session that claims the pty through its master, as aplus-terminal does, then watches what Ctrl-C and Ctrl-Z do to its shell.
 *
 * @return 0, or the number of the step that failed.
 */
static int terminal_session(void) {

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    if (setsid() < 0 || ioctl(master, TIOCSCTTY, 0) < 0)
        return 1;


    int status = 0;

    pid_t shell = fork();

    if (shell == 0)
        _exit(terminal_shell());

    waitpid(shell, &status, 0);

    if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGINT)
        return WIFEXITED(status) ? WEXITSTATUS(status) : 2;


    shell = fork();

    if (shell == 0)
        _exit(terminal_shell());

    waitpid(shell, &status, WUNTRACED);

    if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTSTP)
        return WIFEXITED(status) ? WEXITSTATUS(status) : 3;

    kill(shell, SIGKILL);
    waitpid(shell, &status, 0);

    return 0;
}

/**
 * @brief Waits for the shell to report it is in the foreground, then types a key on the master.
 *
 * @param key The byte to type.
 * @return true if the shell reported in.
 */
static bool type_when_ready(char key) {

    for (;;) {

        struct pollfd p = {.fd = master, .events = POLLIN};

        if (poll(&p, 1, 5000) <= 0)
            return false;

        char c = 0;

        if (read(master, &c, 1) == 1 && c == 'R')
            break;
    }

    return write(master, &key, 1) == 1;
}


int main(void) {

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("ctty-test: starting\n");

    CHECK(!has_ctty(), "starts without a controlling terminal", "%s", "/dev/tty opened");

    CHECK(openpty(&master, &slave, NULL, NULL, NULL) == 0, "openpty", "%s", strerror(errno));

    int e = run(session_leader);
    CHECK(e == 0, "session leader rules", "failed at step %d", e);


    close(master);
    close(slave);

    CHECK(openpty(&master, &slave, NULL, NULL, NULL) == 0, "openpty for the terminal session", "%s", strerror(errno));

    pid_t pid = fork();

    if (pid == 0)
        _exit(terminal_session());

    bool typed_intr = type_when_ready(0x03);
    bool typed_susp = type_when_ready(0x1A);

    int status = 0;
    waitpid(pid, &status, 0);

    e = WIFEXITED(status) ? WEXITSTATUS(status) : 100 + WTERMSIG(status);

    CHECK(typed_intr && typed_susp && e == 0, "Ctrl-C and Ctrl-Z reach the foreground group", "typed %d/%d, failed at step %d", typed_intr, typed_susp, e);


    printf("ctty-test: %d/%d passed, %d failed\n", total - failures, total, failures);

    return failures ? 1 : 0;
}
