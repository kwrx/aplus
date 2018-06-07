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
#include <aplus/base.h>
#include <aplus/mm.h>
#include <aplus/vfs.h>
#include <aplus/ipc.h>
#include <aplus/module.h>
#include <aplus/debug.h>
#include <libc.h>

struct mmap {
    task_t* task;
    uintptr_t addr;
    uintptr_t size;
    off_t off;
    inode_t* inode;
    int prot;
    int flags;
};

static list(struct mmap*, mmaps);


#define IN_RANGE(m, a, s)       \
    (a >= (m)->addr && (a + s) <= ((m)->addr + (m)->size))


static void __invalidate(struct mmap* mm, uintptr_t addr, size_t size) {
    list_each(mmaps, v) {
        if(v->inode != mm->inode)
            continue;
        
        if(!IN_RANGE(v, addr, size))
            continue;

        uintptr_t p;
        for(p = 0; p < size; p += PAGE_SIZE)
            disable_page(v->addr + (addr - mm->addr) + mm->off);
    }
}


int vmm_swap(uintptr_t address) {
    list_each(mmaps, v) {
        if(!IN_RANGE(v, address, 1))
            continue;

        address &= ~(PAGE_SIZE - 1);
        enable_page(address);

        if(unlikely(vfs_read(v->inode, (void*) address, (address - v->addr) + v->off, PAGE_SIZE) < 0))
            kprintf(WARN "swap: could not read from %s, pos: %d\n", v->inode, (address - v->addr) + v->off);
        
        return 0;
    }

    errno = EFAULT;
    return -1;
}


int vmm_swap_setup(uintptr_t addr, off_t off, size_t size, inode_t* inode, int prot, int flags) {
    
    struct mmap* mm = NULL;
    list_each(mmaps, v) {
        if(!IN_RANGE(v, addr, size))
            continue;
        
        mm = v;
        break;
    }


    if(mm)
        vmm_swap_remove(mm->addr, mm->size);
    else
        if(!(mm = (struct mmap*) kmalloc(sizeof(struct mmap), GFP_KERNEL)))
            return errno = ENOMEM, -1;


    mm->task = current_task;
    mm->addr = addr;
    mm->size = size;
    mm->off = off;
    mm->inode = inode;
    mm->prot = prot;
    mm->flags = flags;


    uintptr_t p;
    for(p = 0; p < size; p += PAGE_SIZE)
        disable_page(addr + p);


    list_push(mmaps, mm);
    return 0;
}

int vmm_swap_remove(uintptr_t addr, size_t size) {
    struct mmap* mm = NULL;
    list_each(mmaps, v) {
        if(!IN_RANGE(v, addr, size))
            continue;
        
        mm = v;
        break;
    }

    if(unlikely(!mm))
        return errno = ESRCH, -1;

    uintptr_t p;
    for(p = 0; p < size; p += PAGE_SIZE)
        enable_page(addr + p);



    list_remove(mmaps, mm);
    kfree(mm);

    return 0;
}

int vmm_swap_sync(uintptr_t addr, size_t size, int flags) {
    struct mmap* mm = NULL;
    list_each(mmaps, v) {
        if(!IN_RANGE(v, addr, size))
            continue;
        
        mm = v;
        break;
    }

    if(unlikely(!mm))
        return errno = ESRCH, -1;


    int e = 0;
    if(mm->flags & MAP_SHARED)
        e = vfs_write(mm->inode, (void*) addr, (addr - mm->addr) + mm->off, size);

    if(flags & MS_INVALIDATE)
        __invalidate(mm, addr, size);
    
    return e;
}