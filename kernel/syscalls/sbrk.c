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

SYSCALL(801, sbrk,
void* sys_sbrk(ptrdiff_t incr) {
    if(current_task->image->end + incr < current_task->image->start) {
        errno = EINVAL;
        return (void*) -1;
    }

    uintptr_t cr = current_task->image->end;

    incr += PAGE_SIZE;
    incr &= ~(PAGE_SIZE - 1);
    
    
    int i;
    if(incr > 0)
        for(i = 0; i < incr; i += PAGE_SIZE)
            map_page(current_task->image->end + i, pmm_alloc_frame() << 12, 1);
    else
        for(i = 0; i >= incr; i -= PAGE_SIZE)
            unmap_page(current_task->image->end + i);
    
    current_task->image->end += incr;
    current_task->vmsize += incr;
    return (void*) cr;
});
