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


static void* do_mmap(void* addr, size_t len, int prot, int flags, int fd, off_t pgoff) {        
    if(unlikely(!len))
        return errno = EINVAL, MAP_FAILED;
    

    inode_t* inode = NULL;
    if(unlikely(!(flags & MAP_ANONYMOUS))) {
        if(fd < 0)
            return errno = EBADF, MAP_FAILED;

        if(!(inode = current_task->fd[fd].inode))
            return errno = EBADF, MAP_FAILED;
    }

    if(len & (PAGE_SIZE - 1)) {
        len += PAGE_SIZE - 1;
        len &= ~(PAGE_SIZE - 1);
    }


    uintptr_t rd = 0;
    if(flags & MAP_FIXED) {
        rd = (uintptr_t) addr;
        rd &= ~(PAGE_SIZE - 1);
        
        uintptr_t i;
        for(i = 0; i < len; i += PAGE_SIZE)
            map_page(rd + i, pmm_alloc_frame() << 12, 1);

    } else {
        if(flags & MAP_PRIVATE)
            rd = (uintptr_t) sys_sbrk(len);
        else
            rd = (uintptr_t) kvalloc(len, GFP_ATOMIC);
    }

    if(unlikely(!rd))
        return errno = ENOMEM, MAP_FAILED;


    if(flags & MAP_ANONYMOUS)
        ; //memset((void*) rd, 0, len);
    else
        if(vmm_swap_setup(rd, pgoff * PAGE_SIZE, len, inode, prot, flags) < 0)
            return errno = EFAULT, MAP_FAILED;


    return (void*) rd;
}

SYSCALL(192, mmap,
    __weak_alias(do_mmap, sys_mmap);
);
