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

SYSCALL(8, link,
int sys_link(const char* oldname, const char* newname) {
    if(unlikely(!oldname || !newname)) {
        errno = EINVAL;
        return -1;
    }

    int sfd = sys_open(oldname, O_RDONLY, 0);
    if(sfd < 0) {
        errno = ENOENT;
        return -1;
    }


    int dfd = sys_open(newname, O_EXCL | O_CREAT | O_RDONLY, S_IFLNK | 0666);
    if(dfd < 0)
        return -1;
    
    inode_t* sino = current_task->fd[sfd].inode;
    inode_t* dino = current_task->fd[dfd].inode;


    sino->nlink += 1;
    dino->link = sino;

    sys_close(dfd);
    sys_close(sfd);

    return 0;
});
