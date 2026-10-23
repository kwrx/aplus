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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>

void show_usage(void) {
    printf("Use: shutdown [OPTION] TIME...\n"
              "Shutdown the system.\n\n"
              "Options:\n"
              "  -H, --halt     Halt the machine\n"
              "  -P, --poweroff Power off the machine\n"
              "  -r, --reboot   Reboot the machine\n"
              "  -h, --help     Show this help message and exit\n"
              "  -v, --version  Show version information and exit\n");
}

void show_version(void) {
    printf("shutdown (aplus coreutils) 0.1\n"
           "Copyright (c) %s Antonino Natale.\n"
           "Built with gcc %s (%s)\n",
           &__DATE__[7], __VERSION__, __TIMESTAMP__);
}

int main(int argc, char** argv) {
    static const struct option long_options[] = {
        {"halt",     no_argument, NULL, 'H'},
        {"poweroff", no_argument, NULL, 'P'},
        {"reboot",   no_argument, NULL, 'r'},
        {"help",     no_argument, NULL, 'h'},
        {"version",  no_argument, NULL, 'v'},
        {NULL,       0,           NULL, 0  },
    };

    int sig_to_send = SIGTERM;
    int delay = 60;

    int opt;
    while ((opt = getopt_long(argc, argv, "HPrhv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'H':
                sig_to_send = SIGUSR1;
                break;
            case 'P':
                sig_to_send = SIGUSR2;
                break;
            case 'r':
                sig_to_send = SIGTERM;
                break;
            case 'h':
                show_usage();
                return 0;
            case 'v':
                show_version();
                return 0;
            default:
                show_usage();
                return 1;
        }
    }

    if (geteuid() != 0 && getuid() != 0) {
        fprintf(stderr, "shutdown: you must be root to shutdown the system\n");
        return 1;
    }

    if (optind < argc) {
        if (strcmp(argv[optind], "now") == 0) {
            delay = 0;
        } else if (argv[optind][0] == '+') {
            delay = atoi(argv[optind] + 1);
        } else {
            fprintf(stderr, "shutdown: invalid time argument: %s\n", argv[optind]);
            return 1;
        }
    }

    if (delay > 0) {
        printf("Shutdown scheduled in %d seconds...\n", delay);
        sleep(delay);
    }

    return kill(1, sig_to_send);
}