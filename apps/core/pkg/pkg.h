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

#ifndef _PKG_H
#define _PKG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <sqlite3.h>

#define PKG_NAME    "pkg"
#define PKG_VERSION "1.0.0"

#define PKG_CONF_PATH  "/etc/pkg.conf"
#define PKG_DB_DIR     "/var/lib/pkg"
#define PKG_DB_PATH    "/var/lib/pkg/pkg.db"
#define PKG_LOCK_PATH  "/var/lib/pkg/lock"
#define PKG_CACHE_DIR  "/var/cache/pkg"
#define PKG_SCRIPT_DIR "/tmp"

#define PKG_SHA256_LEN 64
#define PKG_DB_BUSY_MS 10000
#define PKG_PATH_MAX   4096

/** @brief A repository declared by a section of pkg.conf. */
typedef struct pkg_repo {
    char* name;
    char* url;
    struct pkg_repo* next;
} pkg_repo_t;

/** @brief A path a package owns: 'f' for a file, 'l' for a link, 'd' for a directory. */
typedef struct pkg_entry {
    char* path;
    char type;
} pkg_entry_t;

/** @brief A growable list of package entries. */
typedef struct pkg_entries {
    pkg_entry_t* items;
    size_t count;
    size_t capacity;
} pkg_entries_t;

/** @brief Options shared by every command. */
typedef struct pkg_options {
    bool yes;
    bool force;
} pkg_options_t;

extern pkg_options_t pkg_options;
extern const char pkg_schema[];


void pkg_error(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void pkg_warn(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
char* pkg_strdup(const char* s);
char* pkg_format(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
bool pkg_confirm(const char* question);
int pkg_mkdirs(const char* path, mode_t mode);
int pkg_copy_file(const char* src, const char* dst, mode_t mode);
int pkg_run_script(const char* path);

pkg_repo_t* pkg_conf_load(const char* path);
void pkg_conf_free(pkg_repo_t* repos);

sqlite3* pkg_db_open(bool writable);
void pkg_db_close(sqlite3* db);
void pkg_db_error(sqlite3* db);
int pkg_db_lock(void);
void pkg_db_unlock(void);
sqlite3_stmt* pkg_db_prepare(sqlite3* db, const char* sql, const char* fmt, ...);
int pkg_db_run(sqlite3* db, const char* sql, const char* fmt, ...);
int pkg_db_begin(sqlite3* db);
int pkg_db_commit(sqlite3* db);
void pkg_db_rollback(sqlite3* db);

char* pkg_fetch_mem(const char* url, size_t* size);
int pkg_fetch_file(const char* url, const char* path, const char* label);
int pkg_sha256_file(const char* path, char digest[PKG_SHA256_LEN + 1]);

void pkg_entries_add(pkg_entries_t* entries, const char* path, char type);
void pkg_entries_free(pkg_entries_t* entries);
int pkg_entries_load(sqlite3* db, const char* package, pkg_entries_t* entries);
void pkg_entries_purge(sqlite3* db, pkg_entries_t* entries);

int pkg_archive_extract(const char* archive, const char* package, pkg_entries_t* entries);

int pkg_cmd_update(sqlite3* db, int argc, char** argv);
int pkg_cmd_install(sqlite3* db, int argc, char** argv);
int pkg_cmd_upgrade(sqlite3* db, int argc, char** argv);
int pkg_cmd_remove(sqlite3* db, int argc, char** argv);
int pkg_cmd_autoremove(sqlite3* db, int argc, char** argv);
int pkg_cmd_search(sqlite3* db, int argc, char** argv);
int pkg_cmd_info(sqlite3* db, int argc, char** argv);
int pkg_cmd_list(sqlite3* db, int argc, char** argv);
int pkg_cmd_files(sqlite3* db, int argc, char** argv);
int pkg_cmd_clean(sqlite3* db, int argc, char** argv);

#endif
