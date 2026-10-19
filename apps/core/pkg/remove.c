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

#include "pkg.h"


#define PKG_ORPHANS_SQL                                                       \
    "WITH RECURSIVE needed (name) AS ("                                       \
    "    SELECT name FROM installed WHERE explicit = 1"                       \
    "    UNION"                                                               \
    "    SELECT d.dep FROM installed_deps d JOIN needed n ON d.name = n.name" \
    ") "                                                                      \
    "SELECT name FROM installed WHERE name NOT IN (SELECT name FROM needed) ORDER BY name"


/**
 * @brief Tells whether a list of package names holds one.
 * @param names The names.
 * @param name The name.
 * @return True when it does.
 */
static bool pkg_remove_listed(const pkg_entries_t* names, const char* name) {

    for (size_t i = 0; i < names->count; i++) {

        if (strcmp(names->items[i].path, name) == 0)
            return true;
    }

    return false;
}


/**
 * @brief Collects the installed packages that no explicitly installed package needs.
 * @param db The local index.
 * @param orphans Where to append their names.
 * @return 0 on success, -1 on error.
 */
static int pkg_remove_orphans(sqlite3* db, pkg_entries_t* orphans) {

    sqlite3_stmt* stmt = pkg_db_prepare(db, PKG_ORPHANS_SQL, NULL);

    if (!stmt)
        return -1;

    while (sqlite3_step(stmt) == SQLITE_ROW)
        pkg_entries_add(orphans, (const char*)sqlite3_column_text(stmt, 0), 'f');

    sqlite3_finalize(stmt);
    return 0;
}


/**
 * @brief Removes the given packages after showing them and asking for confirmation.
 * @param db The local index.
 * @param names The packages.
 * @return The exit status.
 */
static int pkg_remove_apply(sqlite3* db, const pkg_entries_t* names) {

    printf("\npackages (%zu):\n", names->count);

    for (size_t i = 0; i < names->count; i++)
        printf("  %s\n", names->items[i].path);

    printf("\n");

    if (!pkg_confirm("remove?"))
        return 1;


    for (size_t i = 0; i < names->count; i++) {

        pkg_entries_t entries = {0};

        printf("removing %s\n", names->items[i].path);
        fflush(stdout);

        if (pkg_entries_load(db, names->items[i].path, &entries) < 0 || pkg_db_run(db, "DELETE FROM installed WHERE name = ?", "s", names->items[i].path) < 0) {
            pkg_entries_free(&entries);
            return 1;
        }

        pkg_entries_purge(db, &entries);
        pkg_entries_free(&entries);
    }

    return 0;
}


/**
 * @brief Removes installed packages; one that another installed package needs stays, unless --force.
 * @param db The local index.
 * @param argc The number of arguments.
 * @param argv The arguments, starting with the command.
 * @return The exit status.
 */
int pkg_cmd_remove(sqlite3* db, int argc, char** argv) {

    if (argc < 2) {
        pkg_error("remove: no package specified");
        return 1;
    }

    pkg_entries_t names = {0};
    int r               = 0;

    for (int i = 1; i < argc; i++) {

        sqlite3_stmt* stmt = pkg_db_prepare(db, "SELECT 1 FROM installed WHERE name = ?", "s", argv[i]);

        if (stmt && sqlite3_step(stmt) == SQLITE_ROW) {

            if (!pkg_remove_listed(&names, argv[i]))
                pkg_entries_add(&names, argv[i], 'f');

        } else {
            pkg_error("%s: not installed", argv[i]);
            r = 1;
        }

        sqlite3_finalize(stmt);
    }

    for (size_t i = 0; r == 0 && !pkg_options.force && i < names.count; i++) {

        sqlite3_stmt* stmt = pkg_db_prepare(db, "SELECT name FROM installed_deps WHERE dep = ?1 AND name != ?1 ORDER BY name", "s", names.items[i].path);

        while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {

            const char* dependent = (const char*)sqlite3_column_text(stmt, 0);

            if (!pkg_remove_listed(&names, dependent)) {
                pkg_error("%s is needed by %s, remove both or use --force", names.items[i].path, dependent);
                r = 1;
            }
        }

        sqlite3_finalize(stmt);
    }

    if (r == 0)
        r = pkg_remove_apply(db, &names);

    pkg_entries_free(&names);


    if (r == 0) {

        pkg_entries_t orphans = {0};

        if (pkg_remove_orphans(db, &orphans) == 0 && orphans.count > 0)
            printf("%zu packages are no longer needed, run 'pkg autoremove'\n", orphans.count);

        pkg_entries_free(&orphans);
    }

    return r;
}


/**
 * @brief Removes the packages installed as dependencies that no explicitly installed package needs anymore.
 * @param db The local index.
 * @param argc The number of arguments.
 * @param argv The arguments, starting with the command.
 * @return The exit status.
 */
int pkg_cmd_autoremove(sqlite3* db, int argc, char** argv) {

    (void)argv;

    if (argc > 1) {
        pkg_error("autoremove: takes no arguments");
        return 1;
    }

    pkg_entries_t orphans = {0};

    if (pkg_remove_orphans(db, &orphans) < 0)
        return 1;

    int r = 0;

    if (orphans.count == 0)
        printf("nothing to do\n");
    else
        r = pkg_remove_apply(db, &orphans);

    pkg_entries_free(&orphans);
    return r;
}
