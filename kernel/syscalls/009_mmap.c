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

#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>
#include <aplus/smp.h>
#include <aplus/errno.h>
#include <aplus/hal.h>




/***
 * Name:        mmap
 * Description: map or unmap files or devices into memory
 * URL:         http://man7.org/linux/man-pages/man2/mmap.2.html
 *
 * Input Parameters:
 *  0: 0x09
 *  1: undefined
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

SYSCALL(9, mmap,
long sys_mmap (unsigned long addr, unsigned long len, int prot, int flags, int fd, long offset) {

    DEBUG_ASSERT(current_task);    
    DEBUG_ASSERT(current_task->address_space);    
    DEBUG_ASSERT(current_task->address_space->mmap.heap_start);    
    DEBUG_ASSERT(current_task->address_space->mmap.heap_end);    


    // not supported    
    PANIC_ON((flags & MAP_TYPE) == MAP_PRIVATE);
    PANIC_ON((flags & MAP_TYPE) != MAP_SHARED);
    PANIC_ON((flags & MAP_TYPE) != MAP_SHARED_VALIDATE);

    PANIC_ON(!(flags & MAP_FIXED));
    PANIC_ON(!(flags & MAP_FIXED_NOREPLACE));
    PANIC_ON(!(flags & MAP_GROWSDOWN));


    // Silenty ignored
    DEBUG_ASSERT(!(flags & MAP_SYNC));
    DEBUG_ASSERT(!(flags & MAP_NORESERVE));
    DEBUG_ASSERT(!(flags & MAP_32BIT));
    DEBUG_ASSERT(!(flags & MAP_NONBLOCK));


    if(unlikely(flags == 0))
        return -EINVAL;


    if(unlikely(
        !(flags & MAP_PRIVATE) &&
        !(flags & MAP_SHARED)  &&
        !(flags & MAP_SHARED_VALIDATE)
    )) return -EINVAL;


    uintptr_t pagesize = arch_vmm_getpagesize();
    uintptr_t start = 0UL;



    if(!(flags & MAP_ANONYMOUS)) {

        if(unlikely(fd >= CONFIG_OPEN_MAX))
            return -EBADF;

        shared_ptr_access(current_task->fd, fds, {

            if(unlikely(!fds->descriptors[fd].ref))
                return -EBADF;

        });

    }

    


    int arch_flags = 0;


    if(prot != PROT_NONE)
        arch_flags |= ARCH_VMM_MAP_USER;

    // if(!(prot & PROT_READ))
    //     arch_flags |= ARCH_VMM_MAP_USER;

    if(!(prot & PROT_EXEC))
        arch_flags |= ARCH_VMM_MAP_NOEXEC;

    if( (prot & PROT_WRITE))
        arch_flags |= ARCH_VMM_MAP_RDWR;


#if defined(CONFIG_DEMAND_PAGING)
    if(
        !(flags & MAP_POPULATE) &&
        !(flags & MAP_LOCKED)
    ) arch_flags |= ARCH_VMM_MAP_DEMAND;
#endif


    if(flags & MAP_HUGETLB) {
     
        arch_flags |= ARCH_VMM_MAP_HUGETLB;


        switch((flags & MAP_HUGE_MASK)) {

            case MAP_HUGE_2MB: arch_flags |= ARCH_VMM_MAP_HUGE_2MB; pagesize = arch_vmm_gethugepagesize(ARCH_VMM_MAP_HUGE_2MB); break;
            case MAP_HUGE_1GB: arch_flags |= ARCH_VMM_MAP_HUGE_1GB; pagesize = arch_vmm_gethugepagesize(ARCH_VMM_MAP_HUGE_2MB); break;

            default:
                return -EINVAL;

        }

    }


    if(unlikely(addr & (pagesize - 1)))
        return -EINVAL;

    if(unlikely(offset & (pagesize - 1)))
        return -EINVAL;


    if(unlikely(len & (pagesize - 1))) {
        len = (len & ~(pagesize - 1)) + pagesize;
    }
    

    spinlock_lock(&current_task->address_space->lock);


    int i;
    for(i = 0; i < CONFIG_MMAP_MAX; i++) {

        if(current_task->address_space->mmap.mappings[i].start != 0UL)
            continue;

        
        current_task->address_space->mmap.mappings[i].start  = current_task->address_space->mmap.heap_end - len;
        current_task->address_space->mmap.mappings[i].end    = current_task->address_space->mmap.heap_end;
        current_task->address_space->mmap.mappings[i].fd     = fd;
        current_task->address_space->mmap.mappings[i].offset = offset;


        if(current_task->address_space->mmap.heap_end & (pagesize - 1)) {
         
            current_task->address_space->mmap.heap_end &= ~(pagesize - 1);
            current_task->address_space->mmap.heap_end +=  (pagesize);
        
        }


        start = current_task->address_space->mmap.heap_end;

        current_task->address_space->mmap.heap_end += len;

        break;

    }

    spinlock_unlock(&current_task->address_space->lock);


    if(i == CONFIG_MMAP_MAX)
        return -ENOMEM;




    uintptr_t ret = arch_vmm_map(current_task->address_space, start, -1, len, arch_flags);

    // if(unlikely(ret < 0))
    //     return -ENOMEM;


    if(flags & MAP_LOCKED || flags & MAP_POPULATE) {

        if(flags & MAP_ANONYMOUS)
            memset((void*) ret, 0, len);
        else
            sys_read(fd, (void*) start, len);

    }
    

    return ret;


});
