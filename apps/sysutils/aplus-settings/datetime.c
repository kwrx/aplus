/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2026 Antonino Natale
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <aplus/ui-anim.h>

#include "settings.h"


#define DATETIME_ZONEINFO      "/usr/share/zoneinfo"
#define DATETIME_ZONE_TAB       DATETIME_ZONEINFO "/zone.tab"
#define DATETIME_COUNTRY_TAB    DATETIME_ZONEINFO "/iso3166.tab"
#define DATETIME_LOCALTIME      "/etc/localtime"
#define DATETIME_LOCALTIME_NEW  "/etc/localtime.new"
#define DATETIME_LINK_PREFIX    "../usr/share/zoneinfo/"
#define DATETIME_NTP_CONF       "/etc/ntp.conf"
#define DATETIME_NTP_CONF_MAX   4096
#define DATETIME_SERVICE        "/etc/init.d/02-ntpd.sh"
#define DATETIME_SERVICE_PID    "/var/run/init.d/02-ntpd.sh.pid"
#define DATETIME_NTPD           "/usr/sbin/ntpd"
#define DATETIME_SHELL          "/bin/sh"
#define DATETIME_SERVER_DEFAULT "pool.ntp.org"
#define DATETIME_SERVER_MAX     64
#define DATETIME_ZONES_MAX      512
#define DATETIME_COUNTRIES_MAX  320
#define DATETIME_NAME_MAX       48
#define DATETIME_PATH_MAX       256
#define DATETIME_POLL_MS        100
#define DATETIME_SYNC_TIMEOUT   20000
#define DATETIME_OUTPUT_MAX     4096
#define DATETIME_UTC_LABEL      "Coordinated Universal Time"
#define DATETIME_DASH           "\xE2\x80\x94"
#define DATETIME_ELLIPSIS       "\xE2\x80\xA6"


typedef enum {

    DATETIME_NOW = 0,
    DATETIME_ZONE,
    DATETIME_AUTO,
    DATETIME_SERVER,
    DATETIME_MANUAL,

    DATETIME_ROWS,

} datetime_row_t;


static const char* const datetime_keys[DATETIME_ROWS] = {
    "Date and time", "Time zone", "Set automatically", "Time server", "Set manually",
};


typedef struct {

    char name[DATETIME_NAME_MAX];
    char country[DATETIME_NAME_MAX];

} datetime_zone_t;


typedef void (*datetime_done_fn)(int status, const char* output);


static struct {

    settings_header_t header;
    settings_card_t card;

    ui_widget_t* off;
    ui_widget_t* on;
    ui_widget_t* server;
    ui_widget_t* sync;
    ui_widget_t* date;
    ui_widget_t* time;
    ui_widget_t* set;
    ui_widget_t* status;

    ui_widget_t* zone_heading;
    ui_widget_t* search;
    ui_widget_t* list;

    datetime_zone_t zones[DATETIME_ZONES_MAX];
    size_t zone_count;

    char current[DATETIME_PATH_MAX];
    char saved_server[DATETIME_SERVER_MAX];

    bool loaded;
    bool filling;
    bool automatic;
    bool restart;

    struct {

        pid_t pid;
        int fd;
        uint64_t deadline;

        char output[DATETIME_OUTPUT_MAX];
        size_t length;

        datetime_done_fn done;

    } job;

} datetime;


static void datetime_report(const char* text) {
    settings_fit(datetime.status, text);
}


static void datetime_reload_zone(void) {

    if (getenv("TZ")) {
        return;
    }

    setenv("TZ", ":" DATETIME_LOCALTIME, 1);
    tzset();
    unsetenv("TZ");
    tzset();
}


static void datetime_read_zone(void) {

    char link[DATETIME_PATH_MAX];

    const ssize_t n = readlink(DATETIME_LOCALTIME, link, sizeof(link) - 1);

    datetime.current[0] = '\0';

    if (n <= 0) {
        return;
    }

    link[n] = '\0';

    const char* zone = strstr(link, "zoneinfo/");

    if (zone) {
        snprintf(datetime.current, sizeof(datetime.current), "%s", zone + strlen("zoneinfo/"));
    }
}


static void datetime_show_zone(void) {

    char text[SETTINGS_TEXT_MAX];
    char abbrev[16] = "";
    long offset     = 0;

    const time_t now = time(NULL);

    struct tm tm;

    if (localtime_r(&now, &tm)) {
        strftime(abbrev, sizeof(abbrev), "%Z", &tm);
        offset = tm.tm_gmtoff;
    }

    const long minutes = (offset < 0 ? -offset : offset) / 60;

    const char* zone = datetime.current[0] ? datetime.current : "Custom";

    if (!abbrev[0] || strcmp(abbrev, zone) == 0) {
        snprintf(text, sizeof(text), "%.64s \xC2\xB7 UTC%c%02ld:%02ld", zone, offset < 0 ? '-' : '+', minutes / 60, minutes % 60);
    } else {
        snprintf(text, sizeof(text), "%.64s \xC2\xB7 %s, UTC%c%02ld:%02ld", zone, abbrev, offset < 0 ? '-' : '+', minutes / 60, minutes % 60);
    }

    settings_row_set(&datetime.card.rows[DATETIME_ZONE], text);
}


static void datetime_show_now(void) {

    char text[SETTINGS_TEXT_MAX];

    const time_t now = time(NULL);

    struct tm tm;

    if (!localtime_r(&now, &tm) || !strftime(text, sizeof(text), "%A %d %B %Y, %H:%M:%S", &tm)) {
        snprintf(text, sizeof(text), SETTINGS_UNKNOWN);
    }

    settings_row_set(&datetime.card.rows[DATETIME_NOW], text);
}


static void datetime_fill_manual(void) {

    char date[16];
    char clock[16];

    const time_t now = time(NULL);

    struct tm tm;

    if (!localtime_r(&now, &tm)) {
        return;
    }

    strftime(date, sizeof(date), "%Y-%m-%d", &tm);
    strftime(clock, sizeof(clock), "%H:%M:%S", &tm);

    ui_entry_set_text(datetime.date, date);
    ui_entry_set_text(datetime.time, clock);
}


static bool datetime_job_running(void) {
    return datetime.job.pid != 0;
}


static void datetime_update_controls(void) {

    const bool idle = !datetime_job_running();

    ui_button_set_style(datetime.off, datetime.automatic ? UI_BUTTON_STYLE_DEFAULT : UI_BUTTON_STYLE_PRIMARY);
    ui_button_set_style(datetime.on, datetime.automatic ? UI_BUTTON_STYLE_PRIMARY : UI_BUTTON_STYLE_DEFAULT);

    ui_widget_set_enabled(datetime.off, idle);
    ui_widget_set_enabled(datetime.on, idle);
    ui_widget_set_enabled(datetime.server, idle);
    ui_widget_set_enabled(datetime.sync, idle);

    ui_widget_set_enabled(datetime.date, idle && !datetime.automatic);
    ui_widget_set_enabled(datetime.time, idle && !datetime.automatic);
    ui_widget_set_enabled(datetime.set, idle && !datetime.automatic);
}


static bool datetime_service_enabled(void) {
    return access(DATETIME_SERVICE, X_OK) == 0;
}


static void datetime_read_server(void) {

    snprintf(datetime.saved_server, sizeof(datetime.saved_server), "%s", DATETIME_SERVER_DEFAULT);

    FILE* fp = fopen(DATETIME_NTP_CONF, "r");

    if (fp) {

        char line[256];

        while (fgets(line, sizeof(line), fp)) {

            char key[16];
            char value[DATETIME_SERVER_MAX];

            if (sscanf(line, " %15s %63s", key, value) == 2 && strcmp(key, "server") == 0) {
                snprintf(datetime.saved_server, sizeof(datetime.saved_server), "%s", value);
                break;
            }
        }

        fclose(fp);
    }

    ui_entry_set_text(datetime.server, datetime.saved_server);
}


static bool datetime_valid_server(const char* server) {

    if (!server[0] || server[0] == '-' || strlen(server) >= DATETIME_SERVER_MAX) {
        return false;
    }

    for (const char* p = server; *p; p++) {

        if (!isalnum((unsigned char)*p) && !strchr(".:_-", *p)) {
            return false;
        }
    }

    return true;
}


static int datetime_write_server(const char* server) {

    static char contents[DATETIME_NTP_CONF_MAX];

    size_t length = 0;
    bool written  = false;

    FILE* fp = fopen(DATETIME_NTP_CONF, "r");

    if (fp) {

        char line[256];

        while (fgets(line, sizeof(line), fp)) {

            char key[16];

            if (sscanf(line, " %15s", key) == 1 && strcmp(key, "server") == 0) {

                if (written) {
                    continue;
                }

                length  += (size_t)snprintf(contents + length, sizeof(contents) - length, "server %s\n", server);
                written  = true;

            } else {
                length += (size_t)snprintf(contents + length, sizeof(contents) - length, "%s", line);
            }

            if (length >= sizeof(contents)) {
                fclose(fp);
                errno = EFBIG;
                return -1;
            }
        }

        fclose(fp);
    }

    if (!written) {
        length += (size_t)snprintf(contents + length, sizeof(contents) - length, "server %s\n", server);
    }

    if (length >= sizeof(contents)) {
        errno = EFBIG;
        return -1;
    }

    if (settings_write_file(DATETIME_NTP_CONF, contents) < 0) {
        return -1;
    }

    snprintf(datetime.saved_server, sizeof(datetime.saved_server), "%s", server);

    return 0;
}


static int datetime_spawn(const char* path, char* const argv[], datetime_done_fn done, int timeout) {

    int fds[2];

    if (datetime_job_running() || pipe(fds) < 0) {
        return -1;
    }

    const pid_t pid = fork();

    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }

    if (pid == 0) {

        const int null = open("/dev/null", O_RDONLY);

        if (null >= 0) {
            dup2(null, STDIN_FILENO);
        }

        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);

        for (int fd = 3; fd < 256; fd++) {
            close(fd);
        }

        execv(path, argv);
        _exit(127);
    }

    close(fds[1]);

    fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | O_NONBLOCK);

    datetime.job.pid      = pid;
    datetime.job.fd       = fds[0];
    datetime.job.length   = 0;
    datetime.job.done     = done;
    datetime.job.deadline = timeout > 0 ? ui_anim_now() + (uint64_t)timeout : 0;

    datetime.job.output[0] = '\0';

    datetime_update_controls();

    return 0;
}


static void datetime_drain(void) {

    for (;;) {

        char chunk[512];

        const ssize_t n = read(datetime.job.fd, chunk, sizeof(chunk));

        if (n <= 0) {
            break;
        }

        for (ssize_t i = 0; i < n; i++) {

            if (datetime.job.length + 1 >= sizeof(datetime.job.output)) {
                memmove(datetime.job.output, datetime.job.output + sizeof(datetime.job.output) / 2, sizeof(datetime.job.output) / 2);
                datetime.job.length -= sizeof(datetime.job.output) / 2;
            }

            datetime.job.output[datetime.job.length++] = chunk[i];
        }

        datetime.job.output[datetime.job.length] = '\0';
    }
}


static void datetime_last_line(const char* output, char* out, size_t size) {

    size_t end = strlen(output);

    while (end > 0 && isspace((unsigned char)output[end - 1])) {
        end--;
    }

    size_t start = end;

    while (start > 0 && output[start - 1] != '\n') {
        start--;
    }

    snprintf(out, size, "%.*s", (int)(end - start), output + start);
}


static void datetime_service(const char* action, datetime_done_fn done) {

    char* argv[] = {DATETIME_SHELL, DATETIME_SERVICE, (char*)action, NULL};

    if (datetime_spawn(DATETIME_SHELL, argv, done, 0) < 0) {

        char text[SETTINGS_TEXT_MAX];

        snprintf(text, sizeof(text), "Cannot run the NTP service: %s", strerror(errno));
        datetime_report(text);
    }
}


static void datetime_on_service(int status, const char* output) {

    char text[SETTINGS_TEXT_MAX];

    if (status != 0) {

        char line[SETTINGS_TEXT_MAX];

        datetime_last_line(output, line, sizeof(line));
        snprintf(text, sizeof(text), "The NTP service failed: %.96s", line[0] ? line : "unknown error");
        datetime_report(text);
        return;
    }

    if (datetime.automatic) {
        snprintf(text, sizeof(text), "The clock follows %s.", datetime.saved_server);
        datetime_report(text);
    } else {
        datetime_report("The clock is no longer synchronised.");
        datetime_fill_manual();
    }
}


static void datetime_on_sync(int status, const char* output) {

    char text[SETTINGS_TEXT_MAX];

    if (status == 0) {

        snprintf(text, sizeof(text), "Synchronised with %s.", datetime.saved_server);
        datetime_report(text);

        datetime_show_now();
        datetime_fill_manual();

    } else if (status < 0) {

        snprintf(text, sizeof(text), "No answer from %s.", datetime.saved_server);
        datetime_report(text);

    } else {

        char line[SETTINGS_TEXT_MAX];

        datetime_last_line(output, line, sizeof(line));
        snprintf(text, sizeof(text), "Cannot synchronise: %.96s", line[0] ? line : "unknown error");
        datetime_report(text);
    }

    if (datetime.restart) {
        datetime.restart = false;
        datetime_service("restart", datetime_on_service);
    }
}


static bool datetime_take_server(void) {

    char server[DATETIME_SERVER_MAX];
    char text[SETTINGS_TEXT_MAX];

    snprintf(server, sizeof(server), "%s", ui_entry_text(datetime.server));

    char* start = server;

    while (isspace((unsigned char)*start)) {
        start++;
    }

    for (size_t n = strlen(start); n > 0 && isspace((unsigned char)start[n - 1]); n--) {
        start[n - 1] = '\0';
    }

    if (!datetime_valid_server(start)) {
        snprintf(text, sizeof(text), "Not a server name: %s", start[0] ? start : "(empty)");
        datetime_report(text);
        return false;
    }

    if (strcmp(start, datetime.saved_server) == 0) {
        return true;
    }

    if (datetime_write_server(start) < 0) {
        snprintf(text, sizeof(text), "Cannot save the server: %s", strerror(errno));
        datetime_report(text);
        return false;
    }

    ui_entry_set_text(datetime.server, start);

    datetime.restart = datetime.automatic;

    return true;
}


static void datetime_on_sync_now(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    if (datetime_job_running() || !datetime_take_server()) {
        return;
    }

    char* argv[] = {DATETIME_NTPD, "-n", "-q", "-p", datetime.saved_server, NULL};

    if (datetime_spawn(DATETIME_NTPD, argv, datetime_on_sync, DATETIME_SYNC_TIMEOUT) < 0) {

        char text[SETTINGS_TEXT_MAX];

        snprintf(text, sizeof(text), "Cannot run ntpd: %s", strerror(errno));
        datetime_report(text);
        return;
    }

    char text[SETTINGS_TEXT_MAX];

    snprintf(text, sizeof(text), "Asking %s for the time" DATETIME_ELLIPSIS, datetime.saved_server);
    datetime_report(text);
}


static void datetime_on_automatic(ui_widget_t* widget, void* user) {

    (void)widget;

    const bool enable = (intptr_t)user != 0;

    if (datetime_job_running() || enable == datetime.automatic) {
        return;
    }

    if (enable && !datetime_take_server()) {
        return;
    }

    if (chmod(DATETIME_SERVICE, enable ? 0755 : 0644) < 0) {

        char text[SETTINGS_TEXT_MAX];

        snprintf(text, sizeof(text), "Cannot change the NTP service: %s", strerror(errno));
        datetime_report(text);
        return;
    }

    datetime.automatic = enable;
    datetime.restart   = false;

    datetime_report(enable ? "Starting the NTP service" DATETIME_ELLIPSIS : "Stopping the NTP service" DATETIME_ELLIPSIS);
    datetime_service(enable ? "restart" : "stop", datetime_on_service);

    datetime_update_controls();
}


static void datetime_on_set(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    if (datetime.automatic || datetime_job_running()) {
        return;
    }

    struct tm tm;

    memset(&tm, 0, sizeof(tm));

    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second = 0;
    char tail;

    if (sscanf(ui_entry_text(datetime.date), " %d-%d-%d %c", &year, &month, &day, &tail) != 3 || year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31) {
        datetime_report("Write the date as YYYY-MM-DD, between 2000 and 2099.");
        return;
    }

    const int fields = sscanf(ui_entry_text(datetime.time), " %d:%d:%d %c", &hour, &minute, &second, &tail);

    if ((fields != 2 && fields != 3) || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        datetime_report("Write the time as HH:MM or HH:MM:SS.");
        return;
    }

    tm.tm_year  = year - 1900;
    tm.tm_mon   = month - 1;
    tm.tm_mday  = day;
    tm.tm_hour  = hour;
    tm.tm_min   = minute;
    tm.tm_sec   = second;
    tm.tm_isdst = -1;

    const time_t when = mktime(&tm);

    if (when == (time_t)-1 || tm.tm_mday != day || tm.tm_mon != month - 1) {
        datetime_report("There is no such day.");
        return;
    }

    const struct timespec ts = {.tv_sec = when, .tv_nsec = 0};

    if (clock_settime(CLOCK_REALTIME, &ts) < 0) {

        char text[SETTINGS_TEXT_MAX];

        snprintf(text, sizeof(text), "Cannot set the clock: %s", strerror(errno));
        datetime_report(text);
        return;
    }

    datetime_show_now();
    datetime_fill_manual();
    datetime_report("Date and time set.");
}


static const char* datetime_country(char countries[][2][DATETIME_NAME_MAX], size_t count, const char* code) {

    for (size_t i = 0; i < count; i++) {

        if (strcmp(countries[i][0], code) == 0) {
            return countries[i][1];
        }
    }

    return code;
}


static int datetime_compare(const void* a, const void* b) {
    return strcmp(((const datetime_zone_t*)a)->name, ((const datetime_zone_t*)b)->name);
}


static void datetime_load_zones(void) {

    static char countries[DATETIME_COUNTRIES_MAX][2][DATETIME_NAME_MAX];

    size_t country_count = 0;

    char line[256];

    FILE* fp = fopen(DATETIME_COUNTRY_TAB, "r");

    if (fp) {

        while (country_count < DATETIME_COUNTRIES_MAX && fgets(line, sizeof(line), fp)) {

            if (line[0] == '#') {
                continue;
            }

            char* tab = strchr(line, '\t');

            if (!tab) {
                continue;
            }

            *tab++ = '\0';
            tab[strcspn(tab, "\r\n")] = '\0';

            snprintf(countries[country_count][0], DATETIME_NAME_MAX, "%.7s", line);
            snprintf(countries[country_count][1], DATETIME_NAME_MAX, "%.47s", tab);

            country_count++;
        }

        fclose(fp);
    }

    datetime.zone_count = 0;

    snprintf(datetime.zones[0].name, DATETIME_NAME_MAX, "UTC");
    snprintf(datetime.zones[0].country, DATETIME_NAME_MAX, DATETIME_UTC_LABEL);

    datetime.zone_count = 1;

    if ((fp = fopen(DATETIME_ZONE_TAB, "r")) == NULL) {
        return;
    }

    while (datetime.zone_count < DATETIME_ZONES_MAX && fgets(line, sizeof(line), fp)) {

        char code[8];
        char coords[32];
        char name[DATETIME_NAME_MAX];

        if (line[0] == '#' || sscanf(line, "%7s %31s %47s", code, coords, name) != 3) {
            continue;
        }

        datetime_zone_t* zone = &datetime.zones[datetime.zone_count++];

        snprintf(zone->name, sizeof(zone->name), "%s", name);
        snprintf(zone->country, sizeof(zone->country), "%s", datetime_country(countries, country_count, code));
    }

    fclose(fp);

    qsort(datetime.zones, datetime.zone_count, sizeof(datetime.zones[0]), datetime_compare);
}


static void datetime_pretty(const char* name, char* out, size_t size) {

    snprintf(out, size, "%s", name);

    for (char* p = out; *p; p++) {

        if (*p == '_') {
            *p = ' ';
        }
    }
}


static bool datetime_matches(const datetime_zone_t* zone, const char* query) {

    if (!query[0]) {
        return true;
    }

    char name[DATETIME_NAME_MAX];

    datetime_pretty(zone->name, name, sizeof(name));

    return strcasestr(name, query) || strcasestr(zone->name, query) || strcasestr(zone->country, query);
}


static void datetime_fill(void) {

    char query[DATETIME_NAME_MAX];

    snprintf(query, sizeof(query), "%s", ui_entry_text(datetime.search));

    datetime.filling = true;

    ui_list_clear(datetime.list);

    int selected = -1;

    for (size_t i = 0; i < datetime.zone_count; i++) {

        const datetime_zone_t* zone = &datetime.zones[i];

        if (!datetime_matches(zone, query)) {
            continue;
        }

        char name[DATETIME_NAME_MAX];

        datetime_pretty(zone->name, name, sizeof(name));

        const int index = ui_list_add(datetime.list, name, zone->country, (void*)(intptr_t)i);

        if (index >= 0 && strcmp(zone->name, datetime.current) == 0) {
            selected = index;
        }
    }

    ui_list_select(datetime.list, selected);

    if (selected >= 0) {
        ui_list_scroll_to(datetime.list, selected);
    }

    datetime.filling = false;
}


static int datetime_link_zone(const char* zone) {

    char path[DATETIME_PATH_MAX];
    char target[DATETIME_PATH_MAX];

    if (strstr(zone, "..") || (size_t)snprintf(path, sizeof(path), DATETIME_ZONEINFO "/%s", zone) >= sizeof(path)) {
        errno = EINVAL;
        return -1;
    }

    if (access(path, R_OK) < 0) {
        return -1;
    }

    snprintf(target, sizeof(target), DATETIME_LINK_PREFIX "%s", zone);

    unlink(DATETIME_LOCALTIME_NEW);

    if (symlink(target, DATETIME_LOCALTIME_NEW) < 0) {
        return -1;
    }

    if (rename(DATETIME_LOCALTIME_NEW, DATETIME_LOCALTIME) < 0) {

        const int e = errno;

        unlink(DATETIME_LOCALTIME_NEW);

        errno = e;
        return -1;
    }

    return 0;
}


static void datetime_on_zone(ui_widget_t* widget, int index, void* user) {

    (void)user;

    if (datetime.filling || index < 0) {
        return;
    }

    const size_t i = (size_t)(intptr_t)ui_list_item_user(widget, index);

    if (i >= datetime.zone_count || strcmp(datetime.zones[i].name, datetime.current) == 0) {
        return;
    }

    char text[SETTINGS_TEXT_MAX];

    if (datetime_link_zone(datetime.zones[i].name) < 0) {
        snprintf(text, sizeof(text), "Cannot change the time zone: %s", strerror(errno));
        datetime_report(text);
        return;
    }

    datetime_reload_zone();
    datetime_read_zone();

    datetime_show_zone();
    datetime_show_now();

    if (!datetime.automatic) {
        datetime_fill_manual();
    }

    snprintf(text, sizeof(text), "The time zone is now %s.", datetime.zones[i].name);
    datetime_report(text);
}


static void datetime_on_search(ui_widget_t* widget, void* user) {

    (void)widget;
    (void)user;

    datetime_fill();
}


static int datetime_build(void) {

    if (settings_header_build(&datetime.header, "Date & time", "The time zone, and where the clock takes the time from.") < 0) {
        return -1;
    }

    if (settings_card_build(&datetime.card, datetime_keys, DATETIME_ROWS, (1U << DATETIME_AUTO) | (1U << DATETIME_SERVER) | (1U << DATETIME_MANUAL)) < 0) {
        return -1;
    }

    datetime.off          = settings_own(ui_button_create(settings.view, "Off", datetime_on_automatic, (void*)(intptr_t)0));
    datetime.on           = settings_own(ui_button_create(settings.view, "On", datetime_on_automatic, (void*)(intptr_t)1));
    datetime.server       = settings_own(ui_entry_create(settings.view, DATETIME_SERVER_DEFAULT));
    datetime.sync         = settings_own(ui_button_create(settings.view, "Sync now", datetime_on_sync_now, NULL));
    datetime.date         = settings_own(ui_entry_create(settings.view, "YYYY-MM-DD"));
    datetime.time         = settings_own(ui_entry_create(settings.view, "HH:MM:SS"));
    datetime.set          = settings_own(ui_button_create(settings.view, "Set", datetime_on_set, NULL));
    datetime.status       = settings_own(ui_label_create(settings.view, ""));
    datetime.zone_heading = settings_heading("Time zone");
    datetime.search       = settings_own(ui_entry_create(settings.view, "Search cities or countries"));
    datetime.list         = settings_own(ui_list_create(settings.view));

    if (!datetime.off || !datetime.on || !datetime.server || !datetime.sync || !datetime.date || !datetime.time || !datetime.set || !datetime.status || !datetime.zone_heading || !datetime.search || !datetime.list) {
        return -1;
    }

    datetime.job.fd = -1;

    ui_entry_on_submit(datetime.server, datetime_on_sync_now, NULL);
    ui_entry_on_submit(datetime.date, datetime_on_set, NULL);
    ui_entry_on_submit(datetime.time, datetime_on_set, NULL);
    ui_entry_on_change(datetime.search, datetime_on_search, NULL);
    ui_list_on_select(datetime.list, datetime_on_zone, NULL);

    return 0;
}


static void datetime_layout(ui_rect_t page) {

    const int control = settings_control_height();
    const int pad     = SETTINGS_CARD_PADDING;

    int y = settings_header_place(&datetime.header, page, 0);

    y += settings_card_place(&datetime.card, page.x, y, page.width);


    const ui_rect_t automatic = settings_card_value(&datetime.card, DATETIME_AUTO);

    int toggle = settings_text_width("Off", UI_FONT_REGULAR);

    if (settings_text_width("On", UI_FONT_REGULAR) > toggle) {
        toggle = settings_text_width("On", UI_FONT_REGULAR);
    }

    toggle += 2 * pad;

    ui_widget_set_rect(datetime.on, automatic.x + automatic.width - toggle, automatic.y + 5, toggle, automatic.height - 10);
    ui_widget_set_rect(datetime.off, automatic.x + automatic.width - 2 * toggle - SETTINGS_GAP, automatic.y + 5, toggle, automatic.height - 10);


    const ui_rect_t server = settings_card_value(&datetime.card, DATETIME_SERVER);
    const int sync         = settings_text_width("Sync now", UI_FONT_REGULAR) + 2 * pad;
    const int field        = server.width - sync - SETTINGS_GAP;

    ui_widget_set_rect(datetime.server, server.x, server.y + 5, field > 0 ? field : 0, server.height - 10);
    ui_widget_set_rect(datetime.sync, server.x + server.width - sync, server.y + 5, sync, server.height - 10);


    const ui_rect_t manual = settings_card_value(&datetime.card, DATETIME_MANUAL);
    const int set          = sync;
    const int fields       = manual.width - set - 2 * SETTINGS_GAP;
    const int date         = fields > 0 ? fields * 11 / 20 : 0;
    const int clock        = fields > 0 ? fields - date : 0;

    ui_widget_set_rect(datetime.date, manual.x, manual.y + 5, date, manual.height - 10);
    ui_widget_set_rect(datetime.time, manual.x + date + SETTINGS_GAP, manual.y + 5, clock, manual.height - 10);
    ui_widget_set_rect(datetime.set, manual.x + manual.width - set, manual.y + 5, set, manual.height - 10);


    const int line = settings_line_height();

    ui_widget_set_rect(datetime.status, page.x, y + SETTINGS_GAP, page.width, line);

    y += SETTINGS_GAP + line + SETTINGS_GAP;


    const int search  = page.width * 9 / 20;
    const int heading = page.width - search - SETTINGS_GAP;

    ui_widget_set_rect(datetime.zone_heading, page.x, y, heading > 0 ? heading : 0, control);
    ui_widget_set_rect(datetime.search, page.x + page.width - search, y, search, control);

    y += control + SETTINGS_GAP;

    const int bottom = page.y + page.height;

    ui_widget_set_rect(datetime.list, page.x, y, page.width, bottom - y > 0 ? bottom - y : 0);

    if (ui_list_selected(datetime.list) >= 0) {
        ui_list_scroll_to(datetime.list, ui_list_selected(datetime.list));
    }
}


static void datetime_restyle(void) {

    settings_header_restyle(&datetime.header);
    settings_card_restyle(&datetime.card);

    ui_label_set_color(datetime.status, ui_theme()->text_muted);

    datetime_update_controls();
}


static void datetime_enter(void) {

    if (!datetime.loaded) {
        datetime_load_zones();
        datetime.loaded = true;
    }

    datetime_reload_zone();
    datetime_read_zone();

    datetime.automatic = datetime_service_enabled();

    if (!datetime_job_running()) {
        datetime_report("");
        datetime_read_server();
    }

    datetime_show_zone();
    datetime_show_now();
    datetime_fill_manual();
    datetime_fill();
    datetime_update_controls();
}


static int datetime_poll(uint64_t now) {

    datetime_drain();

    int status;

    const pid_t e = waitpid(datetime.job.pid, &status, WNOHANG);

    if (e == 0) {

        if (datetime.job.deadline == 0 || now < datetime.job.deadline) {
            return DATETIME_POLL_MS;
        }

        kill(datetime.job.pid, SIGKILL);
        waitpid(datetime.job.pid, &status, 0);

        status = -1;

    } else {
        status = e < 0 ? 1 : WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }

    datetime_drain();

    close(datetime.job.fd);

    const datetime_done_fn done = datetime.job.done;

    datetime.job.pid = 0;
    datetime.job.fd  = -1;

    done(status, datetime.job.output);

    datetime_update_controls();

    return datetime_job_running() ? 0 : -1;
}


static int datetime_tick(uint64_t now, bool visible) {

    int wait = -1;

    if (datetime_job_running()) {
        wait = datetime_poll(now);
    }

    if (!visible) {
        return wait;
    }

    datetime_show_now();

    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);

    const int second = (int)((1000000000L - ts.tv_nsec) / 1000000L) + 1;

    return wait >= 0 && wait < second ? wait : second;
}


const settings_page_t settings_datetime = {
    .name    = "datetime",
    .label   = "Date & time",
    .icon    = "preferences-system-time",
    .build   = datetime_build,
    .layout  = datetime_layout,
    .restyle = datetime_restyle,
    .enter   = datetime_enter,
    .tick    = datetime_tick,
};
