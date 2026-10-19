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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pkg.h"


/**
 * @brief Tells whether a string is a valid package name.
 * @param s The string.
 * @return True when it is non-empty and made of letters, digits and "+-._@".
 */
static bool pkg_index_name(const char* s) {

    if (!*s)
        return false;

    for (; *s; s++) {

        if (!isalnum((unsigned char)*s) && !strchr("+-._@", *s))
            return false;
    }

    return true;
}


/**
 * @brief Validates a hex SHA-256 digest and lowercases it in place.
 * @param s The digest.
 * @return True when it is well formed.
 */
static bool pkg_index_sha256(char* s) {

    size_t len = 0;

    for (; s[len]; len++) {

        if (!isxdigit((unsigned char)s[len]))
            return false;

        s[len] = (char)tolower((unsigned char)s[len]);
    }

    return len == PKG_SHA256_LEN;
}


/**
 * @brief Splits off the next ':'-separated field in place.
 * @param s The cursor, moved past the separator; NULL once the line is consumed.
 * @return The field, or NULL when none is left.
 */
static char* pkg_index_field(char** s) {

    char* field = *s;

    if (!field)
        return NULL;

    char* colon = strchr(field, ':');

    if (colon) {
        *colon = '\0';
        *s     = colon + 1;
    } else {
        *s = NULL;
    }

    return field;
}


/**
 * @brief Replaces the packages of a repository with the lines of its index, "name:version:sha256[:dep,dep...]".
 * @param db The local index.
 * @param repo The repository.
 * @param index The content of the remote index, modified in place.
 * @return The number of packages loaded, or -1 on error.
 */
static int pkg_index_load(sqlite3* db, const char* repo, char* index) {

    if (pkg_db_run(db, "DELETE FROM remote WHERE repo = ?", "s", repo) < 0)
        return -1;

    sqlite3_stmt* package = pkg_db_prepare(db, "INSERT OR IGNORE INTO remote (repo, name, version, sha256) VALUES (?, ?, ?, ?)", NULL);
    sqlite3_stmt* depends = pkg_db_prepare(db, "INSERT OR IGNORE INTO remote_deps (repo, name, dep) VALUES (?, ?, ?)", NULL);

    if (!package || !depends) {
        sqlite3_finalize(package);
        sqlite3_finalize(depends);
        return -1;
    }


    int count     = 0;
    size_t lineno = 0;

    for (char* line = index; line && *line;) {

        char* next = strchr(line, '\n');

        if (next)
            *next++ = '\0';

        lineno++;
        line[strcspn(line, "\r")] = '\0';

        if (!*line || *line == '#') {
            line = next;
            continue;
        }

        char* cursor  = line;
        char* name    = pkg_index_field(&cursor);
        char* version = pkg_index_field(&cursor);
        char* sha256  = pkg_index_field(&cursor);
        char* deps    = cursor;

        if (!name || !version || !sha256 || !pkg_index_name(name) || !*version || !pkg_index_sha256(sha256)) {
            pkg_warn("%s: index line %zu is malformed", repo, lineno);
            line = next;
            continue;
        }

        sqlite3_reset(package);
        sqlite3_bind_text(package, 1, repo, -1, SQLITE_STATIC);
        sqlite3_bind_text(package, 2, name, -1, SQLITE_STATIC);
        sqlite3_bind_text(package, 3, version, -1, SQLITE_STATIC);
        sqlite3_bind_text(package, 4, sha256, -1, SQLITE_STATIC);

        if (sqlite3_step(package) != SQLITE_DONE) {
            pkg_db_error(db);
            count = -1;
            break;
        }

        if (sqlite3_changes(db) == 0) {
            pkg_warn("%s: index line %zu repeats package %s", repo, lineno, name);
            line = next;
            continue;
        }

        for (char* dep = deps ? strtok(deps, ",") : NULL; dep; dep = strtok(NULL, ",")) {

            while (isspace((unsigned char)*dep))
                dep++;

            dep[strcspn(dep, " \t")] = '\0';

            if (!pkg_index_name(dep)) {
                pkg_warn("%s: index line %zu has a malformed dependency", repo, lineno);
                continue;
            }

            sqlite3_reset(depends);
            sqlite3_bind_text(depends, 1, repo, -1, SQLITE_STATIC);
            sqlite3_bind_text(depends, 2, name, -1, SQLITE_STATIC);
            sqlite3_bind_text(depends, 3, dep, -1, SQLITE_STATIC);

            if (sqlite3_step(depends) != SQLITE_DONE) {
                pkg_db_error(db);
                count = -1;
                break;
            }
        }

        if (count < 0)
            break;

        count++;
        line = next;
    }

    sqlite3_finalize(package);
    sqlite3_finalize(depends);

    return count;
}


/**
 * @brief Counts the installed packages whose repository version differs.
 * @param db The local index.
 * @return The count, or -1 on error.
 */
static int pkg_index_upgradable(sqlite3* db) {

    sqlite3_stmt* stmt = pkg_db_prepare(db, "SELECT COUNT(*) FROM installed i JOIN candidates c ON c.name = i.name WHERE c.version != i.version", NULL);

    if (!stmt)
        return -1;

    int count = sqlite3_step(stmt) == SQLITE_ROW ? sqlite3_column_int(stmt, 0) : -1;

    sqlite3_finalize(stmt);

    return count;
}


/**
 * @brief Fetches the index of every repository of pkg.conf into the local index, dropping repositories no longer listed.
 * @param db The local index.
 * @param argc The number of arguments.
 * @param argv The arguments, starting with the command.
 * @return The exit status.
 */
int pkg_cmd_update(sqlite3* db, int argc, char** argv) {

    (void)argv;

    if (argc > 1) {
        pkg_error("update: takes no arguments");
        return 1;
    }

    pkg_repo_t* repos = pkg_conf_load(PKG_CONF_PATH);

    if (!repos)
        return 1;


    int failures = 0;
    int priority = 0;

    if (pkg_db_begin(db) < 0 || pkg_db_run(db, "UPDATE repos SET priority = -1", NULL) < 0)
        goto fail;

    for (pkg_repo_t* repo = repos; repo; repo = repo->next, priority++) {

        if (pkg_db_run(db,
                       "INSERT INTO repos (name, url, priority) VALUES (?, ?, ?) "
                       "ON CONFLICT (name) DO UPDATE SET url = excluded.url, priority = excluded.priority",
                       "ssi", repo->name, repo->url, priority) < 0)
            goto fail;

        char* url   = pkg_format("%s/index", repo->url);
        char* index = pkg_fetch_mem(url, NULL);

        free(url);

        if (!index) {
            failures++;
            continue;
        }

        int count = pkg_index_load(db, repo->name, index);

        free(index);

        if (count < 0)
            goto fail;

        if (pkg_db_run(db, "UPDATE repos SET updated = ? WHERE name = ?", "ls", (long long)time(NULL), repo->name) < 0)
            goto fail;

        printf("%s: %d packages\n", repo->name, count);
    }

    if (pkg_db_run(db, "DELETE FROM repos WHERE priority < 0", NULL) < 0 || pkg_db_commit(db) < 0)
        goto fail;

    pkg_conf_free(repos);


    int upgradable = pkg_index_upgradable(db);

    if (upgradable > 0)
        printf("%d installed packages can be upgraded, run 'pkg upgrade'\n", upgradable);

    return failures ? 1 : 0;


fail:
    pkg_db_rollback(db);
    pkg_conf_free(repos);
    return 1;
}
