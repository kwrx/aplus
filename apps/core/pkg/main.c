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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pkg.h"


/** @brief A command of the pkg command line. */
typedef struct pkg_command {
    const char* name;
    const char* args;
    const char* help;
    bool writable;
    int (*run)(sqlite3* db, int argc, char** argv);
} pkg_command_t;


static const pkg_command_t pkg_commands[] = {
    {"update",     "",             "fetch the package index of every repository", true,  pkg_cmd_update    },
    {"install",    "<package>...", "install packages and their dependencies",     true,  pkg_cmd_install   },
    {"upgrade",    "[package...]", "upgrade installed packages",                  true,  pkg_cmd_upgrade   },
    {"remove",     "<package>...", "remove installed packages",                   true,  pkg_cmd_remove    },
    {"autoremove", "",             "remove dependencies nothing needs anymore",   true,  pkg_cmd_autoremove},
    {"search",     "[pattern]",    "search the package index",                    false, pkg_cmd_search    },
    {"info",       "<package>...", "show details about packages",                 false, pkg_cmd_info      },
    {"list",       "",             "list installed packages",                     false, pkg_cmd_list      },
    {"files",      "<package>...", "list the files of installed packages",        false, pkg_cmd_files     },
    {"clean",      "",             "delete downloaded archives",                  true,  pkg_cmd_clean     },
};


/**
 * @brief Prints the usage.
 * @param fp Where to print it.
 */
static void pkg_usage(FILE* fp) {

    fprintf(fp, "Usage: %s [options] <command> [args]\n", PKG_NAME);
    fprintf(fp, "\nOptions:\n");
    fprintf(fp, "  -y, --yes      answer yes to every question\n");
    fprintf(fp, "  -f, --force    reinstall up-to-date packages, remove needed ones\n");
    fprintf(fp, "  -h, --help     show this help\n");
    fprintf(fp, "  -v, --version  show the version\n");
    fprintf(fp, "\nCommands:\n");

    for (size_t i = 0; i < sizeof(pkg_commands) / sizeof(*pkg_commands); i++) {
        char usage[64];
        snprintf(usage, sizeof(usage), "%s %s", pkg_commands[i].name, pkg_commands[i].args);
        fprintf(fp, "  %-26s %s\n", usage, pkg_commands[i].help);
    }

    fprintf(fp, "\nRepositories are read from %s, the local index lives in %s.\n", PKG_CONF_PATH, PKG_DB_PATH);
}


/**
 * @brief Runs one pkg command.
 * @param argc The number of arguments.
 * @param argv The arguments.
 * @return The exit status.
 */
int main(int argc, char** argv) {

    static const struct option options[] = {
        {"yes",     no_argument, NULL, 'y'},
        {"force",   no_argument, NULL, 'f'},
        {"help",    no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {NULL,      0,           NULL, 0  },
    };

    int opt;

    while ((opt = getopt_long(argc, argv, "yfhv", options, NULL)) != -1) {

        switch (opt) {
            case 'y':
                pkg_options.yes = true;
                break;
            case 'f':
                pkg_options.force = true;
                break;
            case 'h':
                pkg_usage(stdout);
                return 0;
            case 'v':
                printf("%s %s\n", PKG_NAME, PKG_VERSION);
                return 0;
            default:
                pkg_usage(stderr);
                return 1;
        }
    }

    if (optind >= argc) {
        pkg_usage(stderr);
        return 1;
    }


    const pkg_command_t* command = NULL;

    for (size_t i = 0; i < sizeof(pkg_commands) / sizeof(*pkg_commands); i++) {

        if (strcmp(pkg_commands[i].name, argv[optind]) == 0)
            command = &pkg_commands[i];
    }

    if (!command) {
        pkg_error("unknown command '%s', see '%s --help'", argv[optind], PKG_NAME);
        return 1;
    }

    if (command->writable && geteuid() != 0) {
        pkg_error("%s: must be run as root", command->name);
        return 1;
    }

    if (command->writable && pkg_db_lock() < 0)
        return 1;


    sqlite3* db = pkg_db_open(command->writable);
    int r       = db ? command->run(db, argc - optind, &argv[optind]) : 1;

    pkg_db_close(db);
    pkg_db_unlock();

    return r;
}
