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
 * @param inode The inode to start from.
 * @param links The links the whole lookup may still follow, shared by every nesting level.
 * @param depth How deeply this resolution is nested inside the resolution of another link.
 * @return The inode the links lead to, or NULL with errno set.
 */
static inode_t* __path_follow_links(inode_t* inode, int* links, int depth) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(links);


    char* target = NULL;

    while (inode) {

        struct stat st = {0};

        if (vfs_getattr(inode, &st) < 0) {
            inode = NULL;
            break;
        }

        if (!S_ISLNK(st.st_mode))
            break;

        if (--(*links) < 0 || depth >= PATH_MAXNESTED) {
            errno = ELOOP;
            inode = NULL;
            break;
        }

        if (!target && (target = kmalloc(CONFIG_PATH_MAX + 1, GFP_KERNEL)) == NULL) {
            errno = ENOMEM;
            inode = NULL;
            break;
        }

        ssize_t n = vfs_readlink(inode, target, CONFIG_PATH_MAX);

        if (n <= 0) {

            if (n == 0)
                errno = ENOENT;

            inode = NULL;
            break;
        }

        target[n] = '\0';

        inode = __path_lookup(inode->parent ? inode->parent : inode, target, O_NOFOLLOW, 0, links, depth + 1);
    }

    if (target)
        kfree(target);

    return inode;
}


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



inode_t* path_follows(inode_t* inode) {

    DEBUG_ASSERT(inode);

    int links = PATH_MAXSYMLINKS;

    errno = 0;

    return __path_follow_links(inode, &links, 0);
}



static inode_t* __path_lookup(inode_t* cwd, const char* path, int flags, mode_t mode, int* links, int depth) {

    DEBUG_ASSERT(cwd);
    DEBUG_ASSERT(path);


    inode_t* c = NULL;

    if (path[0] == '/') {

        shared_ptr_access(current_task->fs, fs, { c = fs->root; });

    } else {

        c = cwd;
    }

    DEBUG_ASSERT(c);


    while (path[0] == '/') {
        path++;
    }

    while (strchr(path, '/') && c) {

        c    = path_find(c, path, strcspn(path, "/"), true, links, depth);
        path = strchr(path, '/') + 1;

        while (path[0] == '/')
            path++;
    }


    if (unlikely(!c)) {
        return errno = (__path_failed_on_its_own() ? errno : ENOENT), NULL;
    }

    inode_t* r;

    if (path[0] != '\0')
        r = path_find(c, path, strlen(path), !(flags & O_NOFOLLOW), links, depth);
    else
        r = c;


    if (unlikely(!r)) {

        if ((flags & O_CREAT) && !__path_failed_on_its_own()) {

            if ((mode & S_IFMT) == 0) {
                mode |= S_IFREG;
            }

            r = vfs_creat(c, path, mode);

            if (unlikely(!r))
                return NULL;

        } else {
            return errno = (__path_failed_on_its_own() ? errno : ENOENT), NULL;
        }

    } else {

        if ((flags & O_EXCL) && (flags & O_CREAT))
            return errno = EEXIST, NULL;
    }

    return r;
}



inode_t* path_lookup(inode_t* cwd, const char* path, int flags, mode_t mode) {

    int links = PATH_MAXSYMLINKS;

    errno = 0;

    return __path_lookup(cwd, path, flags, mode, &links, 0);
}
