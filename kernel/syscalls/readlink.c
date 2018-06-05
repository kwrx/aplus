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
#include <aplus/network.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(85, readlink,
ssize_t sys_readlink(const char* filename, char* buf, size_t bufsize) {
    if(unlikely(!filename || !buf || !bufsize)) {
        errno = EINVAL;
        return -1;
    }
    
    
    int fd = sys_open(filename, O_RDONLY | O_NOFOLLOW, 0);
    if(fd < 0)
        return -1;

    inode_t* inode = current_task->fd[fd].inode;
    sys_close(fd);
    
    
    if(unlikely(!inode)) {
        errno = EBADF;
        return -1;
    }

    if(likely(S_ISLNK(inode->mode))) {
        if(likely(inode->link)) {
            char b[BUFSIZ];
            char* p = b;
            int i;
            int j;
            
            
            inode_t* tmp;
            for(tmp = inode->link; tmp->parent; tmp = tmp->parent) {
                if(tmp == current_task->root)
                    break;
                    
                for(i = strlen(tmp->name) - 1; i >= 0; i--)
                    *p++ = tmp->name[i];
                    
                *p++ = '/';
            }
            
            if(p == b)
                *p++ = '/';
            *p++ = '\0';
            
            for(i = 0, j = strlen(b) - 1; i < bufsize && j >= 0; i++, j--)
                buf[i] = b[j];
                
            buf[i++] = '\0';
            return i;
        }
    }

    errno = EINVAL;
    return -1;        
});
