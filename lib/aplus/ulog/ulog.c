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

#include <aplus/ulog.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>

extern char* program_invocation_short_name;

static FILE* log_output = NULL;
static char* log_appname = NULL;

#if defined(DEBUG)
static ulog_level_t log_level = ULOG_DEBUG;
#else
static ulog_level_t log_level = ULOG_INFO;
#endif


static void ulog_atexit(void) {
    if (log_output != NULL) {
        fclose(log_output);
        log_output = NULL;
    }
}


/**
 * @brief Opens the log file and sets the most verbose level that gets written.
 * @param path File to append log lines to.
 * @param level Most verbose level to keep, capped at ULOG_DEBUG.
 * @return 0 on success, -1 if the file cannot be opened.
 */
int ulog_init(const char* path, const ulog_level_t level) {
    log_level = level > ULOG_DEBUG ? ULOG_DEBUG : level;

    if (log_output) {
        log_output = freopen(path, "ae", log_output);
    } else {
        log_output = fopen(path, "ae");
        atexit(ulog_atexit);
    }

    if (!log_output) {
        return -1;
    }

    setvbuf(log_output, NULL, _IOLBF, 0);
    return 0;
}


/**
 * @brief Writes one timestamped log line tagged with the program name and level.
 * @param level Severity of the message.
 * @param fmt printf-style format of the message.
 * @return Bytes written, 0 if the level is filtered out, -1 on a write error.
 */
__attribute__((format(printf, 2, 3)))
int ulogf(const ulog_level_t level, const char* fmt, ...) {
    if (level > log_level) {
        return 0;
    }

    int saved_errno = errno;

    FILE* output = log_output;
    if (!output) {
        output = stderr;
    }

    char* appname = log_appname;
    if (!appname) {
        appname = program_invocation_short_name;
    }

    static const char* const names[] = {
        [ULOG_ERROR]   = "ERROR",
        [ULOG_WARNING] = "WARNING",
        [ULOG_INFO]    = "INFO",
        [ULOG_DEBUG]   = "DEBUG",
    };

    time_t now = time(NULL);
    struct tm tm;
    if (!localtime_r(&now, &tm) && !gmtime_r(&now, &tm)) {
        tm = (struct tm){0};
    }

    char when[sizeof("YYYY-MM-DDTHH:MM:SS+hhmm")];
    if (strftime(when, sizeof(when), "%Y-%m-%dT%H:%M:%S%z", &tm) == 0) {
        when[0] = '\0';
    }

    flockfile(output);

    int count = -1;
    int prefix = fprintf(output, "%s %s[%d]: %s: ", when, appname, (int)getpid(), names[level]);

    if (prefix >= 0) {
        va_list ap;
        va_start(ap, fmt);
        int message = vfprintf(output, fmt, ap);
        va_end(ap);

        if (message >= 0 && fputc('\n', output) != EOF) {
            count = prefix + message + 1;
        }
    }

    funlockfile(output);

    errno = saved_errno;
    return count;
}
