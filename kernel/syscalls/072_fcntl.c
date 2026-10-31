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
#include <poll.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/vfs.h>

#include <aplus/network.h>


#define FCNTL_SETFL_MASK (O_APPEND | O_ASYNC | O_DIRECT | O_NOATIME | O_NONBLOCK)


static long __fcntl_lock_range(struct file* file, const struct flock* fl, off_t* start, off_t* end) {

    off_t base = 0;

    switch (fl->l_whence) {

        case SEEK_SET:
            break;

        case SEEK_CUR:
            scoped_lock(&file->lock) {
                base = file->position;
            }
            break;

        case SEEK_END: {

            struct stat st = {0};

            if (vfs_getattr(file->inode, &st) < 0)
                return -errno;

            base = st.st_size;
            break;
        }

        default:
            return -EINVAL;
    }


    off_t first;
    off_t last;

    if (__builtin_add_overflow(base, fl->l_start, &first))
        return -EOVERFLOW;

    if (fl->l_len > 0) {

        if (__builtin_add_overflow(first, fl->l_len - 1, &last))
            return -EOVERFLOW;

    } else if (fl->l_len == 0) {

        last = VFS_LOCK_EOF;

    } else {

        last = first - 1;

        if (__builtin_add_overflow(first, fl->l_len, &first))
            return -EINVAL;
    }

    if (first < 0 || last < first)
        return -EINVAL;

    *start = first;
    *end   = last;

    return 0;
}


static long __fcntl_lock(unsigned int fd, unsigned int cmd, struct flock* user) {

    const bool ofd  = cmd == F_OFD_GETLK || cmd == F_OFD_SETLK || cmd == F_OFD_SETLKW;
    const bool test = cmd == F_GETLK || cmd == F_OFD_GETLK;
    const bool wait = cmd == F_SETLKW || cmd == F_OFD_SETLKW;
    const int mode  = test ? R_OK | W_OK : R_OK;

    if (unlikely(!uio_check(user, mode) || !uio_check((const char*)(user + 1) - 1, mode)))
        return -EFAULT;


    struct flock fl;
    uio_memcpy_u2s(&fl, user, sizeof(fl));

    if (fl.l_type != F_RDLCK && fl.l_type != F_WRLCK && fl.l_type != F_UNLCK)
        return -EINVAL;

    if (test && fl.l_type == F_UNLCK)
        return -EINVAL;

    if (ofd && fl.l_pid != 0)
        return -EINVAL;


    int flags         = 0;
    struct file* file = fd_get(fd, &flags);

    if (unlikely(!file))
        return -EBADF;

    if (unlikely(flags & O_PATH)) {
        fd_put(file);
        return -EBADF;
    }

    if (!test && ((fl.l_type == F_RDLCK && (flags & 03) == O_WRONLY) || (fl.l_type == F_WRLCK && (flags & 03) == O_RDONLY))) {
        fd_put(file);
        return -EBADF;
    }


    uintptr_t owner = (uintptr_t)file;

    if (!ofd)
        shared_ptr_access(current_task->fd, fds, { owner = (uintptr_t)fds; });


    off_t start = 0;
    off_t end   = 0;
    long e      = __fcntl_lock_range(file, &fl, &start, &end);

    if (e < 0) {
        fd_put(file);
        return e;
    }


    if (test) {

        short type = fl.l_type;
        pid_t pid  = 0;

        vfs_lock_test(file->inode, owner, &type, &start, &end, &pid);

        fl.l_type = type;

        if (type != F_UNLCK) {
            fl.l_whence = SEEK_SET;
            fl.l_start  = start;
            fl.l_len    = end == VFS_LOCK_EOF ? 0 : end - start + 1;
            fl.l_pid    = pid;
        }

        fd_put(file);

        uio_memcpy_s2u(user, &fl, sizeof(fl));
        return 0;
    }


    const uint32_t seq = atomic_load(&vfs_lock_event);

    if (vfs_lock_set(file->inode, owner, ofd ? -1 : current_task->pid, fl.l_type, start, end) == 0) {
        fd_put(file);
        return 0;
    }

    e = -errno;

    fd_put(file);

    if (e != -EAGAIN || !wait)
        return e;


    futex_wait(current_task, &vfs_lock_event, seq, NULL);

    thread_suspend(current_task);
    thread_restart_sched(current_task);
    thread_restart_syscall(current_task);

    return -EINTR;
}


/***
 * Name:        fcntl
 * Description: manipulate file descriptor
 * URL:         http://man7.org/linux/man-pages/man2/fcntl.2.html
 *
 * Input Parameters:
 *  0: 0x48
 *  1: unsigned int fd
 *  2: unsigned int cmd
 *  3: unsigned long arg
 *
 * Auto-generated by scripts/gen-syscalls.js
 */

SYSCALL(
    72, fcntl, long sys_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg) {

        {

            if (unlikely(fd >= CONFIG_OPEN_MAX))
                return -EBADF;


            shared_ptr_access(current_task->fd, fds, {
                if (unlikely(!fds->descriptors[fd].ref))
                    return -EBADF;
            });


            switch (cmd) {

                case F_DUPFD:
                case F_DUPFD_CLOEXEC:

                    if (unlikely(arg >= CONFIG_OPEN_MAX))
                        return -EINVAL;

                    return fd_dup(fd, (unsigned int)arg, cmd == F_DUPFD_CLOEXEC);

                case F_GETFD:

                    shared_ptr_access(current_task->fd, fds, { return fds->descriptors[fd].close_on_exec ? FD_CLOEXEC : 0; });

                case F_SETFD:

                    shared_ptr_access(current_task->fd, fds, { fds->descriptors[fd].close_on_exec = !!(arg & FD_CLOEXEC); });

                    return 0;

                case F_GETFL: {

                    int flags         = 0;
                    struct file* file = fd_get(fd, &flags);

                    if (unlikely(!file))
                        return -EBADF;

                    fd_put(file);

                    return flags;
                }

                case F_SETFL: {

                    struct file* file = fd_get(fd, NULL);

                    if (unlikely(!file))
                        return -EBADF;

                    fd_set_flags(file, FCNTL_SETFL_MASK, (int)arg);
                    fd_put(file);

                    return 0;
                }

                case F_GETLK:
                case F_SETLK:
                case F_SETLKW:
                case F_OFD_GETLK:
                case F_OFD_SETLK:
                case F_OFD_SETLKW:
                    return __fcntl_lock(fd, cmd, (struct flock*)arg);

                case F_SETPIPE_SZ:
                case F_GETPIPE_SZ:
                    /* TODO: Implements pipe size routines for fcntl */
                    return -ENOSYS;
            }


            return -EINVAL;
        }
    });
