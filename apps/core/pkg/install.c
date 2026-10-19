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
#include <time.h>
#include <unistd.h>

#include "pkg.h"


/** @brief Why a package enters the plan. */
typedef enum pkg_want {
    PKG_WANT_EXPLICIT,
    PKG_WANT_DEPENDENCY,
    PKG_WANT_UPGRADE,
} pkg_want_t;

/** @brief A package the plan installs, upgrades or reinstalls. */
typedef struct pkg_target {
    char* name;
    char* repo;
    char* url;
    char* version;
    char* sha256;
    char* installed;
    bool explicit;
} pkg_target_t;

/** @brief The packages to install, dependencies first, and those only to mark as explicit. */
typedef struct pkg_plan {
    pkg_target_t* items;
    size_t count;
    size_t capacity;
    pkg_entries_t visiting;
    pkg_entries_t marks;
} pkg_plan_t;


/**
 * @brief Finds a package in the plan.
 * @param plan The plan.
 * @param name The package.
 * @return The target, or NULL.
 */
static pkg_target_t* pkg_plan_find(pkg_plan_t* plan, const char* name) {

    for (size_t i = 0; i < plan->count; i++) {

        if (strcmp(plan->items[i].name, name) == 0)
            return &plan->items[i];
    }

    return NULL;
}


/**
 * @brief Tells whether a list holds a path.
 * @param entries The list.
 * @param path The path.
 * @return True when it does.
 */
static bool pkg_plan_has(const pkg_entries_t* entries, const char* path) {

    for (size_t i = 0; i < entries->count; i++) {

        if (strcmp(entries->items[i].path, path) == 0)
            return true;
    }

    return false;
}


/**
 * @brief Frees a plan.
 * @param plan The plan.
 */
static void pkg_plan_free(pkg_plan_t* plan) {

    for (size_t i = 0; i < plan->count; i++) {
        free(plan->items[i].name);
        free(plan->items[i].repo);
        free(plan->items[i].url);
        free(plan->items[i].version);
        free(plan->items[i].sha256);
        free(plan->items[i].installed);
    }

    free(plan->items);

    pkg_entries_free(&plan->visiting);
    pkg_entries_free(&plan->marks);
}


/**
 * @brief Duplicates a text column, NULL when it is NULL.
 * @param stmt The statement.
 * @param col The column.
 * @return The copy, or NULL.
 */
static char* pkg_plan_column(sqlite3_stmt* stmt, int col) {

    const char* s = (const char*)sqlite3_column_text(stmt, col);

    return s ? pkg_strdup(s) : NULL;
}


/**
 * @brief Adds a package and, before it, the dependencies it lacks.
 * @param db The local index.
 * @param plan The plan.
 * @param name The package.
 * @param parent The package that depends on it, or NULL.
 * @param want Why it is added.
 * @return 0 on success, -1 when a package cannot be found.
 */
static int pkg_plan_visit(sqlite3* db, pkg_plan_t* plan, const char* name, const char* parent, pkg_want_t want) {

    pkg_target_t* found = pkg_plan_find(plan, name);

    if (found) {
        found->explicit |= want == PKG_WANT_EXPLICIT;
        return 0;
    }

    if (pkg_plan_has(&plan->visiting, name))
        return 0;


    sqlite3_stmt* stmt = pkg_db_prepare(db, "SELECT repo, url, version, sha256 FROM candidates WHERE name = ?", "s", name);

    if (!stmt)
        return -1;

    if (sqlite3_step(stmt) != SQLITE_ROW) {

        if (parent)
            pkg_error("%s: dependency %s is in no repository", parent, name);
        else
            pkg_error("%s: no such package, try 'pkg update'", name);

        sqlite3_finalize(stmt);
        return -1;
    }

    pkg_target_t target = {
        .name    = pkg_strdup(name),
        .repo    = pkg_plan_column(stmt, 0),
        .url     = pkg_plan_column(stmt, 1),
        .version = pkg_plan_column(stmt, 2),
        .sha256  = pkg_plan_column(stmt, 3),
    };

    sqlite3_finalize(stmt);


    bool explicit = false;

    if ((stmt = pkg_db_prepare(db, "SELECT version, explicit FROM installed WHERE name = ?", "s", name)) && sqlite3_step(stmt) == SQLITE_ROW) {
        target.installed = pkg_plan_column(stmt, 0);
        explicit         = sqlite3_column_int(stmt, 1) != 0;
    }

    sqlite3_finalize(stmt);

    target.explicit = explicit || want == PKG_WANT_EXPLICIT;


    bool skip = false;

    if (target.installed) {

        bool current = strcmp(target.installed, target.version) == 0;

        if (want == PKG_WANT_DEPENDENCY) {

            skip = true;

        } else if (current && !pkg_options.force) {

            skip = true;

            if (want == PKG_WANT_EXPLICIT) {

                printf("%s %s is already installed\n", name, target.installed);

                if (!explicit)
                    pkg_entries_add(&plan->marks, name, 'f');
            }
        }
    }

    if (skip) {
        free(target.name);
        free(target.repo);
        free(target.url);
        free(target.version);
        free(target.sha256);
        free(target.installed);
        return 0;
    }


    pkg_entries_t deps = {0};

    if ((stmt = pkg_db_prepare(db, "SELECT dep FROM remote_deps WHERE repo = ? AND name = ?", "ss", target.repo, name))) {

        while (sqlite3_step(stmt) == SQLITE_ROW)
            pkg_entries_add(&deps, (const char*)sqlite3_column_text(stmt, 0), 'f');

        sqlite3_finalize(stmt);
    }

    pkg_entries_add(&plan->visiting, name, 'f');

    int r = 0;

    for (size_t i = 0; r == 0 && i < deps.count; i++)
        r = pkg_plan_visit(db, plan, deps.items[i].path, name, PKG_WANT_DEPENDENCY);

    free(plan->visiting.items[--plan->visiting.count].path);
    pkg_entries_free(&deps);


    if (r < 0) {
        free(target.name);
        free(target.repo);
        free(target.url);
        free(target.version);
        free(target.sha256);
        free(target.installed);
        return -1;
    }

    if (plan->count == plan->capacity) {

        size_t capacity     = plan->capacity ? plan->capacity * 2 : 16;
        pkg_target_t* items = realloc(plan->items, capacity * sizeof(*items));

        if (!items) {
            pkg_error("out of memory");
            exit(1);
        }

        plan->items    = items;
        plan->capacity = capacity;
    }

    plan->items[plan->count++] = target;
    return 0;
}


/**
 * @brief The path of the cached archive of a target.
 * @param target The target.
 * @return The path.
 */
static char* pkg_target_archive(const pkg_target_t* target) {
    return pkg_format("%s/%s-%s.tar.xz", PKG_CACHE_DIR, target->name, target->version);
}


/**
 * @brief Downloads the archive of a target unless the cache holds it, and verifies its checksum.
 * @param target The target.
 * @return 0 on success, -1 on error.
 */
static int pkg_target_fetch(const pkg_target_t* target) {

    char* path = pkg_target_archive(target);
    char digest[PKG_SHA256_LEN + 1];

    if (pkg_sha256_file(path, digest) == 0 && strcmp(digest, target->sha256) == 0) {
        free(path);
        return 0;
    }

    char* url   = pkg_format("%s/%s.tar.xz", target->url, target->name);
    char* label = pkg_format("%s %s", target->name, target->version);

    int r = pkg_fetch_file(url, path, label);

    if (r == 0) {

        if (pkg_sha256_file(path, digest) < 0) {

            pkg_error("%s: %s", path, strerror(errno));
            r = -1;

        } else if (strcmp(digest, target->sha256) != 0) {

            pkg_error("%s: checksum mismatch, expected %s, got %s", label, target->sha256, digest);
            unlink(path);
            r = -1;
        }
    }

    free(url);
    free(label);
    free(path);

    return r;
}


/**
 * @brief Records an installed target, its files and its dependencies, taking over files other packages owned.
 * @param db The local index.
 * @param target The target.
 * @param entries The paths it installed.
 * @return 0 on success, -1 on error.
 */
static int pkg_target_record(sqlite3* db, const pkg_target_t* target, const pkg_entries_t* entries) {

    if (pkg_db_begin(db) < 0)
        return -1;

    if (pkg_db_run(db, "DELETE FROM installed WHERE name = ?", "s", target->name) < 0)
        goto fail;

    if (pkg_db_run(db, "INSERT INTO installed (name, version, repo, sha256, explicit, date) VALUES (?, ?, ?, ?, ?, ?)", "ssssil", target->name, target->version, target->repo, target->sha256, target->explicit ? 1 : 0,
                   (long long)time(NULL)) < 0)
        goto fail;


    sqlite3_stmt* stmt = pkg_db_prepare(db, "INSERT OR IGNORE INTO files (package, path, type) VALUES (?, ?, ?)", NULL);

    if (!stmt)
        goto fail;

    for (size_t i = 0; i < entries->count; i++) {

        char type[2] = {entries->items[i].type, '\0'};

        sqlite3_reset(stmt);
        sqlite3_bind_text(stmt, 1, target->name, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, entries->items[i].path, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, type, -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            pkg_db_error(db);
            sqlite3_finalize(stmt);
            goto fail;
        }
    }

    sqlite3_finalize(stmt);


    stmt = pkg_db_prepare(db,
                          "SELECT o.path, o.package FROM files o JOIN files n ON n.path = o.path "
                          "WHERE n.package = ?1 AND o.package != ?1 AND n.type != 'd' AND o.type != 'd'",
                          "s", target->name);

    if (!stmt)
        goto fail;

    while (sqlite3_step(stmt) == SQLITE_ROW)
        pkg_warn("%s: %s was owned by %s", target->name, sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1));

    sqlite3_finalize(stmt);


    if (pkg_db_run(db,
                   "DELETE FROM files WHERE package != ?1 AND type != 'd' "
                   "AND path IN (SELECT path FROM files WHERE package = ?1 AND type != 'd')",
                   "s", target->name) < 0)
        goto fail;

    if (pkg_db_run(db, "INSERT INTO installed_deps (name, dep) SELECT ?1, dep FROM remote_deps WHERE repo = ?2 AND name = ?1", "ss", target->name, target->repo) < 0)
        goto fail;

    return pkg_db_commit(db);


fail:
    pkg_db_rollback(db);
    return -1;
}


/**
 * @brief Deletes the files and links a failed new install wrote, keeping every directory.
 * @param db The local index.
 * @param entries The paths it installed.
 */
static void pkg_target_rollback(sqlite3* db, pkg_entries_t* entries) {

    pkg_entries_t written = {0};

    for (size_t i = 0; i < entries->count; i++) {

        if (entries->items[i].type != 'd')
            pkg_entries_add(&written, entries->items[i].path, entries->items[i].type);
    }

    pkg_entries_purge(db, &written);
    pkg_entries_free(&written);
}


/**
 * @brief Installs a downloaded target over the root, then deletes the files only its previous version had; a failed new install leaves nothing behind.
 * @param db The local index.
 * @param target The target.
 * @return 0 on success, -1 on error.
 */
static int pkg_target_install(sqlite3* db, const pkg_target_t* target) {

    pkg_entries_t previous = {0};
    pkg_entries_t entries  = {0};

    char* path = pkg_target_archive(target);
    int r      = -1;

    if (target->installed && strcmp(target->installed, target->version) != 0)
        printf("upgrading %s %s -> %s\n", target->name, target->installed, target->version);
    else
        printf("installing %s %s\n", target->name, target->version);

    fflush(stdout);

    if (target->installed && pkg_entries_load(db, target->name, &previous) < 0)
        goto out;

    if (pkg_archive_extract(path, target->name, &entries) < 0) {

        if (!target->installed)
            pkg_target_rollback(db, &entries);

        goto out;
    }

    if (pkg_target_record(db, target, &entries) < 0)
        goto out;

    pkg_entries_purge(db, &previous);
    r = 0;

out:
    pkg_entries_free(&previous);
    pkg_entries_free(&entries);
    free(path);

    return r;
}


/**
 * @brief Shows a plan, asks for confirmation, then downloads every archive before installing any.
 * @param db The local index.
 * @param plan The plan.
 * @return The exit status.
 */
static int pkg_plan_apply(sqlite3* db, pkg_plan_t* plan) {

    for (size_t i = 0; i < plan->marks.count; i++) {

        if (pkg_db_run(db, "UPDATE installed SET explicit = 1 WHERE name = ?", "s", plan->marks.items[i].path) < 0)
            return 1;

        printf("%s is now marked as explicitly installed\n", plan->marks.items[i].path);
    }

    if (plan->count == 0) {

        if (plan->marks.count == 0)
            printf("nothing to do\n");

        return 0;
    }


    printf("\npackages (%zu):\n", plan->count);

    for (size_t i = 0; i < plan->count; i++) {

        const pkg_target_t* target = &plan->items[i];

        if (!target->installed)
            printf("  %-24s %s%s\n", target->name, target->version, target->explicit ? "" : "  (dependency)");
        else if (strcmp(target->installed, target->version) != 0)
            printf("  %-24s %s -> %s\n", target->name, target->installed, target->version);
        else
            printf("  %-24s %s  (reinstall)\n", target->name, target->version);
    }

    printf("\n");

    if (!pkg_confirm("proceed?"))
        return 1;

    if (pkg_mkdirs(PKG_CACHE_DIR, 0755) < 0)
        return 1;


    printf("downloading:\n");

    for (size_t i = 0; i < plan->count; i++) {

        if (pkg_target_fetch(&plan->items[i]) < 0)
            return 1;
    }

    for (size_t i = 0; i < plan->count; i++) {

        if (pkg_target_install(db, &plan->items[i]) < 0)
            return 1;
    }

    return 0;
}


/**
 * @brief Installs packages and the dependencies they lack; with --force, reinstalls them.
 * @param db The local index.
 * @param argc The number of arguments.
 * @param argv The arguments, starting with the command.
 * @return The exit status.
 */
int pkg_cmd_install(sqlite3* db, int argc, char** argv) {

    if (argc < 2) {
        pkg_error("install: no package specified");
        return 1;
    }

    pkg_plan_t plan = {0};
    int r           = 0;

    for (int i = 1; r == 0 && i < argc; i++)
        r = pkg_plan_visit(db, &plan, argv[i], NULL, PKG_WANT_EXPLICIT);

    r = r == 0 ? pkg_plan_apply(db, &plan) : 1;

    pkg_plan_free(&plan);
    return r;
}


/**
 * @brief Upgrades the named installed packages, or every one whose repository version differs.
 * @param db The local index.
 * @param argc The number of arguments.
 * @param argv The arguments, starting with the command.
 * @return The exit status.
 */
int pkg_cmd_upgrade(sqlite3* db, int argc, char** argv) {

    pkg_entries_t names = {0};
    sqlite3_stmt* stmt  = NULL;
    int r               = 0;

    if (argc > 1) {

        for (int i = 1; i < argc; i++) {

            if ((stmt = pkg_db_prepare(db, "SELECT 1 FROM installed WHERE name = ?", "s", argv[i])) && sqlite3_step(stmt) == SQLITE_ROW) {
                pkg_entries_add(&names, argv[i], 'f');
            } else {
                pkg_error("%s: not installed", argv[i]);
                r = 1;
            }

            sqlite3_finalize(stmt);
        }

    } else if ((stmt = pkg_db_prepare(db, "SELECT i.name FROM installed i JOIN candidates c ON c.name = i.name WHERE c.version != i.version ORDER BY i.name", NULL))) {

        while (sqlite3_step(stmt) == SQLITE_ROW)
            pkg_entries_add(&names, (const char*)sqlite3_column_text(stmt, 0), 'f');

        sqlite3_finalize(stmt);

    } else {
        r = 1;
    }


    pkg_plan_t plan = {0};

    for (size_t i = 0; r == 0 && i < names.count; i++)
        r = pkg_plan_visit(db, &plan, names.items[i].path, NULL, PKG_WANT_UPGRADE) < 0 ? 1 : 0;

    if (r == 0 && plan.count == 0)
        printf("every package is up to date\n");
    else if (r == 0)
        r = pkg_plan_apply(db, &plan);

    pkg_plan_free(&plan);
    pkg_entries_free(&names);

    return r;
}
