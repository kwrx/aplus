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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pkg.h"


/**
 * @brief Appends an entry to a list.
 * @param entries The list.
 * @param path The absolute path.
 * @param type 'f', 'l' or 'd'.
 */
void pkg_entries_add(pkg_entries_t* entries, const char* path, char type) {

    if (entries->count == entries->capacity) {

        size_t capacity    = entries->capacity ? entries->capacity * 2 : 64;
        pkg_entry_t* items = realloc(entries->items, capacity * sizeof(*items));

        if (!items) {
            pkg_error("out of memory");
            exit(1);
        }

        entries->items    = items;
        entries->capacity = capacity;
    }

    entries->items[entries->count].path = pkg_strdup(path);
    entries->items[entries->count].type = type;
    entries->count++;
}


/**
 * @brief Frees the entries of a list and empties it.
 * @param entries The list.
 */
void pkg_entries_free(pkg_entries_t* entries) {

    for (size_t i = 0; i < entries->count; i++)
        free(entries->items[i].path);

    free(entries->items);

    entries->items    = NULL;
    entries->count    = 0;
    entries->capacity = 0;
}


/**
 * @brief Loads the paths an installed package owns.
 * @param db The local index.
 * @param package The package.
 * @param entries The list to append to.
 * @return 0 on success, -1 on error.
 */
int pkg_entries_load(sqlite3* db, const char* package, pkg_entries_t* entries) {

    sqlite3_stmt* stmt = pkg_db_prepare(db, "SELECT path, type FROM files WHERE package = ?", "s", package);

    if (!stmt)
        return -1;

    int rc;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        pkg_entries_add(entries, (const char*)sqlite3_column_text(stmt, 0), *(const char*)sqlite3_column_text(stmt, 1));

    if (rc != SQLITE_DONE)
        pkg_db_error(db);

    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE ? 0 : -1;
}


/**
 * @brief Orders entries so that the deepest paths come first.
 * @param a The first entry.
 * @param b The second entry.
 * @return The qsort() ordering.
 */
static int pkg_entries_deepest(const void* a, const void* b) {

    size_t la = strlen(((const pkg_entry_t*)a)->path);
    size_t lb = strlen(((const pkg_entry_t*)b)->path);

    return (la < lb) - (la > lb);
}


/**
 * @brief Deletes from disk the entries no installed package owns any longer; non-empty directories stay.
 * @param db The local index.
 * @param entries The entries, reordered by depth.
 */
void pkg_entries_purge(sqlite3* db, pkg_entries_t* entries) {

    if (entries->count == 0)
        return;

    sqlite3_stmt* stmt = pkg_db_prepare(db, "SELECT 1 FROM files WHERE path = ? LIMIT 1", NULL);

    if (!stmt)
        return;

    qsort(entries->items, entries->count, sizeof(*entries->items), pkg_entries_deepest);

    for (size_t i = 0; i < entries->count; i++) {

        const pkg_entry_t* entry = &entries->items[i];

        sqlite3_reset(stmt);
        sqlite3_bind_text(stmt, 1, entry->path, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
            continue;

        if (entry->type == 'd')
            rmdir(entry->path);
        else if (unlink(entry->path) < 0 && errno != ENOENT)
            pkg_warn("cannot remove %s: %s", entry->path, strerror(errno));
    }

    sqlite3_finalize(stmt);
}
