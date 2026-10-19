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
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pkg.h"


static bool pkg_db_locked;


/**
 * @brief Opens the local index, creating its tables when it is writable.
 * @param writable Whether the command changes the index.
 * @return The connection, or NULL on error.
 */
sqlite3* pkg_db_open(bool writable) {

    if (writable && pkg_mkdirs(PKG_DB_DIR, 0755) < 0)
        return NULL;

    if (!writable && access(PKG_DB_PATH, F_OK) < 0) {
        pkg_error("%s: no local index, run 'pkg update' first", PKG_DB_PATH);
        return NULL;
    }


    sqlite3* db = NULL;

    int flags = writable ? SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE : SQLITE_OPEN_READONLY;

    if (sqlite3_open_v2(PKG_DB_PATH, &db, flags, NULL) != SQLITE_OK) {
        pkg_error("%s: %s", PKG_DB_PATH, db ? sqlite3_errmsg(db) : "out of memory");
        sqlite3_close(db);
        return NULL;
    }

    sqlite3_busy_timeout(db, PKG_DB_BUSY_MS);

    char* err = NULL;

    if (sqlite3_exec(db, writable ? pkg_schema : "PRAGMA foreign_keys = ON;", NULL, NULL, &err) != SQLITE_OK) {
        pkg_error("%s: %s", PKG_DB_PATH, err ? err : sqlite3_errmsg(db));
        sqlite3_free(err);
        sqlite3_close(db);
        return NULL;
    }

    return db;
}


/**
 * @brief Reports the last error of the local index, with the system error behind an I/O failure.
 * @param db The connection.
 */
void pkg_db_error(sqlite3* db) {

    int errnum = sqlite3_system_errno(db);

    if ((sqlite3_errcode(db) & 0xff) == SQLITE_IOERR && errnum)
        pkg_error("database: %s (%s)", sqlite3_errmsg(db), strerror(errnum));
    else
        pkg_error("database: %s", sqlite3_errmsg(db));
}


/**
 * @brief Closes the local index.
 * @param db The connection, or NULL.
 */
void pkg_db_close(sqlite3* db) {
    sqlite3_close(db);
}


/**
 * @brief Reads the pid recorded in the lock file.
 * @return The pid, or 0 when the file holds none.
 */
static pid_t pkg_db_lock_owner(void) {

    FILE* fp = fopen(PKG_LOCK_PATH, "r");

    if (!fp)
        return 0;

    int pid = 0;

    if (fscanf(fp, "%d", &pid) != 1)
        pid = 0;

    fclose(fp);

    return pid > 0 ? pid : 0;
}


/**
 * @brief Takes the lock that serialises the commands changing the system, clearing one left by a dead process.
 * @return 0 on success, -1 when another pkg holds it.
 */
int pkg_db_lock(void) {

    if (pkg_mkdirs(PKG_DB_DIR, 0755) < 0)
        return -1;

    for (int attempt = 0; attempt < 2; attempt++) {

        int fd = open(PKG_LOCK_PATH, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);

        if (fd >= 0) {

            dprintf(fd, "%d\n", getpid());
            close(fd);

            pkg_db_locked = true;
            return 0;
        }

        if (errno != EEXIST) {
            pkg_error("%s: %s", PKG_LOCK_PATH, strerror(errno));
            return -1;
        }

        pid_t owner = pkg_db_lock_owner();

        if (owner == 0 || kill(owner, 0) == 0 || errno != ESRCH)
            break;

        unlink(PKG_LOCK_PATH);
    }

    pkg_error("another pkg is running, otherwise remove %s", PKG_LOCK_PATH);
    return -1;
}


/**
 * @brief Releases the lock taken by pkg_db_lock(), if any.
 */
void pkg_db_unlock(void) {

    if (pkg_db_locked)
        unlink(PKG_LOCK_PATH);

    pkg_db_locked = false;
}


/**
 * @brief Prepares a statement and binds its parameters.
 * @param db The connection.
 * @param sql The statement.
 * @param fmt One character per parameter: 's' a string (NULL binds NULL), 'i' an int, 'l' a long long.
 * @param ap The parameters.
 * @return The statement, or NULL on error.
 */
static sqlite3_stmt* pkg_db_vprepare(sqlite3* db, const char* sql, const char* fmt, va_list ap) {

    sqlite3_stmt* stmt = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pkg_db_error(db);
        return NULL;
    }

    for (int i = 0; fmt && fmt[i]; i++) {

        int rc;

        switch (fmt[i]) {

            case 's': {
                const char* s = va_arg(ap, const char*);
                rc            = s ? sqlite3_bind_text(stmt, i + 1, s, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, i + 1);
                break;
            }

            case 'i':
                rc = sqlite3_bind_int(stmt, i + 1, va_arg(ap, int));
                break;

            case 'l':
                rc = sqlite3_bind_int64(stmt, i + 1, va_arg(ap, long long));
                break;

            default:
                rc = SQLITE_MISUSE;
                break;
        }

        if (rc != SQLITE_OK) {
            pkg_error("database: %s", sqlite3_errstr(rc));
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    return stmt;
}


/**
 * @brief Prepares a statement and binds its parameters.
 * @param db The connection.
 * @param sql The statement.
 * @param fmt One character per parameter, see pkg_db_vprepare().
 * @return The statement, or NULL on error.
 */
sqlite3_stmt* pkg_db_prepare(sqlite3* db, const char* sql, const char* fmt, ...) {

    va_list ap;
    va_start(ap, fmt);

    sqlite3_stmt* stmt = pkg_db_vprepare(db, sql, fmt, ap);

    va_end(ap);

    return stmt;
}


/**
 * @brief Runs a statement to completion, discarding its rows.
 * @param db The connection.
 * @param sql The statement.
 * @param fmt One character per parameter, see pkg_db_vprepare().
 * @return 0 on success, -1 on error.
 */
int pkg_db_run(sqlite3* db, const char* sql, const char* fmt, ...) {

    va_list ap;
    va_start(ap, fmt);

    sqlite3_stmt* stmt = pkg_db_vprepare(db, sql, fmt, ap);

    va_end(ap);

    if (!stmt)
        return -1;

    int rc;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        ;

    if (rc != SQLITE_DONE)
        pkg_db_error(db);

    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE ? 0 : -1;
}


/**
 * @brief Starts a transaction.
 * @param db The connection.
 * @return 0 on success, -1 on error.
 */
int pkg_db_begin(sqlite3* db) {
    return pkg_db_run(db, "BEGIN", NULL);
}


/**
 * @brief Commits the current transaction.
 * @param db The connection.
 * @return 0 on success, -1 on error.
 */
int pkg_db_commit(sqlite3* db) {
    return pkg_db_run(db, "COMMIT", NULL);
}


/**
 * @brief Rolls back the current transaction, if any.
 * @param db The connection.
 */
void pkg_db_rollback(sqlite3* db) {

    if (!sqlite3_get_autocommit(db))
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
}
