/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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
#include <aplus/debug.h>
#include <aplus/smp.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>
#include <aplus/memory.h>
#include <aplus/errno.h>
#include <stdint.h>

#include <hal/timer.h>

#include "tmpfs.h"



void* tmpfs_cache_load(vfs_cache_t* cache, ino_t ino) {


    tmpfs_inode_t* i = (tmpfs_inode_t*) kcalloc(sizeof(tmpfs_inode_t), 1, GFP_KERNEL);

    i->capacity = 0;
    i->data = NULL;


    i->st.st_uid = current_task->uid;
    i->st.st_gid = current_task->gid;

    i->st.st_ino = ino;
    i->st.st_nlink = 1;
    i->st.st_blocks = 1;

    i->st.st_atime = arch_timer_gettime();
    i->st.st_mtime = arch_timer_gettime();
    i->st.st_ctime = arch_timer_gettime();


    return (void*) i;
}



void tmpfs_cache_flush(vfs_cache_t* cache, ino_t ino, void* data) {
    return;
}