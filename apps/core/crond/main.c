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

#include <stdatomic.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include <aplus/ulog.h>


struct cron_job {
    uint64_t minute;
    uint64_t hour;
    uint64_t mday;
    uint64_t month;
    uint64_t wday;
    bool mday_any;
    bool wday_any;
    char* command;
    struct cron_job* next;
};

struct crontab {
    struct cron_job* jobs;
    char** env;
    size_t envc;
};

static atomic_bool crond_reload = false;
static atomic_bool crond_reap_children = false;


void show_usage(void) {
    printf("Use: crond [OPTION]...\n"
           "Run scheduled commands read from the crontab.\n\n"
           "Options:\n"
           "  -f, --foreground    run in the foreground instead of daemonizing\n"
           "  -c, --crontab FILE  read the crontab from FILE (default: /etc/crontab)\n"
           "      --help          display this help and exit\n"
           "      --version       output version information and exit\n");
}

void show_version(void) {
    printf("crond (aplus coreutils) 0.1\n"
           "Copyright (c) %s Antonino Natale.\n"
           "Built with gcc %s (%s)\n",
           &__DATE__[7], __VERSION__, __TIMESTAMP__);
}

static void crond_signal_handler(int sig) {
    switch (sig) {
        case SIGHUP:
            atomic_store(&crond_reload, true);
            break;
        case SIGCHLD:
            atomic_store(&crond_reap_children, true);
            break;
    }
}

static bool parse_field(char* field, int min, int max, uint64_t* mask) {
    *mask = 0;

    char* saveptr = NULL;
    for (char* item = strtok_r(field, ",", &saveptr); item; item = strtok_r(NULL, ",", &saveptr)) {
        long lo, hi, step = 1;
        bool ranged = false;
        char* end;

        if (*item == '*') {
            lo = min;
            hi = max;
            end = item + 1;
            ranged = true;
        } else {
            lo = strtol(item, &end, 10);
            if (end == item)
                return false;

            hi = lo;
            if (*end == '-') {
                char* start = end + 1;
                hi = strtol(start, &end, 10);
                if (end == start)
                    return false;
                ranged = true;
            }
        }

        if (*end == '/') {
            char* start = end + 1;
            step = strtol(start, &end, 10);
            if (end == start || step <= 0)
                return false;
            if (!ranged)
                hi = max;
        }

        if (*end != '\0' || lo < min || hi > max || lo > hi)
            return false;

        for (long v = lo; v <= hi; v += step)
            *mask |= UINT64_C(1) << v;
    }

    return *mask != 0;
}

static bool crontab_setenv(struct crontab* tab, const char* name, const char* value) {
    size_t len = strlen(name);

    char* entry = malloc(len + strlen(value) + 2);
    if (!entry)
        return false;

    sprintf(entry, "%s=%s", name, value);

    for (size_t i = 0; i < tab->envc; i++) {
        if (strncmp(tab->env[i], entry, len + 1) == 0) {
            free(tab->env[i]);
            tab->env[i] = entry;
            return true;
        }
    }

    char** env = realloc(tab->env, (tab->envc + 2) * sizeof(*env));
    if (!env) {
        free(entry);
        return false;
    }

    env[tab->envc++] = entry;
    env[tab->envc] = NULL;
    tab->env = env;
    return true;
}

static bool parse_env(char* line, struct crontab* tab) {
    char* eq = line + strcspn(line, "= \t");
    if (*eq != '=' || eq == line)
        return false;

    *eq = '\0';
    if (!crontab_setenv(tab, line, eq + 1))
        ulog_errorf("%s: %s", line, strerror(ENOMEM));

    return true;
}

static bool parse_job(char* line, struct cron_job* job) {
    static const int limits[5][2] = {
        {0, 59},
        {0, 23},
        {1, 31},
        {1, 12},
        {0, 7 },
    };

    uint64_t* masks[5] = {&job->minute, &job->hour, &job->mday, &job->month, &job->wday};

    char* p = line;
    for (int i = 0; i < 5; i++) {
        while (isspace((unsigned char)*p))
            p++;

        char* field = p;
        while (*p && !isspace((unsigned char)*p))
            p++;

        if (p == field || *p == '\0')
            return false;

        *p++ = '\0';

        if (i == 2)
            job->mday_any = field[0] == '*';
        if (i == 4)
            job->wday_any = field[0] == '*';

        if (!parse_field(field, limits[i][0], limits[i][1], masks[i]))
            return false;
    }

    job->wday = (job->wday | (job->wday >> 7)) & 0x7F;

    while (isspace((unsigned char)*p))
        p++;

    if (*p == '\0')
        return false;

    job->command = strdup(p);
    return job->command != NULL;
}

static void crontab_free(struct crontab* tab) {
    while (tab->jobs) {
        struct cron_job* next = tab->jobs->next;
        free(tab->jobs->command);
        free(tab->jobs);
        tab->jobs = next;
    }

    for (size_t i = 0; i < tab->envc; i++)
        free(tab->env[i]);

    free(tab->env);
    *tab = (struct crontab){0};
}

static bool crontab_load(const char* path, struct crontab* tab) {
    *tab = (struct crontab){0};

    FILE* fp = fopen(path, "r");
    if (!fp) {
        ulog_errorf("%s: %s", path, strerror(errno));
        return false;
    }

    crontab_setenv(tab, "SHELL", "/bin/sh");
    crontab_setenv(tab, "PATH", "/usr/sbin:/usr/bin:/sbin:/bin");
    crontab_setenv(tab, "HOME", "/");

    struct cron_job** tail = &tab->jobs;

    char* line = NULL;
    size_t size = 0;
    int lineno = 0;

    while (getline(&line, &size, fp) != -1) {
        lineno++;
        line[strcspn(line, "\r\n")] = '\0';

        char* p = line;
        while (isspace((unsigned char)*p))
            p++;

        if (*p == '\0' || *p == '#')
            continue;

        if (parse_env(p, tab))
            continue;

        struct cron_job* job = calloc(1, sizeof(*job));
        if (!job)
            break;

        if (!parse_job(p, job)) {
            ulog_warnf("%s:%d: invalid entry, skipped", path, lineno);
            free(job);
            continue;
        }

        *tail = job;
        tail = &job->next;
    }

    free(line);
    fclose(fp);

    if (!tab->jobs)
        ulog_errorf("%s: no jobs found", path);

    return true;
}

static bool crontab_match(const struct cron_job* job, const struct tm* tm) {
    if (!(job->minute & (UINT64_C(1) << tm->tm_min)))
        return false;

    if (!(job->hour & (UINT64_C(1) << tm->tm_hour)))
        return false;

    if (!(job->month & (UINT64_C(1) << (tm->tm_mon + 1))))
        return false;

    bool mday = job->mday & (UINT64_C(1) << tm->tm_mday);
    bool wday = job->wday & (UINT64_C(1) << tm->tm_wday);

    if (job->mday_any || job->wday_any)
        return mday && wday;

    return mday || wday;
}

static pid_t crontab_run(const struct crontab* tab, const struct cron_job* job) {
    pid_t pid = fork();

    if (pid < 0) {
        ulog_errorf("%s: fork(): %s", job->command, strerror(errno));
        return -1;
    }

    if (pid == 0) {
        signal(SIGHUP, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        sigset_t empty;
        sigemptyset(&empty);
        sigprocmask(SIG_SETMASK, &empty, NULL);

        if (execle("/bin/sh", "sh", "-c", job->command, (char*)NULL, tab->env) < 0) {
            ulog_errorf("%s: execle(/bin/sh): %s", job->command, strerror(errno));
        }
        _exit(127);
    } else {
        ulog_infof("running %s (pid %d)", job->command, (int)pid);
        return pid;
    }

}

static bool crond_daemonize(void) {
    pid_t pid = fork();

    if (pid < 0) {
        ulog_errorf("fork(): %s", strerror(errno));
        return false;
    }

    if (pid > 0)
        _exit(0);

    if (setsid() < 0) {
        ulog_errorf("setsid(): %s", strerror(errno));
        return false;
    }

    if (chdir("/") < 0)
        ulog_warnf("chdir(/): %s", strerror(errno));

    umask(022);

    int fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        ulog_warnf("/dev/null: %s", strerror(errno));
        return true;
    }

    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);

    if (fd > STDERR_FILENO)
        close(fd);

    return true;
}

int main(int argc, char** argv) {
    static const struct option long_options[] = {
        {"foreground", no_argument, NULL, 'f'},
        {"crontab",    required_argument, NULL, 'c'},
        {"help",       no_argument, NULL, 'h'},
        {"version",    no_argument, NULL, 'v'},
        {NULL,         0,           NULL, 0  },
    };

    bool foreground = false;
    char* crontab_path = "/etc/crontab";

    int opt;
    while ((opt = getopt_long(argc, argv, "fc:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'f':
                foreground = true;
                break;
            case 'c':
                crontab_path = optarg;
                break;
            case 'h':
                show_usage();
                return 0;
            case 'v':
                show_version();
                return 0;
            default:
                fprintf(stderr, "Try 'crond --help' for more information.\n");
                return 1;
        }
    }

    struct crontab tab = {0};
    if (!crontab_load(crontab_path, &tab) || !tab.jobs) {
        crontab_free(&tab);
        return 1;
    }

    if (!foreground && !crond_daemonize())
        return 1;

    ulog_infof("started with %s", crontab_path);

    sigset_t empty;
    sigemptyset(&empty);
    sigprocmask(SIG_SETMASK, &empty, NULL);

    struct sigaction sa = {0};
    sa.sa_handler = crond_signal_handler;
    sigemptyset(&sa.sa_mask);

    sa.sa_flags = 0;
    sigaction(SIGHUP, &sa, NULL);
    sa.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);


    time_t last_minute = -1;

    do {

        if (atomic_exchange(&crond_reload, false)) {
            struct crontab loaded;
            if (crontab_load(crontab_path, &loaded)) {
                if (!loaded.jobs) {
                    crontab_free(&loaded);
                    return 1;
                }
                crontab_free(&tab);
                tab = loaded;
            } else {
                ulog_warnf("%s: reload failed, keeping the previous jobs", crontab_path);
            }
        }

        if (atomic_exchange(&crond_reap_children, false)) {
            while (waitpid(-1, NULL, WNOHANG) > 0)
                ;
        }


        struct tm tm;
        time_t now = time(NULL);
        if (!localtime_r(&now, &tm)) {
            ulog_errorf("localtime_r(): %s", strerror(errno));
            return 1;
        }

        if (now / 60 != last_minute) {
            last_minute = now / 60;

            for (struct cron_job* job = tab.jobs; job; job = job->next) {
                if (crontab_match(job, &tm)) {
                    crontab_run(&tab, job);
                }
            }
        }

        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) < 0) {
            ulog_errorf("clock_gettime(): %s", strerror(errno));
            return 1;
        }

        sleep(60 - (ts.tv_sec % 60));

    } while (true);

    return 0;
}
