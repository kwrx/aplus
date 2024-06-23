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

SYSCALL(
    257, openat, long sys_openat(int dfd, const char *filename, int flags, mode_t mode) {
        if (unlikely(!filename))
            return -EINVAL;

        if (unlikely(!uio_check(filename, R_OK)))
            return -EFAULT;



        inode_t *cwd = NULL;


        if (dfd < 0) {

            if (dfd != AT_FDCWD)
                return -EBADF;


            shared_ptr_access(current_task->fs, fs, {
                if (unlikely(!fs->cwd))
                    return -ENOENT;

                cwd = fs->cwd;
            });


        } else {

            if (dfd >= CONFIG_OPEN_MAX)
                return -EBADF;

            shared_ptr_access(current_task->fd, fds, {
                if (unlikely(!fds->descriptors[dfd].ref))
                    return -EBADF;

                cwd = fds->descriptors[dfd].ref->inode;
            });
        }

        DEBUG_ASSERT(cwd);



        char __safe_filename[CONFIG_PATH_MAX] = {0};
        uio_strncpy_u2s(__safe_filename, filename, CONFIG_PATH_MAX);



#if DEBUG_LEVEL_TRACE
        kprintf("openat(%d, \"%s\", %d, %d)\n", dfd, __safe_filename, flags, mode);
#endif


        inode_t *r = NULL;

        if ((r = path_lookup(cwd, __safe_filename, flags, mode)) == NULL)
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
            if ((r = path_follows(r)) == NULL)
                return -errno;
        }



#ifdef O_DIRECTORY
        if (flags & O_DIRECTORY) {
            if (!(S_ISDIR(st.st_mode)))
                return -ENOTDIR;
        }
#endif



        if (current_task->uid != 0) {

            if (st.st_uid == current_task->uid) {
                if (!((flags & O_RDONLY ? (mode & S_IRUSR) : 1) && (flags & O_WRONLY ? (mode & S_IWUSR) : 1) && (flags & O_RDWR ? (mode & S_IRUSR) && (mode & S_IWUSR) : 1)))
                    return -EACCES;

            } else if (st.st_gid == current_task->gid) {
                if (!((flags & O_RDONLY ? (mode & S_IRGRP) : 1) && (flags & O_WRONLY ? (mode & S_IWGRP) : 1) && (flags & O_RDWR ? (mode & S_IRGRP) && (mode & S_IWGRP) : 1)))
                    return -EACCES;

            } else {
                if (!((flags & O_RDONLY ? (mode & S_IROTH) : 1) && (flags & O_WRONLY ? (mode & S_IWOTH) : 1) && (flags & O_RDWR ? (mode & S_IROTH) && (mode & S_IWOTH) : 1)))
                    return -EACCES;
            }
        }



        inode_t *inode = NULL;

        if ((inode = vfs_open(r, flags)) == NULL) {

            if (errno != ENOSYS)
                return -errno;

            inode = r;
        }

        DEBUG_ASSERT(inode);


        int fd = -1;

        shared_ptr_access(current_task->fd, fds, {
            __lock(&current_task->lock, {
                for (fd = 0; fd < CONFIG_OPEN_MAX; fd++) {

                    if (fds->descriptors[fd].ref == NULL)
                        break;
                }

                if (fd == CONFIG_OPEN_MAX)
                    break;


                struct file *ref = NULL;

                if ((ref = fd_append(inode, 0, 0)) == NULL) {

                    fd = CONFIG_FILE_MAX;

                } else {

                    if (flags & O_APPEND)
                        ref->position = st.st_size;
                    else
                        ref->position = 0;


                    fds->descriptors[fd].ref   = ref;
                    fds->descriptors[fd].flags = flags;
                }
            });


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
    });
