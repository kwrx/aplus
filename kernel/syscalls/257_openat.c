/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
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

#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/vfs.h>



/**
 * @brief Tells whether the current task may open an existing file with the access mode open(2) asks for.
 *
 * @param st The file's attributes.
 * @param flags The open() flags.
 * @return true for root, for O_PATH, and when the owner, group or other bits that apply grant the access.
 */
static bool __openat_permitted(const struct stat* st, int flags) {

    if (current_task->fsuid == 0 || (flags & O_PATH))
        return true;


    mode_t want = 0;

    if ((flags & 03) != O_WRONLY)
        want |= S_IROTH;

    if ((flags & 03) != O_RDONLY)
        want |= S_IWOTH;


    mode_t have = st->st_mode & S_IRWXO;

    if (st->st_uid == current_task->fsuid)
        have = (st->st_mode & S_IRWXU) >> 6;
    else if (task_in_group(current_task, st->st_gid))
        have = (st->st_mode & S_IRWXG) >> 3;

    return (have & want) == want;
}


/**
 * @brief Opens, and possibly creates, a file for open(), openat() and creat(), or for a syscall that opens a path to reach its inode.
 *
 * @param dfd The directory a relative path starts from, or AT_FDCWD.
 * @param filename The path, in user memory.
 * @param flags The open() flags.
 * @param mode The permissions of a created file.
 * @param check_access Whether to apply open(2)'s permission check, which a file this call created always passes.
 * @return The new descriptor, or a negative errno.
 */
long do_openat(int dfd, const char* filename, int flags, mode_t mode, bool check_access) {
    if (unlikely(!filename))
        return -EINVAL;

    if (unlikely(!uio_check(filename, R_OK)))
        return -EFAULT;

    inode_t* cwd __scoped(vfs_inode_cleanup) = NULL;

    if (dfd < 0) {

        if (dfd != AT_FDCWD)
            return -EBADF;


        shared_ptr_access(current_task->fs, fs, {
            if (unlikely(!fs->cwd))
                return -ENOENT;

            cwd = vfs_inode_get(fs->cwd);
        });


    } else {

        if (dfd >= CONFIG_OPEN_MAX)
            return -EBADF;

        shared_ptr_access(current_task->fd, fds, {
            if (unlikely(!fds->descriptors[dfd].ref))
                return -EBADF;

            cwd = vfs_inode_get(fds->descriptors[dfd].ref->inode);
        });
    }

    DEBUG_ASSERT(cwd);

    char __safe_filename[CONFIG_PATH_MAX] = {0};
    uio_strncpy_u2s(__safe_filename, filename, CONFIG_PATH_MAX);

#if DEBUG_LEVEL_TRACE
    kprintf("syscall: openat(%d, \"%s\", %d, %d)\n", dfd, __safe_filename, flags, mode);
#endif


    inode_t* r __scoped(vfs_inode_cleanup) = NULL;

    bool created = false;

    if ((r = path_lookup_created(cwd, __safe_filename, flags, mode, &created)) == NULL)
        return -errno;

    struct stat st = {0};
    if (vfs_getattr(r, &st) < 0) {
        return (errno == ENOSYS) ? -EACCES : -errno;
    }


    if (
#ifdef O_NOFOLLOW
        !(flags & O_NOFOLLOW) &&
#endif
        S_ISLNK(st.st_mode)) {

        inode_t* target = path_follows(r);

        vfs_inode_put(r);

        if ((r = target) == NULL)
            return -errno;

        if (vfs_getattr(r, &st) < 0)
            return (errno == ENOSYS) ? -EACCES : -errno;
    }



#ifdef O_DIRECTORY
    if (flags & O_DIRECTORY) {
        if (!(S_ISDIR(st.st_mode)))
            return -ENOTDIR;
    }
#endif


    if (check_access && !created && !__openat_permitted(&st, flags))
        return -EACCES;

    if ((flags & O_TRUNC) && ((flags & O_WRONLY) || (flags & O_RDWR)) && S_ISREG(st.st_mode)) {
        if (vfs_truncate(r, 0) < 0)
            return -errno;
    }


    inode_t* inode = NULL;

    if ((inode = vfs_open(r, flags)) == NULL) {

        if (errno != ENOSYS)
            return -errno;

        inode = r;
    }

    DEBUG_ASSERT(inode);


    int fd = -1;

    shared_ptr_access(current_task->fd, fds, {
        scoped_lock(&current_task->lock) {

            for (fd = 0; fd < CONFIG_OPEN_MAX; fd++) {
                if (fds->descriptors[fd].ref == NULL) {
                    break;
                }
            }

            if (fd == CONFIG_OPEN_MAX)
                break;


            struct file* ref = NULL;

            if ((ref = fd_append(inode, 0, flags)) == NULL) {

                fd = CONFIG_FILE_MAX;

            } else {

                fds->descriptors[fd].ref           = ref;
                fds->descriptors[fd].close_on_exec = !!(flags & O_CLOEXEC);
            }
        }


        if (fd == CONFIG_OPEN_MAX)
            return -EMFILE;

        if (fd == CONFIG_FILE_MAX)
            return -ENFILE;


        DEBUG_ASSERT(fd >= 0);
        DEBUG_ASSERT(fd <= CONFIG_OPEN_MAX - 1);
        DEBUG_ASSERT(fds->descriptors[fd].ref);
        DEBUG_ASSERT(fds->descriptors[fd].ref->inode);
    });


    return fd;
}


/***
 * Name:        openat
 * Description: open and possibly create a file
 * URL:         http://man7.org/linux/man-pages/man2/openat.2.html
 *
 * Input Parameters:
 *  0: 0x101
 *  1: int dfd
 *  2: const char  * filename
 *  3: int flags
 *  4: umode_t mode
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(257, openat, long sys_openat(int dfd, const char* filename, int flags, mode_t mode) { return do_openat(dfd, filename, flags, mode, true); });
