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
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(198, lchown,
int sys_lchown(const char* pathname, uid_t owner, gid_t group) {
    int fd = sys_open(pathname, O_NOFOLLOW | O_RDONLY, 0);
    if(fd < 0)
        return -1;

    inode_t* inode = current_task->fd[fd].inode;
    sys_close(fd);

    if(!((current_task->uid == inode->uid) || (current_task->uid == TASK_ROOT_UID))) {
        errno = EPERM;
        return -1;
    }

    return vfs_chown(inode, owner, group);
});