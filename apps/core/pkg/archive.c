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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <lzma.h>

#include "pkg.h"


#define PKG_TAR_BLOCK    512
#define PKG_TAR_META_MAX (1024 * 1024)
#define PKG_ARCHIVE_BUF  65536


/** @brief A tar header block, POSIX ustar layout. */
typedef struct pkg_tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} pkg_tar_header_t;

_Static_assert(sizeof(pkg_tar_header_t) == PKG_TAR_BLOCK, "tar header must be one block");

/** @brief An xz-compressed stream being decoded. */
typedef struct pkg_archive {
    const char* path;
    int fd;
    bool eof;
    bool end;
    lzma_stream lzma;
    uint8_t* input;
    uint8_t* data;
} pkg_archive_t;

/** @brief A link created once every file of the archive is in place. */
typedef struct pkg_link {
    char* path;
    char* target;
    bool hard;
    int error;
} pkg_link_t;

/** @brief The state of one extraction. */
typedef struct pkg_extract {
    pkg_archive_t archive;
    const char* package;
    pkg_entries_t* entries;
    pkg_link_t* links;
    size_t nlinks;
    size_t links_capacity;
    char* longname;
    char* longlink;
    char* post_install;
} pkg_extract_t;


/**
 * @brief Describes a liblzma error.
 * @param rc The error.
 * @return The description.
 */
static const char* pkg_archive_strerror(lzma_ret rc) {

    switch (rc) {
        case LZMA_MEM_ERROR:
            return "out of memory";
        case LZMA_FORMAT_ERROR:
            return "not an xz archive";
        case LZMA_OPTIONS_ERROR:
            return "unsupported xz options";
        case LZMA_DATA_ERROR:
            return "corrupted archive";
        case LZMA_BUF_ERROR:
            return "truncated archive";
        default:
            return "decompression failed";
    }
}


/**
 * @brief Opens an xz archive for decoding.
 * @param ar The stream.
 * @param path The archive.
 * @return 0 on success, -1 on error.
 */
static int pkg_archive_open(pkg_archive_t* ar, const char* path) {

    memset(ar, 0, sizeof(*ar));

    ar->path = path;
    ar->fd   = open(path, O_RDONLY | O_CLOEXEC);

    if (ar->fd < 0) {
        pkg_error("%s: %s", path, strerror(errno));
        return -1;
    }

    ar->input = malloc(PKG_ARCHIVE_BUF);
    ar->data  = malloc(PKG_ARCHIVE_BUF);

    if (!ar->input || !ar->data) {
        pkg_error("out of memory");
        exit(1);
    }

    lzma_ret rc = lzma_stream_decoder(&ar->lzma, UINT64_MAX, LZMA_CONCATENATED);

    if (rc != LZMA_OK) {
        pkg_error("%s: %s", path, pkg_archive_strerror(rc));
        close(ar->fd);
        free(ar->input);
        free(ar->data);
        return -1;
    }

    return 0;
}


/**
 * @brief Closes a stream opened by pkg_archive_open().
 * @param ar The stream.
 */
static void pkg_archive_close(pkg_archive_t* ar) {

    lzma_end(&ar->lzma);
    close(ar->fd);

    free(ar->input);
    free(ar->data);
}


/**
 * @brief Decodes up to size bytes, fewer only when the stream ends.
 * @param ar The stream.
 * @param buf Where to store them.
 * @param size How many to decode.
 * @return The bytes decoded, or -1 on error.
 */
static ssize_t pkg_archive_read(pkg_archive_t* ar, void* buf, size_t size) {

    if (ar->end)
        return 0;

    ar->lzma.next_out  = buf;
    ar->lzma.avail_out = size;

    while (ar->lzma.avail_out > 0) {

        if (ar->lzma.avail_in == 0 && !ar->eof) {

            ssize_t n = read(ar->fd, ar->input, PKG_ARCHIVE_BUF);

            if (n < 0) {

                if (errno == EINTR)
                    continue;

                pkg_error("%s: %s", ar->path, strerror(errno));
                return -1;
            }

            ar->eof           = n == 0;
            ar->lzma.next_in  = ar->input;
            ar->lzma.avail_in = (size_t)n;
        }

        lzma_ret rc = lzma_code(&ar->lzma, ar->eof ? LZMA_FINISH : LZMA_RUN);

        if (rc == LZMA_STREAM_END) {
            ar->end = true;
            break;
        }

        if (rc != LZMA_OK) {
            pkg_error("%s: %s", ar->path, pkg_archive_strerror(rc));
            return -1;
        }
    }

    return (ssize_t)(size - ar->lzma.avail_out);
}


/**
 * @brief Decodes exactly size bytes.
 * @param ar The stream.
 * @param buf Where to store them.
 * @param size How many to decode.
 * @return 0 on success, -1 on error or when the stream ends first.
 */
static int pkg_archive_need(pkg_archive_t* ar, void* buf, size_t size) {

    ssize_t n = pkg_archive_read(ar, buf, size);

    if (n < 0)
        return -1;

    if ((size_t)n != size) {
        pkg_error("%s: truncated archive", ar->path);
        return -1;
    }

    return 0;
}


/**
 * @brief Decodes and discards bytes.
 * @param ar The stream.
 * @param size How many to discard.
 * @return 0 on success, -1 on error.
 */
static int pkg_archive_skip(pkg_archive_t* ar, uint64_t size) {

    while (size > 0) {

        size_t chunk = size < PKG_ARCHIVE_BUF ? (size_t)size : PKG_ARCHIVE_BUF;

        if (pkg_archive_need(ar, ar->data, chunk) < 0)
            return -1;

        size -= chunk;
    }

    return 0;
}


/**
 * @brief Parses a numeric header field, octal or GNU base-256.
 * @param field The field.
 * @param len Its size.
 * @param value Where to store the number.
 * @return True when the field is well formed.
 */
static bool pkg_tar_number(const char* field, size_t len, uint64_t* value) {

    uint64_t v = 0;

    if ((unsigned char)field[0] & 0x80) {

        v = (unsigned char)field[0] & 0x7f;

        for (size_t i = 1; i < len; i++) {

            if (v >> 56)
                return false;

            v = (v << 8) | (unsigned char)field[i];
        }

        *value = v;
        return true;
    }

    size_t i = 0;

    while (i < len && field[i] == ' ')
        i++;

    for (; i < len && field[i] >= '0' && field[i] <= '7'; i++)
        v = (v << 3) | (uint64_t)(field[i] - '0');

    if (i < len && field[i] != ' ' && field[i] != '\0')
        return false;

    *value = v;
    return true;
}


/**
 * @brief Verifies the checksum of a header.
 * @param h The header.
 * @return True when it matches.
 */
static bool pkg_tar_checksum(const pkg_tar_header_t* h) {

    uint64_t expected;

    if (!pkg_tar_number(h->chksum, sizeof(h->chksum), &expected))
        return false;

    const unsigned char* p = (const unsigned char*)h;
    uint64_t sum           = 0;

    for (size_t i = 0; i < PKG_TAR_BLOCK; i++) {

        if (i >= offsetof(pkg_tar_header_t, chksum) && i < offsetof(pkg_tar_header_t, chksum) + sizeof(h->chksum))
            sum += ' ';
        else
            sum += p[i];
    }

    return sum == expected;
}


/**
 * @brief Tells whether a block is all zeroes, which ends the archive.
 * @param h The block.
 * @return True for an empty block.
 */
static bool pkg_tar_zero(const pkg_tar_header_t* h) {

    const unsigned char* p = (const unsigned char*)h;

    for (size_t i = 0; i < PKG_TAR_BLOCK; i++) {

        if (p[i])
            return false;
    }

    return true;
}


/**
 * @brief Copies a header string field, which may lack its terminator.
 * @param field The field.
 * @param len Its size.
 * @return The string.
 */
static char* pkg_tar_string(const char* field, size_t len) {
    return pkg_format("%.*s", (int)strnlen(field, len), field);
}


/**
 * @brief Builds the member name, joining the ustar prefix when there is one.
 * @param h The header.
 * @return The name.
 */
static char* pkg_tar_name(const pkg_tar_header_t* h) {

    if (memcmp(h->magic, "ustar", 6) == 0 && h->prefix[0]) {
        return pkg_format("%.*s/%.*s", (int)strnlen(h->prefix, sizeof(h->prefix)), h->prefix, (int)strnlen(h->name, sizeof(h->name)), h->name);
    }

    return pkg_tar_string(h->name, sizeof(h->name));
}


/**
 * @brief Normalises a member name into a path relative to the root, dropping '.' components.
 * @param name The member name.
 * @return The path, "" for the root itself, or NULL when it climbs out with '..'.
 */
static char* pkg_tar_sanitize(const char* name) {

    char* out  = pkg_strdup(name);
    size_t len = 0;

    for (const char* p = name; *p;) {

        while (*p == '/')
            p++;

        const char* end = strchrnul(p, '/');
        size_t n        = (size_t)(end - p);

        if (n == 0)
            break;

        if (n == 2 && p[0] == '.' && p[1] == '.') {
            free(out);
            return NULL;
        }

        if (!(n == 1 && p[0] == '.')) {

            if (len)
                out[len++] = '/';

            memcpy(&out[len], p, n);
            len += n;
        }

        p = end;
    }

    out[len] = '\0';
    return out;
}


/**
 * @brief Decodes the data of a member into a new file.
 * @param ex The extraction.
 * @param path The file, which must not exist.
 * @param mode Its mode.
 * @param size The size of the data.
 * @return 0 on success, -1 on error.
 */
static int pkg_extract_write(pkg_extract_t* ex, const char* path, mode_t mode, uint64_t size) {

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);

    if (fd < 0 && errno == ENOENT) {

        char* dir   = pkg_strdup(path);
        char* slash = strrchr(dir, '/');

        if (slash && slash != dir) {
            *slash = '\0';
            pkg_mkdirs(dir, 0755);
        }

        free(dir);

        fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    }

    if (fd < 0) {
        pkg_error("%s: %s", path, strerror(errno));
        return -1;
    }

    while (size > 0) {

        size_t chunk = size < PKG_ARCHIVE_BUF ? (size_t)size : PKG_ARCHIVE_BUF;

        if (pkg_archive_need(&ex->archive, ex->archive.data, chunk) < 0) {
            close(fd);
            return -1;
        }

        if (write(fd, ex->archive.data, chunk) != (ssize_t)chunk) {
            pkg_error("%s: %s", path, errno ? strerror(errno) : "short write");
            close(fd);
            return -1;
        }

        size -= chunk;
    }

    if (fchmod(fd, mode & 07777) < 0)
        pkg_warn("%s: %s", path, strerror(errno));

    if (close(fd) < 0) {
        pkg_error("%s: %s", path, strerror(errno));
        return -1;
    }

    return 0;
}


/**
 * @brief Extracts a regular file, replacing whatever file or link is in its way.
 * @param ex The extraction.
 * @param path The absolute path.
 * @param mode Its mode.
 * @param size The size of the data.
 * @return 0 on success or when the file is skipped, -1 on error.
 */
static int pkg_extract_file(pkg_extract_t* ex, const char* path, mode_t mode, uint64_t size) {

    struct stat st;

    if (lstat(path, &st) == 0) {

        if (S_ISDIR(st.st_mode)) {
            pkg_warn("%s: %s is a directory, not replaced", ex->package, path);
            return pkg_archive_skip(&ex->archive, size);
        }

        if (unlink(path) < 0) {
            pkg_warn("%s: cannot replace %s: %s", ex->package, path, strerror(errno));
            return pkg_archive_skip(&ex->archive, size);
        }
    }

    if (pkg_extract_write(ex, path, mode, size) < 0)
        return -1;

    pkg_entries_add(ex->entries, path, 'f');
    return 0;
}


/**
 * @brief Creates a directory, keeping an existing one as it is.
 * @param ex The extraction.
 * @param path The absolute path.
 * @param mode Its mode.
 * @return 0 on success or when it is skipped, -1 on error.
 */
static int pkg_extract_dir(pkg_extract_t* ex, const char* path, mode_t mode) {

    struct stat st;

    if (stat(path, &st) == 0) {

        if (!S_ISDIR(st.st_mode)) {
            pkg_warn("%s: %s exists and is not a directory", ex->package, path);
            return 0;
        }

    } else if (pkg_mkdirs(path, mode & 07777) < 0) {
        return -1;
    }

    pkg_entries_add(ex->entries, path, 'd');
    return 0;
}


/**
 * @brief Queues a link for when every file is in place.
 * @param ex The extraction.
 * @param path The absolute path of the link.
 * @param target The symlink target, or the absolute path a hard link points to.
 * @param hard Whether it is a hard link.
 */
static void pkg_extract_defer(pkg_extract_t* ex, const char* path, const char* target, bool hard) {

    if (ex->nlinks == ex->links_capacity) {

        size_t capacity   = ex->links_capacity ? ex->links_capacity * 2 : 32;
        pkg_link_t* links = realloc(ex->links, capacity * sizeof(*links));

        if (!links) {
            pkg_error("out of memory");
            exit(1);
        }

        ex->links          = links;
        ex->links_capacity = capacity;
    }

    ex->links[ex->nlinks++] = (pkg_link_t){
        .path   = pkg_strdup(path),
        .target = pkg_strdup(target),
        .hard   = hard,
        .error  = EAGAIN,
    };
}


/**
 * @brief Creates a queued link, copying its target when the kernel cannot link.
 * @param ex The extraction.
 * @param ln The link.
 * @return 0 on success, otherwise the errno of the failure.
 */
static int pkg_extract_link(pkg_extract_t* ex, const pkg_link_t* ln) {

    struct stat st;

    if (lstat(ln->path, &st) == 0) {

        if (!ln->hard && S_ISLNK(st.st_mode)) {

            char current[PKG_PATH_MAX];
            ssize_t n = readlink(ln->path, current, sizeof(current) - 1);

            if (n >= 0) {

                current[n] = '\0';

                if (strcmp(current, ln->target) == 0) {
                    pkg_entries_add(ex->entries, ln->path, 'l');
                    return 0;
                }
            }
        }

        if (S_ISDIR(st.st_mode))
            return EISDIR;

        if (unlink(ln->path) < 0)
            return errno;
    }


    char* dir   = pkg_strdup(ln->path);
    char* slash = strrchr(dir, '/');

    if (slash)
        slash[slash == dir ? 1 : 0] = '\0';

    pkg_mkdirs(dir, 0755);


    int e = (ln->hard ? link(ln->target, ln->path) : symlink(ln->target, ln->path)) == 0 ? 0 : errno;

    if (e) {

        char* source = ln->hard || ln->target[0] == '/' ? pkg_strdup(ln->target) : pkg_format("%s/%s", dir, ln->target);

        if (stat(source, &st) == 0) {
            e = S_ISREG(st.st_mode) ? pkg_copy_file(source, ln->path, st.st_mode) : e;
        } else {
            e = errno;
        }

        free(source);
    }

    free(dir);

    if (e == 0)
        pkg_entries_add(ex->entries, ln->path, 'l');

    return e;
}


/**
 * @brief Creates the queued links, retrying while links that point to other links resolve.
 * @param ex The extraction.
 */
static void pkg_extract_links(pkg_extract_t* ex) {

    bool progress = true;

    while (progress) {

        progress = false;

        for (size_t i = 0; i < ex->nlinks; i++) {

            if (ex->links[i].error == 0)
                continue;

            if ((ex->links[i].error = pkg_extract_link(ex, &ex->links[i])) == 0)
                progress = true;
        }
    }

    for (size_t i = 0; i < ex->nlinks; i++) {

        if (ex->links[i].error)
            pkg_warn("%s: cannot create %s -> %s: %s", ex->package, ex->links[i].path, ex->links[i].target, strerror(ex->links[i].error));
    }
}


/**
 * @brief Reads the data of a metadata member into a string.
 * @param ex The extraction.
 * @param size The size of the data.
 * @param out Where to store the string, replacing the previous one.
 * @return 0 on success, -1 on error.
 */
static int pkg_extract_string(pkg_extract_t* ex, uint64_t size, char** out) {

    if (size > PKG_TAR_META_MAX) {
        pkg_error("%s: oversized tar metadata", ex->archive.path);
        return -1;
    }

    char* s = malloc((size_t)size + 1);

    if (!s) {
        pkg_error("out of memory");
        exit(1);
    }

    if (pkg_archive_need(&ex->archive, s, (size_t)size) < 0) {
        free(s);
        return -1;
    }

    s[size] = '\0';

    free(*out);
    *out = s;

    return 0;
}


/**
 * @brief Reads a pax extended header, keeping the path and linkpath records for the next member.
 * @param ex The extraction.
 * @param size The size of the data.
 * @return 0 on success, -1 on error.
 */
static int pkg_extract_pax(pkg_extract_t* ex, uint64_t size) {

    char* data = NULL;

    if (pkg_extract_string(ex, size, &data) < 0)
        return -1;

    char* p   = data;
    char* end = data + size;

    while (p < end) {

        char* sp          = NULL;
        unsigned long len = strtoul(p, &sp, 10);

        if (sp == p || *sp != ' ' || len == 0 || len > (unsigned long)(end - p) || p[len - 1] != '\n')
            break;

        p[len - 1] = '\0';

        char* record = sp + 1;
        char* eq     = strchr(record, '=');

        if (eq) {

            *eq = '\0';

            if (strcmp(record, "path") == 0) {
                free(ex->longname);
                ex->longname = pkg_strdup(eq + 1);
            } else if (strcmp(record, "linkpath") == 0) {
                free(ex->longlink);
                ex->longlink = pkg_strdup(eq + 1);
            }
        }

        p += len;
    }

    free(data);
    return 0;
}


/**
 * @brief Handles a member of the .pkg directory: the pre-install script runs at once, the post-install one is kept for the end.
 * @param ex The extraction.
 * @param rel The member path.
 * @param size The size of the data.
 * @return 0 on success, -1 on error.
 */
static int pkg_extract_meta(pkg_extract_t* ex, const char* rel, uint64_t size) {

    bool pre  = strcmp(rel, ".pkg/pre-install") == 0;
    bool post = strcmp(rel, ".pkg/post-install") == 0;

    if (!pre && !post)
        return pkg_archive_skip(&ex->archive, size);

    char* script = pkg_format("%s/pkg-%s-%d.%s", PKG_SCRIPT_DIR, ex->package, (int)getpid(), rel + 5);

    unlink(script);

    if (pkg_extract_write(ex, script, 0755, size) < 0) {
        free(script);
        return -1;
    }

    if (post) {
        free(ex->post_install);
        ex->post_install = script;
        return 0;
    }

    if (pkg_run_script(script) < 0)
        pkg_warn("%s: pre-install script failed", ex->package);

    unlink(script);
    free(script);

    return 0;
}


/**
 * @brief Extracts one member, consuming its data but not its padding.
 * @param ex The extraction.
 * @param h Its header.
 * @param size The size of its data.
 * @return 0 on success or when it is skipped, -1 on error.
 */
static int pkg_extract_member(pkg_extract_t* ex, const pkg_tar_header_t* h, uint64_t size) {

    uint64_t mode;

    if (!pkg_tar_number(h->mode, sizeof(h->mode), &mode)) {
        pkg_error("%s: corrupted tar header", ex->archive.path);
        return -1;
    }

    switch (h->typeflag) {
        case 'L':
            return pkg_extract_string(ex, size, &ex->longname);
        case 'K':
            return pkg_extract_string(ex, size, &ex->longlink);
        case 'x':
            return pkg_extract_pax(ex, size);
        case 'g':
            return pkg_archive_skip(&ex->archive, size);
    }


    char* name = ex->longname ? ex->longname : pkg_tar_name(h);
    char* link = ex->longlink ? ex->longlink : pkg_tar_string(h->linkname, sizeof(h->linkname));

    ex->longname = NULL;
    ex->longlink = NULL;

    char* rel  = pkg_tar_sanitize(name);
    char* path = rel ? pkg_format("/%s", rel) : NULL;
    int r      = 0;

    if (!rel) {

        pkg_warn("%s: skipping unsafe path %s", ex->package, name);
        r = pkg_archive_skip(&ex->archive, size);

    } else if (rel[0] == '\0') {

        r = pkg_archive_skip(&ex->archive, size);

    } else if (strcmp(rel, ".pkg") == 0 || strncmp(rel, ".pkg/", 5) == 0) {

        r = pkg_extract_meta(ex, rel, size);

    } else {

        switch (h->typeflag) {

            case '0':
            case '\0':
            case '7':
                r = pkg_extract_file(ex, path, (mode_t)mode, size);
                break;

            case '5':
                if ((r = pkg_extract_dir(ex, path, (mode_t)mode)) == 0)
                    r = pkg_archive_skip(&ex->archive, size);
                break;

            case '2':
                pkg_extract_defer(ex, path, link, false);
                r = pkg_archive_skip(&ex->archive, size);
                break;

            case '1': {

                char* target = pkg_tar_sanitize(link);

                if (target && target[0]) {
                    char* abs = pkg_format("/%s", target);
                    pkg_extract_defer(ex, path, abs, true);
                    free(abs);
                } else {
                    pkg_warn("%s: skipping %s, unsafe hard link to %s", ex->package, path, link);
                }

                free(target);
                r = pkg_archive_skip(&ex->archive, size);
                break;
            }

            default:
                pkg_warn("%s: skipping %s, unsupported tar type '%c'", ex->package, path, h->typeflag);
                r = pkg_archive_skip(&ex->archive, size);
                break;
        }
    }

    free(path);
    free(rel);
    free(name);
    free(link);

    return r;
}


/**
 * @brief Extracts a package archive over the root, running its scripts and recording what it installs.
 * @param archive The .tar.xz archive.
 * @param package The package name, for messages and script names.
 * @param entries Where to append the paths installed.
 * @return 0 on success, -1 on error.
 */
int pkg_archive_extract(const char* archive, const char* package, pkg_entries_t* entries) {

    pkg_extract_t ex = {
        .package = package,
        .entries = entries,
    };

    if (pkg_archive_open(&ex.archive, archive) < 0)
        return -1;


    int r = 0;

    for (;;) {

        pkg_tar_header_t h;
        ssize_t n = pkg_archive_read(&ex.archive, &h, sizeof(h));

        if (n <= 0) {
            r = (int)n;
            break;
        }

        if ((size_t)n != sizeof(h)) {
            pkg_error("%s: truncated archive", archive);
            r = -1;
            break;
        }

        if (pkg_tar_zero(&h))
            break;

        uint64_t size;

        if (!pkg_tar_checksum(&h) || !pkg_tar_number(h.size, sizeof(h.size), &size)) {
            pkg_error("%s: corrupted tar header", archive);
            r = -1;
            break;
        }

        if ((r = pkg_extract_member(&ex, &h, size)) < 0)
            break;

        if ((r = pkg_archive_skip(&ex.archive, (PKG_TAR_BLOCK - size % PKG_TAR_BLOCK) % PKG_TAR_BLOCK)) < 0)
            break;
    }

    if (r == 0) {

        pkg_extract_links(&ex);

        if (ex.post_install && pkg_run_script(ex.post_install) < 0)
            pkg_warn("%s: post-install script failed", package);
    }

    if (ex.post_install)
        unlink(ex.post_install);

    for (size_t i = 0; i < ex.nlinks; i++) {
        free(ex.links[i].path);
        free(ex.links[i].target);
    }

    free(ex.links);
    free(ex.longname);
    free(ex.longlink);
    free(ex.post_install);

    pkg_archive_close(&ex.archive);

    return r;
}
