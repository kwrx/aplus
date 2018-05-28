/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <libc.h>

SYSCALL(25, umount,
int sys_umount(const char* dev) {
    if((current_task->uid != TASK_ROOT_UID) && (current_task->gid != TASK_ROOT_GID)) {
        errno = EPERM;
        return -1;
    }

    inode_t* dev_ino = NULL;

    int devfd = sys_open(dev, O_RDONLY, 0);
    if(devfd >= 0) {
        dev_ino = current_task->fd[devfd].inode;
        sys_close(devfd);
    }

    return vfs_umount(dev_ino);
});

SYSCALL(26, umount2,
int sys_umount2(const char* dev, int flags) {
    (void) flags;

    return sys_umount(dev);
});
