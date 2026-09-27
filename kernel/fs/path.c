/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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

#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>



/**
 * @brief How many symbolic links one lookup may follow, and how deeply resolving a link's target may nest.
 */
#define PATH_MAXSYMLINKS 40
#define PATH_MAXNESTED   8


static inode_t* __path_lookup(inode_t* cwd, const char* path, int flags, mode_t mode, int* links, int depth);


/**
 * @brief Tells whether a lookup failed for a reason of its own rather than a missing name, which is reported as ENOENT.
 *
 * @return true if errno already holds the reason.
 */
static inline bool __path_failed_on_its_own(void) {
    return errno == ELOOP || errno == ENAMETOOLONG || errno == ENOMEM;
}


/**
 * @brief Follows a link, and each link it lands on, until it reaches an inode that is not a link.
 *
 * @param inode The inode to start from, whose reference the call takes over.
 * @param links The links the whole lookup may still follow, shared by every nesting level.
 * @param depth How deeply this resolution is nested inside the resolution of another link.
 * @return The inode the links lead to, referenced, or NULL with errno set.
 */
static inode_t* __path_follow_links(inode_t* inode, int* links, int depth) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(links);


    char* target = NULL;

    while (inode) {

        struct stat st = {0};

        if (vfs_getattr(inode, &st) < 0) {
            vfs_inode_put(inode);
            inode = NULL;
            break;
        }

        if (!S_ISLNK(st.st_mode))
            break;

        if (--(*links) < 0 || depth >= PATH_MAXNESTED) {
            vfs_inode_put(inode);
            inode = NULL;
            errno = ELOOP;
            break;
        }

        if (!target && (target = kmalloc(CONFIG_PATH_MAX + 1, GFP_KERNEL)) == NULL) {
            vfs_inode_put(inode);
            inode = NULL;
            errno = ENOMEM;
            break;
        }

        ssize_t n = vfs_readlink(inode, target, CONFIG_PATH_MAX);

        if (n <= 0) {

            vfs_inode_put(inode);
            inode = NULL;

            if (n == 0)
                errno = ENOENT;

            break;
        }

        target[n] = '\0';

        inode_t* next = __path_lookup(inode->parent ? inode->parent : inode, target, O_NOFOLLOW, 0, links, depth + 1);

        vfs_inode_put(inode);
        inode = next;
    }

    if (target)
        kfree(target);

    return inode;
}


/**
 * @brief Looks one path component up in a directory, following it if it is a link and @p follow is set.
 *
 * @return The inode, referenced, or NULL with errno set.
 */
static inode_t* path_find(inode_t* inode, const char* path, size_t size, bool follow, int* links, int depth) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(path);
    DEBUG_ASSERT(size);


    if (unlikely(size >= CONFIG_MAXNAMLEN))
        return errno = ENAMETOOLONG, NULL;


    char s[size + 1];
    memset(s, 0, size + 1);

    strncpy(s, path, size);


    if ((inode = vfs_finddir(inode, s)) == NULL)
        return NULL;


    if (!follow)
        return inode;

    return __path_follow_links(inode, links, depth);
}


/**
 * @brief Follows a link the caller holds to the inode it ends at.
 *
 * @param inode The link, whose reference stays with the caller.
 * @return The inode the link leads to, referenced, or NULL with errno set.
 */
inode_t* path_follows(inode_t* inode) {

    DEBUG_ASSERT(inode);

    int links = PATH_MAXSYMLINKS;

    errno = 0;

    return __path_follow_links(vfs_inode_get(inode), &links, 0);
}


/**
 * @brief Resolves a path from a directory, creating the last component when asked to.
 *
 * @return The inode, referenced, or NULL with errno set.
 */
static inode_t* __path_lookup(inode_t* cwd, const char* path, int flags, mode_t mode, int* links, int depth) {

    DEBUG_ASSERT(cwd);
    DEBUG_ASSERT(path);


    inode_t* c = NULL;

    if (path[0] == '/') {

        shared_ptr_access(current_task->fs, fs, { c = vfs_inode_get(fs->root); });

    } else {

        c = vfs_inode_get(cwd);
    }

    DEBUG_ASSERT(c);


    while (path[0] == '/') {
        path++;
    }

    while (c) {

        const char* next = path + strcspn(path, "/");

        while (next[0] == '/')
            next++;

        if (next[0] == '\0')
            break;


        inode_t* found = path_find(c, path, strcspn(path, "/"), true, links, depth);

        vfs_inode_put(c);

        c    = found;
        path = next;
    }


    if (unlikely(!c)) {
        return errno = (__path_failed_on_its_own() ? errno : ENOENT), NULL;
    }


    size_t last = strcspn(path, "/");

    inode_t* r;

    if (last > 0)
        r = path_find(c, path, last, !(flags & O_NOFOLLOW), links, depth);
    else
        r = vfs_inode_get(c);


    if (unlikely(!r)) {

        if ((flags & O_CREAT) && !__path_failed_on_its_own()) {

            if ((mode & S_IFMT) == 0) {
                mode |= S_IFREG;
            }

            char name[last + 1];

            memcpy(name, path, last);
            name[last] = '\0';

            r = vfs_creat(c, name, mode);

        } else {
            errno = (__path_failed_on_its_own() ? errno : ENOENT);
        }

    } else {

        if ((flags & O_EXCL) && (flags & O_CREAT)) {
            vfs_inode_put(r);
            r     = NULL;
            errno = EEXIST;
        }
    }

    vfs_inode_put(c);

    return r;
}


/**
 * @brief Resolves a path from a directory the caller holds, creating the last component when asked to.
 *
 * @param cwd The directory relative paths start from.
 * @param path The path.
 * @param flags O_CREAT, O_EXCL and O_NOFOLLOW as for open().
 * @param mode The type and permissions of a created entry.
 * @return The inode, referenced, or NULL with errno set.
 */
inode_t* path_lookup(inode_t* cwd, const char* path, int flags, mode_t mode) {

    int links = PATH_MAXSYMLINKS;

    errno = 0;

    return __path_lookup(cwd, path, flags, mode, &links, 0);
}
