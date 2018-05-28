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

void* sys_mmap(void* addr, size_t len, int prot, int flags, int fildes, off_t off) {
    (void) fildes;
    (void) off;


    if(unlikely(!len)) {
        errno = EINVAL;
        return NULL;
    }

    if(len & (PAGE_SIZE - 1)) {
        len &= ~(PAGE_SIZE - 1);
        len += PAGE_SIZE;
    }


    uintptr_t rd = 0;
    if(flags & MAP_FIXED)
        rd = (uintptr_t) addr;
    else {
        if(flags & MAP_PRIVATE)
            rd = (uintptr_t) sys_sbrk(len);
        else
            rd = (uintptr_t) kvalloc(len, GFP_ATOMIC);
    }

    if(unlikely(!rd)) {
        errno = ENOMEM;
        return NULL;
    }

    
    rd &= ~(PAGE_SIZE - 1);
    
    uintptr_t i;
    for(i = 0; i < len; i += PAGE_SIZE)
        map_page(rd + i, pmm_alloc_frame() << 12, 1);


    if(flags & MAP_ANON)
        memset((void*) rd, 0, len);
        
        
        

    return (void*) rd;
}

SYSCALL(34, _mmap,
    void* sys__mmap(uintptr_t* p) {
        return sys_mmap(
            (void*) p[0],
            (size_t) p[1],
            (int) p[2],
            (int) p[3],
            (int) p[4],
            (off_t) p[5]
        );
    }
);

EXPORT(sys_mmap);