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
#include <time.h>

#include "pkg.h"


/**
 * @brief Prints a labelled line listing the first column of a query, or "none".
 * @param db The local index.
 * @param label The label.
 * @param sql The query.
 * @param fmt The parameter format, see pkg_db_prepare().
 * @param a The first parameter.
 * @param b The second parameter.
 */
static void pkg_query_names(sqlite3* db, const char* label, const char* sql, const char* fmt, const char* a, const char* b) {

    sqlite3_stmt* stmt = pkg_db_prepare(db, sql, fmt, a, b);
    size_t count       = 0;

    printf("%-14s:", label);

    while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
        printf(" %s", sqlite3_column_text(stmt, 0));
        count++;
    }

    printf("%s\n", count ? "" : " none");

    sqlite3_finalize(stmt);
}


/**
 * @brief Tells whether any repository index has been fetched, hinting at 'pkg update' when none has.
 * @param db The local index.
 * @return True when there is an index.
 */
static bool pkg_query_indexed(sqlite3* db) {

    sqlite3_stmt* stmt = pkg_db_prepare(db, "SELECT 1 FROM repos WHERE updated > 0 LIMIT 1", NULL);
    bool indexed       = stmt && sqlite3_step(stmt) == SQLITE_ROW;

    sqlite3_finalize(stmt);

    if (!indexed)
        printf("no repository index yet, run 'pkg update'\n");

    return indexed;
}


/**
 * @brief Lists the repository packages whose name contains a pattern, or all of them.
 * @param db The local index.
 * @param argc The number of arguments.
 * @param argv The arguments, starting with the command.
 * @return The exit status.
 */
int pkg_cmd_search(sqlite3* db, int argc, char** argv) {

    if (argc > 2) {
        pkg_error("search: takes one pattern");
        return 1;
    }

    if (!pkg_query_indexed(db))
        return 1;


    sqlite3_stmt* stmt = pkg_db_prepare(db,
                                        "SELECT r.repo, r.name, r.version, i.version FROM remote r "
                                        "JOIN repos p ON p.name = r.repo LEFT JOIN installed i ON i.name = r.name "
                                        "WHERE instr(lower(r.name), lower(?)) > 0 ORDER BY r.name, p.priority",
                                        "s", argc > 1 ? argv[1] : "");

    if (!stmt)
        return 1;

    size_t count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {

        const char* version   = (const char*)sqlite3_column_text(stmt, 2);
        const char* installed = (const char*)sqlite3_column_text(stmt, 3);

        printf("%s/%s %s", sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1), version);

        if (installed && strcmp(installed, version) == 0)
            printf(" [installed]");
        else if (installed)
            printf(" [installed: %s]", installed);

        printf("\n");
        count++;
    }

    sqlite3_finalize(stmt);

    if (count == 0 && argc > 1)
        printf("no package matches '%s'\n", argv[1]);

    return count ? 0 : 1;
}


/**
 * @brief Shows what the local index knows about packages.
 * @param db The local index.
 * @param argc The number of arguments.
 * @param argv The arguments, starting with the command.
 * @return The exit status.
 */
int pkg_cmd_info(sqlite3* db, int argc, char** argv) {

    if (argc < 2) {
        pkg_error("info: no package specified");
        return 1;
    }

    int r = 0;

    for (int i = 1; i < argc; i++) {

        const char* name = argv[i];

        sqlite3_stmt* installed = pkg_db_prepare(db, "SELECT version, repo, sha256, explicit, date, (SELECT COUNT(*) FROM files WHERE package = ?1) FROM installed WHERE name = ?1", "s", name);
        sqlite3_stmt* candidate = pkg_db_prepare(db, "SELECT repo, version, sha256 FROM candidates WHERE name = ?", "s", name);

        bool local  = installed && sqlite3_step(installed) == SQLITE_ROW;
        bool remote = candidate && sqlite3_step(candidate) == SQLITE_ROW;

        if (!local && !remote) {

            pkg_error("%s: no such package", name);
            r = 1;

        } else {

            if (i > 1)
                printf("\n");

            printf("%-14s: %s\n", "Name", name);

            if (local) {

                char date[32] = "unknown";
                time_t when   = (time_t)sqlite3_column_int64(installed, 4);
                struct tm* tm = localtime(&when);

                if (tm)
                    strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);

                printf("%-14s: %s, %s, on %s\n", "Installed", sqlite3_column_text(installed, 0), sqlite3_column_int(installed, 3) ? "explicitly" : "as a dependency", date);
                printf("%-14s: %s\n", "From", sqlite3_column_text(installed, 1) ? (const char*)sqlite3_column_text(installed, 1) : "system image");
                printf("%-14s: %d\n", "Files", sqlite3_column_int(installed, 5));

                if (sqlite3_column_text(installed, 2))
                    printf("%-14s: %s\n", "SHA-256", sqlite3_column_text(installed, 2));

            } else {
                printf("%-14s: no\n", "Installed");
            }

            if (remote)
                printf("%-14s: %s from %s\n", "Available", sqlite3_column_text(candidate, 1), sqlite3_column_text(candidate, 0));
            else
                printf("%-14s: in no repository\n", "Available");

            if (local)
                pkg_query_names(db, "Depends on", "SELECT dep FROM installed_deps WHERE name = ? ORDER BY dep", "s", name, NULL);
            else
                pkg_query_names(db, "Depends on", "SELECT dep FROM remote_deps WHERE repo = ? AND name = ? ORDER BY dep", "ss", (const char*)sqlite3_column_text(candidate, 0), name);

            pkg_query_names(db, "Required by", "SELECT name FROM installed_deps WHERE dep = ?1 AND name != ?1 ORDER BY name", "s", name, NULL);
        }

        sqlite3_finalize(installed);
        sqlite3_finalize(candidate);
    }

    return r;
}


/**
 * @brief Lists installed packages, marking dependencies and pending upgrades.
 * @param db The local index.
 * @param argc The number of arguments.
 * @param argv The arguments, starting with the command.
 * @return The exit status.
 */
int pkg_cmd_list(sqlite3* db, int argc, char** argv) {

    (void)argv;

    if (argc > 1) {
        pkg_error("list: takes no arguments");
        return 1;
    }

    sqlite3_stmt* stmt = pkg_db_prepare(db, "SELECT i.name, i.version, i.explicit, c.version FROM installed i LEFT JOIN candidates c ON c.name = i.name ORDER BY i.name", NULL);

    if (!stmt)
        return 1;

    while (sqlite3_step(stmt) == SQLITE_ROW) {

        const char* version   = (const char*)sqlite3_column_text(stmt, 1);
        const char* available = (const char*)sqlite3_column_text(stmt, 3);

        printf("%s %s", sqlite3_column_text(stmt, 0), version);

        if (!sqlite3_column_int(stmt, 2))
            printf(" [dependency]");

        if (available && strcmp(available, version) != 0)
            printf(" [upgradable to %s]", available);

        printf("\n");
    }

    sqlite3_finalize(stmt);
    return 0;
}


/**
 * @brief Lists the paths installed packages own, directories with a trailing slash.
 * @param db The local index.
 * @param argc The number of arguments.
 * @param argv The arguments, starting with the command.
 * @return The exit status.
 */
int pkg_cmd_files(sqlite3* db, int argc, char** argv) {

    if (argc < 2) {
        pkg_error("files: no package specified");
        return 1;
    }

    int r = 0;

    for (int i = 1; i < argc; i++) {

        sqlite3_stmt* stmt = pkg_db_prepare(db, "SELECT 1 FROM installed WHERE name = ?", "s", argv[i]);
        bool installed     = stmt && sqlite3_step(stmt) == SQLITE_ROW;

        sqlite3_finalize(stmt);

        if (!installed) {
            pkg_error("%s: not installed", argv[i]);
            r = 1;
            continue;
        }

        if (!(stmt = pkg_db_prepare(db, "SELECT path, type FROM files WHERE package = ? ORDER BY path", "s", argv[i])))
            return 1;

        while (sqlite3_step(stmt) == SQLITE_ROW) {

            const char* path = (const char*)sqlite3_column_text(stmt, 0);
            bool dir         = *(const char*)sqlite3_column_text(stmt, 1) == 'd';

            printf("%s %s%s\n", argv[i], path, dir ? "/" : "");
        }

        sqlite3_finalize(stmt);
    }

    return r;
}
