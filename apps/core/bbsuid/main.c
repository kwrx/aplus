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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define BBSUID_BUSYBOX "/bin/busybox"


static const char* const bbsuid_applets[] = {
    "su",
    "passwd",
};


static const char* bbsuid_applet(const char* path) {

    const char* name = strrchr(path, '/');
    name             = name ? name + 1 : path;

    for (size_t i = 0; i < sizeof(bbsuid_applets) / sizeof(bbsuid_applets[0]); i++) {
        if (strcmp(name, bbsuid_applets[i]) == 0) {
            return bbsuid_applets[i];
        }
    }

    return NULL;
}


static int bbsuid_stdfds(void) {

    for (int fd = STDIN_FILENO; fd <= STDERR_FILENO; fd++) {

        if (fcntl(fd, F_GETFD) >= 0) {
            continue;
        }

        if (errno != EBADF) {
            return -1;
        }

        int null = open("/dev/null", O_RDWR);

        if (null != fd) {

            if (null >= 0) {
                close(null);
            }

            return -1;
        }
    }

    return 0;
}


int main(int argc, char** argv) {

    if (bbsuid_stdfds() < 0) {
        _exit(EXIT_FAILURE);
    }

    if (argc < 1 || !argv[0]) {
        fprintf(stderr, "bbsuid: missing applet name\n");
        return EXIT_FAILURE;
    }

    const char* applet = bbsuid_applet(argv[0]);
    if (!applet) {
        fprintf(stderr, "bbsuid: %s: applet not allowed\n", argv[0]);
        return EXIT_FAILURE;
    }

    argv[0] = (char*)applet;
    execv(BBSUID_BUSYBOX, argv);

    fprintf(stderr, "bbsuid: %s: %s\n", BBSUID_BUSYBOX, strerror(errno));
    return 127;
}
