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
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(10, unlink,
int sys_unlink(const char* name) {
    char namebuf[1024];
    strcpy(namebuf, name);

    char* s = namebuf;
    char* p = NULL;

    inode_t* cino = NULL;



    if(s[0] == '/') {
        cino = current_task->root;
        s++;
    } else
        cino = current_task->cwd;

    KASSERT(cino);

    
    do {
        if((p = strchr(s, '/')))
            *p++ = '\0';
        else
            break;

        cino = vfs_finddir(cino, s);
        if(unlikely(!cino)) {
            errno = ENOENT;
            return -1;
        }

        if(S_ISLNK(cino->mode)) {
            if(cino->link) {
                if(cino->link == cino) {
                    errno = ELOOP;
                    return -1;
                }

                cino = cino->link;
            }
        }

        s = p;
    } while(s);

    KASSERT(s);
    KASSERT(*s);
    
    if(vfs_unlink(cino, s) != 0)
        return -1;
    
    return 0;
});
