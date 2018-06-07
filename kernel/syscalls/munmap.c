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



SYSCALL(91, munmap,
int sys_munmap(void* addr, size_t len) {
    if((uintptr_t) addr & (PAGE_SIZE - 1)) {
        errno = EINVAL;
        return -1;
    }

    vmm_swap_remove((uintptr_t) addr, len);

    uintptr_t p = (uintptr_t) addr;
    if(p >= CONFIG_HEAP_BASE && p <= (CONFIG_HEAP_BASE + CONFIG_HEAP_SIZE))
        kfree(addr);
    else
        for(uintptr_t i = 0; i < len; i += PAGE_SIZE)
            unmap_page(p + i);

    return 0;
});
