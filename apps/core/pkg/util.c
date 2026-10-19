/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2026 Antonino Natale
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

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "pkg.h"


pkg_options_t pkg_options;


/**
 * @brief Prints an error message on stderr.
 * @param fmt The printf format of the message.
 */
void pkg_error(const char* fmt, ...) {

    va_list ap;
    va_start(ap, fmt);

    fflush(stdout);
    fprintf(stderr, "%s: error: ", PKG_NAME);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);

    va_end(ap);
}


/**
 * @brief Prints a warning message on stderr.
 * @param fmt The printf format of the message.
 */
void pkg_warn(const char* fmt, ...) {

    va_list ap;
    va_start(ap, fmt);

    fflush(stdout);
    fprintf(stderr, "%s: warning: ", PKG_NAME);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);

    va_end(ap);
}


/**
 * @brief Duplicates a string, exiting when memory runs out.
 * @param s The string.
 * @return The copy.
 */
char* pkg_strdup(const char* s) {

    char* copy = strdup(s);

    if (!copy) {
        pkg_error("out of memory");
        exit(1);
    }

    return copy;
}


/**
 * @brief Formats a string into a new allocation, exiting when memory runs out.
 * @param fmt The printf format.
 * @return The formatted string.
 */
char* pkg_format(const char* fmt, ...) {

    char* s = NULL;

    va_list ap;
    va_start(ap, fmt);

    if (vasprintf(&s, fmt, ap) < 0) {
        pkg_error("out of memory");
        exit(1);
    }

    va_end(ap);

    return s;
}


/**
 * @brief Asks a yes/no question on the terminal; without a terminal, or with --yes, the answer is yes.
 * @param question The question.
 * @return True when the user agreed.
 */
bool pkg_confirm(const char* question) {

    if (pkg_options.yes || !isatty(STDIN_FILENO))
        return true;

    printf("%s [Y/n] ", question);
    fflush(stdout);

    char answer[32];

    if (!fgets(answer, sizeof(answer), stdin))
        return false;

    return answer[0] == '\n' || answer[0] == 'y' || answer[0] == 'Y';
}


/**
 * @brief Creates a directory and every missing parent.
 * @param path The directory.
 * @param mode The mode of the directory itself; parents get 0755.
 * @return 0 on success, -1 on error.
 */
int pkg_mkdirs(const char* path, mode_t mode) {

    char* dir = pkg_strdup(path);

    for (char* p = dir + 1; *p; p++) {

        if (*p != '/')
            continue;

        *p = '\0';

        if (mkdir(dir, 0755) < 0 && errno != EEXIST) {
            pkg_error("%s: %s", dir, strerror(errno));
            free(dir);
            return -1;
        }

        *p = '/';
    }

    if (mkdir(dir, mode) < 0 && errno != EEXIST) {
        pkg_error("%s: %s", dir, strerror(errno));
        free(dir);
        return -1;
    }

    free(dir);
    return 0;
}


/**
 * @brief Copies a regular file over a new one.
 * @param src The source file.
 * @param dst The destination, which must not exist.
 * @param mode The mode of the copy.
 * @return 0 on success, otherwise the errno of the failure.
 */
int pkg_copy_file(const char* src, const char* dst, mode_t mode) {

    int in = open(src, O_RDONLY | O_CLOEXEC);

    if (in < 0)
        return errno;

    int out = open(dst, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);

    if (out < 0) {
        int e = errno;
        close(in);
        return e;
    }

    char buf[16384];
    ssize_t n;
    int e = 0;

    while ((n = read(in, buf, sizeof(buf))) > 0) {

        if (write(out, buf, n) != n) {
            e = errno ? errno : EIO;
            break;
        }
    }

    if (n < 0)
        e = errno;

    if (!e && fchmod(out, mode & 07777) < 0)
        e = errno;

    if (close(out) < 0 && !e)
        e = errno;

    close(in);

    if (e)
        unlink(dst);

    return e;
}


/**
 * @brief Executes a script through the interpreter named by its #! line, or /bin/sh without one.
 * @param path The script.
 */
static void pkg_exec_interpreter(const char* path) {

    char line[256] = "";
    FILE* fp       = fopen(path, "r");

    if (fp) {

        if (!fgets(line, sizeof(line), fp))
            line[0] = '\0';

        fclose(fp);
    }

    if (strncmp(line, "#!", 2) != 0) {
        execl("/bin/sh", "sh", path, "/", (char*)NULL);
        return;
    }

    char* save   = NULL;
    char* interp = strtok_r(&line[2], " \t\r\n", &save);
    char* arg    = strtok_r(NULL, " \t\r\n", &save);

    if (!interp)
        return;

    if (arg)
        execl(interp, interp, arg, path, "/", (char*)NULL);
    else
        execl(interp, interp, path, "/", (char*)NULL);
}


/**
 * @brief Runs a package script with the installation prefix as its only argument.
 * @param path The script, which must be executable.
 * @return 0 when it exits with status 0, -1 otherwise.
 */
int pkg_run_script(const char* path) {

    fflush(NULL);

    pid_t pid = fork();

    if (pid < 0) {
        pkg_error("fork: %s", strerror(errno));
        return -1;
    }

    if (pid == 0) {

        execl(path, path, "/", (char*)NULL);

        if (errno == ENOEXEC)
            pkg_exec_interpreter(path);

        fprintf(stderr, "%s: error: %s: %s\n", PKG_NAME, path, strerror(errno));
        _exit(127);
    }

    int status;

    while (waitpid(pid, &status, 0) < 0) {

        if (errno != EINTR) {
            pkg_error("waitpid: %s", strerror(errno));
            return -1;
        }
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return -1;

    return 0;
}
