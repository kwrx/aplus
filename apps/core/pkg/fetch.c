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

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <curl/curl.h>
#include <openssl/sha.h>

#include "pkg.h"


/** @brief A download kept in memory. */
typedef struct pkg_buffer {
    char* data;
    size_t size;
} pkg_buffer_t;

/** @brief The state of the progress line of a download. */
typedef struct pkg_progress {
    const char* label;
    int last;
} pkg_progress_t;


static CURL* pkg_curl;
static char pkg_curl_error[CURL_ERROR_SIZE];


/**
 * @brief Returns the shared curl handle, reset and pointed at a URL.
 * @param url The URL.
 * @return The handle, or NULL when curl cannot start.
 */
static CURL* pkg_fetch_handle(const char* url) {

    if (!pkg_curl) {

        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK || !(pkg_curl = curl_easy_init())) {
            pkg_error("cannot initialise curl");
            return NULL;
        }
    }

    curl_easy_reset(pkg_curl);

    pkg_curl_error[0] = '\0';

    curl_easy_setopt(pkg_curl, CURLOPT_URL, url);
    curl_easy_setopt(pkg_curl, CURLOPT_ERRORBUFFER, pkg_curl_error);
    curl_easy_setopt(pkg_curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(pkg_curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(pkg_curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(pkg_curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(pkg_curl, CURLOPT_USERAGENT, PKG_NAME "/" PKG_VERSION);

    return pkg_curl;
}


/**
 * @brief Reports a failed transfer.
 * @param url The URL.
 * @param rc The curl result.
 */
static void pkg_fetch_failed(const char* url, CURLcode rc) {
    pkg_error("%s: %s", url, pkg_curl_error[0] ? pkg_curl_error : curl_easy_strerror(rc));
}


/**
 * @brief Appends received data to a memory buffer.
 * @param data The received data.
 * @param size The size of an item.
 * @param nmemb The number of items.
 * @param user The pkg_buffer_t.
 * @return The bytes consumed; fewer than offered aborts the transfer.
 */
static size_t pkg_fetch_mem_write(char* data, size_t size, size_t nmemb, void* user) {

    pkg_buffer_t* buffer = user;

    size_t len = size * nmemb;
    char* p    = realloc(buffer->data, buffer->size + len + 1);

    if (!p)
        return 0;

    memcpy(p + buffer->size, data, len);

    buffer->data = p;
    buffer->size += len;
    buffer->data[buffer->size] = '\0';

    return len;
}


/**
 * @brief Downloads a resource into memory.
 * @param url The URL.
 * @param size Where to store its size, or NULL.
 * @return The NUL-terminated content, or NULL on error.
 */
char* pkg_fetch_mem(const char* url, size_t* size) {

    CURL* curl = pkg_fetch_handle(url);

    if (!curl)
        return NULL;

    pkg_buffer_t buffer = {0};

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, pkg_fetch_mem_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

    CURLcode rc = curl_easy_perform(curl);

    if (rc != CURLE_OK) {
        pkg_fetch_failed(url, rc);
        free(buffer.data);
        return NULL;
    }

    if (!buffer.data)
        buffer.data = pkg_strdup("");

    if (size)
        *size = buffer.size;

    return buffer.data;
}


/**
 * @brief Redraws the progress line of a download when its percentage changes.
 * @param user The pkg_progress_t.
 * @param dltotal The expected download size, 0 when unknown.
 * @param dlnow The bytes downloaded so far.
 * @param ultotal Unused.
 * @param ulnow Unused.
 * @return 0, to keep the transfer going.
 */
static int pkg_fetch_progress(void* user, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {

    (void)ultotal;
    (void)ulnow;

    pkg_progress_t* progress = user;

    int percent = dltotal > 0 ? (int)(dlnow * 100 / dltotal) : -1;

    if (percent == progress->last)
        return 0;

    progress->last = percent;

    if (percent >= 0)
        printf("\r  %-40s %9.1f KiB %3d%%", progress->label, dlnow / 1024.0, percent);
    else
        printf("\r  %-40s %9.1f KiB", progress->label, dlnow / 1024.0);

    fflush(stdout);
    return 0;
}


/**
 * @brief Downloads a resource into a file, drawing a progress line on a terminal.
 * @param url The URL.
 * @param path The file, truncated first and deleted on failure.
 * @param label The name shown for the download.
 * @return 0 on success, -1 on error.
 */
int pkg_fetch_file(const char* url, const char* path, const char* label) {

    CURL* curl = pkg_fetch_handle(url);

    if (!curl)
        return -1;

    FILE* fp = fopen(path, "wb");

    if (!fp) {
        pkg_error("%s: %s", path, strerror(errno));
        return -1;
    }


    bool tty                = isatty(STDOUT_FILENO);
    pkg_progress_t progress = {.label = label, .last = -2};

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

    if (tty) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, pkg_fetch_progress);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
    } else {
        printf("  %s\n", label);
    }

    CURLcode rc = curl_easy_perform(curl);

    if (tty)
        printf("\n");

    if (fclose(fp) != 0 && rc == CURLE_OK) {
        pkg_error("%s: %s", path, strerror(errno));
        unlink(path);
        return -1;
    }

    if (rc != CURLE_OK) {
        pkg_fetch_failed(url, rc);
        unlink(path);
        return -1;
    }

    return 0;
}


/**
 * @brief Computes the SHA-256 of a file.
 * @param path The file.
 * @param digest Where to store the digest, in lowercase hex.
 * @return 0 on success, -1 when the file cannot be read, with errno set.
 */
int pkg_sha256_file(const char* path, char digest[PKG_SHA256_LEN + 1]) {

    FILE* fp = fopen(path, "rb");

    if (!fp)
        return -1;

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    unsigned char buf[16384];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        SHA256_Update(&ctx, buf, n);

    int failed = ferror(fp);
    fclose(fp);

    if (failed) {
        errno = EIO;
        return -1;
    }

    unsigned char md[SHA256_DIGEST_LENGTH];
    SHA256_Final(md, &ctx);

    for (size_t i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(&digest[i * 2], "%02x", md[i]);

    return 0;
}


/**
 * @brief Deletes the downloaded archives.
 * @param db The local index, unused.
 * @param argc The number of arguments.
 * @param argv The arguments, starting with the command.
 * @return The exit status.
 */
int pkg_cmd_clean(sqlite3* db, int argc, char** argv) {

    (void)db;
    (void)argv;

    if (argc > 1) {
        pkg_error("clean: takes no arguments");
        return 1;
    }

    DIR* dir = opendir(PKG_CACHE_DIR);

    if (!dir) {

        if (errno == ENOENT) {
            printf("the cache is empty\n");
            return 0;
        }

        pkg_error("%s: %s", PKG_CACHE_DIR, strerror(errno));
        return 1;
    }


    pkg_entries_t archives = {0};
    struct dirent* ent;

    while ((ent = readdir(dir))) {

        if (ent->d_name[0] == '.')
            continue;

        char* path = pkg_format("%s/%s", PKG_CACHE_DIR, ent->d_name);
        pkg_entries_add(&archives, path, 'f');
        free(path);
    }

    closedir(dir);


    size_t count = 0;
    off_t bytes  = 0;
    int r        = 0;

    for (size_t i = 0; i < archives.count; i++) {

        struct stat st;

        if (lstat(archives.items[i].path, &st) < 0 || !S_ISREG(st.st_mode))
            continue;

        if (unlink(archives.items[i].path) < 0) {
            pkg_error("%s: %s", archives.items[i].path, strerror(errno));
            r = 1;
            continue;
        }

        count++;
        bytes += st.st_size;
    }

    pkg_entries_free(&archives);

    printf("removed %zu archives, %.1f MiB\n", count, bytes / (1024.0 * 1024.0));
    return r;
}
