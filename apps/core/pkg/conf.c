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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pkg.h"


/** @brief The keys of the section being parsed. */
typedef struct pkg_conf_section {
    char* name;
    char* repo;
    char* version;
} pkg_conf_section_t;


/**
 * @brief Strips leading and trailing blanks in place.
 * @param s The string.
 * @return The first non-blank character.
 */
static char* pkg_conf_trim(char* s) {

    while (isspace((unsigned char)*s))
        s++;

    char* end = s + strlen(s);

    while (end > s && isspace((unsigned char)end[-1]))
        *--end = '\0';

    return s;
}


/**
 * @brief Strips a pair of matching single or double quotes in place.
 * @param s The value.
 * @return The unquoted value.
 */
static char* pkg_conf_unquote(char* s) {

    size_t len = strlen(s);

    if (len >= 2 && (s[0] == '\'' || s[0] == '"') && s[len - 1] == s[0]) {
        s[len - 1] = '\0';
        return s + 1;
    }

    return s;
}


/**
 * @brief Builds the base URL of a release: a GitHub repository, or any URL followed by the version.
 * @param repo The repo key, "owner/name" or a URL.
 * @param version The repo_version key.
 * @return The URL, without a trailing slash.
 */
static char* pkg_conf_url(const char* repo, const char* version) {

    if (strstr(repo, "://")) {

        size_t len = strlen(repo);

        while (len > 0 && repo[len - 1] == '/')
            len--;

        return pkg_format("%.*s/%s", (int)len, repo, version);
    }

    if (strcmp(version, "latest") == 0)
        return pkg_format("https://github.com/%s/releases/latest/download", repo);

    return pkg_format("https://github.com/%s/releases/download/%s", repo, version);
}


/**
 * @brief Turns the parsed section into a repository and resets it.
 * @param section The section.
 * @param tail Where to link the repository.
 * @param path The configuration file, for messages.
 * @return 0 on success, -1 when the section is incomplete.
 */
static int pkg_conf_flush(pkg_conf_section_t* section, pkg_repo_t*** tail, const char* path) {

    int r = 0;

    if (section->name) {

        if (!section->repo || !section->version) {

            pkg_error("%s: section [%s] needs both repo and repo_version", path, section->name);
            r = -1;

        } else {

            pkg_repo_t* repo = calloc(1, sizeof(*repo));

            if (!repo) {
                pkg_error("out of memory");
                exit(1);
            }

            repo->name = pkg_strdup(section->name);
            repo->url  = pkg_conf_url(section->repo, section->version);

            **tail = repo;
            *tail  = &repo->next;
        }
    }

    free(section->name);
    free(section->repo);
    free(section->version);

    section->name    = NULL;
    section->repo    = NULL;
    section->version = NULL;

    return r;
}


/**
 * @brief Loads the repositories of pkg.conf, in order of priority.
 * @param path The configuration file.
 * @return The repositories, or NULL on error or when there are none.
 */
pkg_repo_t* pkg_conf_load(const char* path) {

    FILE* fp = fopen(path, "r");

    if (!fp) {
        pkg_error("%s: %s", path, strerror(errno));
        return NULL;
    }


    pkg_repo_t* repos          = NULL;
    pkg_repo_t** tail          = &repos;
    pkg_conf_section_t section = {0};

    char line[BUFSIZ];
    size_t lineno = 0;
    int r         = 0;

    while (r == 0 && fgets(line, sizeof(line), fp)) {

        lineno++;
        line[strcspn(line, "\r\n")] = '\0';

        char* p = pkg_conf_trim(line);

        if (*p == '\0' || *p == '#' || *p == ';')
            continue;

        if (*p == '[') {

            char* end = strchr(p, ']');

            if (!end) {
                pkg_error("%s:%zu: unterminated section", path, lineno);
                r = -1;
                break;
            }

            *end = '\0';
            r    = pkg_conf_flush(&section, &tail, path);

            for (pkg_repo_t* repo = repos; r == 0 && repo; repo = repo->next) {

                if (strcmp(repo->name, pkg_conf_trim(p + 1)) == 0) {
                    pkg_error("%s:%zu: section [%s] is declared twice", path, lineno, repo->name);
                    r = -1;
                }
            }

            section.name = pkg_strdup(pkg_conf_trim(p + 1));
            continue;
        }

        char* eq = strchr(p, '=');

        if (!eq) {
            pkg_error("%s:%zu: expected key=value", path, lineno);
            r = -1;
            break;
        }

        *eq = '\0';

        char* key   = pkg_conf_trim(p);
        char* value = pkg_conf_unquote(pkg_conf_trim(eq + 1));

        if (!section.name) {
            pkg_error("%s:%zu: '%s' is outside a section", path, lineno, key);
            r = -1;
        } else if (strcmp(key, "repo") == 0) {
            free(section.repo);
            section.repo = pkg_strdup(value);
        } else if (strcmp(key, "repo_version") == 0) {
            free(section.version);
            section.version = pkg_strdup(value);
        } else {
            pkg_warn("%s:%zu: unknown key '%s'", path, lineno, key);
        }
    }

    if (pkg_conf_flush(&section, &tail, path) < 0)
        r = -1;

    fclose(fp);

    if (r == 0 && !repos) {
        pkg_error("%s: no repository is configured", path);
        r = -1;
    }

    if (r < 0) {
        pkg_conf_free(repos);
        return NULL;
    }

    return repos;
}


/**
 * @brief Frees a list of repositories.
 * @param repos The first repository.
 */
void pkg_conf_free(pkg_repo_t* repos) {

    while (repos) {

        pkg_repo_t* next = repos->next;

        free(repos->name);
        free(repos->url);
        free(repos);

        repos = next;
    }
}
