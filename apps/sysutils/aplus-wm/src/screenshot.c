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

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <aplus/ulog.h>

#include <wm.h>


static cairo_status_t wm_screenshot_write(void* closure, const unsigned char* data, unsigned int length) {

    const int fd = *(int*)closure;

    while (length > 0) {

        const ssize_t n = write(fd, data, length);

        if (n < 0) {

            if (errno == EINTR) {
                continue;
            }

            return CAIRO_STATUS_WRITE_ERROR;
        }

        data += n;
        length -= (unsigned int)n;
    }

    return CAIRO_STATUS_SUCCESS;
}


typedef struct {
    cairo_surface_t* shot;
    int fd;
    char part[WM_APP_PATH_MAX];
    char path[WM_APP_PATH_MAX];
} wm_screenshot_job_t;


static void* wm_screenshot_save(void* arg) {

    wm_screenshot_job_t* job = arg;
    cairo_status_t status = cairo_surface_write_to_png_stream(job->shot, wm_screenshot_write, &job->fd);

    if (close(job->fd) < 0 && status == CAIRO_STATUS_SUCCESS) {
        status = CAIRO_STATUS_WRITE_ERROR;
    }

    if (status == CAIRO_STATUS_SUCCESS && rename(job->part, job->path) == 0) {
        ulog_debugf("saved a screenshot to %s", job->path);
    } else {
        ulog_errorf("cannot write the screenshot %s: %s", job->path, status == CAIRO_STATUS_SUCCESS ? strerror(errno) : cairo_status_to_string(status));
        unlink(job->part);
    }

    cairo_surface_destroy(job->shot);
    free(job);

    return NULL;
}


static int wm_screenshot_open(wm_screenshot_job_t* job) {

    char dir[WM_APP_PATH_MAX];
    char stamp[32] = "unknown";

    const char* home = getenv("HOME");
    if (!home || snprintf(dir, sizeof(dir), "%s/" UI_SCREENSHOT_DIR, home) >= (int)sizeof(dir) || (mkdir(dir, 0755) < 0 && errno != EEXIST)) {
        snprintf(dir, sizeof(dir), UI_SCREENSHOT_FALLBACK);
    }

    const time_t t = time(NULL);
    struct tm tm;

    if (localtime_r(&t, &tm)) {
        strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tm);
    }

    for (size_t counter = 0; counter < 100; counter++) {

        int n;

        if (counter == 0) {
            n = snprintf(job->path, sizeof(job->path), "%s/" UI_SCREENSHOT_PREFIX "%s.png", dir, stamp);
        } else {
            n = snprintf(job->path, sizeof(job->path), "%s/" UI_SCREENSHOT_PREFIX "%s-%zu.png", dir, stamp, counter);
        }

        if (n < 0 || snprintf(job->part, sizeof(job->part), "%s.part", job->path) >= (int)sizeof(job->part)) {
            return errno = ENAMETOOLONG, -1;
        }

        if (access(job->path, F_OK) == 0) {
            continue;
        }

        if ((job->fd = open(job->part, O_WRONLY | O_CREAT | O_EXCL, 0644)) >= 0) {
            return 0;
        }

        if (errno != EEXIST) {
            return -1;
        }
    }

    return errno = EEXIST, -1;
}


int wm_screenshot_take(wm_display_t* display, char* path, size_t size) {

    wm_screenshot_job_t* job = calloc(1, sizeof(*job));
    if (!job) {
        return -1;
    }

    if (wm_screenshot_open(job) < 0) {
        ulog_errorf("cannot create the screenshot %s: %s", job->path, strerror(errno));
        free(job);
        return -1;
    }


    job->shot = cairo_image_surface_create(CAIRO_FORMAT_RGB24, display->width, display->height);

    if (cairo_surface_status(job->shot) != CAIRO_STATUS_SUCCESS) {
        ulog_errorf("cannot allocate the screenshot: %s", cairo_status_to_string(cairo_surface_status(job->shot)));
        errno = ENOMEM;
        goto fail;
    }


    cairo_t* cr = cairo_create(job->shot);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr, display->back, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    cairo_surface_flush(job->shot);


    if (path && size) {
        snprintf(path, size, "%s", job->path);
    }


    pthread_t thread;
    int e;

    if ((e = pthread_create(&thread, NULL, wm_screenshot_save, job)) != 0) {
        ulog_errorf("cannot start the screenshot writer: %s", strerror(e));
        errno = e;
        goto fail;
    }

    pthread_detach(thread);
    return 0;

fail:
    close(job->fd);
    unlink(job->part);
    cairo_surface_destroy(job->shot);
    free(job);
    return -1;
}
