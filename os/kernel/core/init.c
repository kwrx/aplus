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
#include <aplus/elf.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

spinlock_t debug_lock;


static void* kmalloc_std(size_t size) {
    return kmalloc(size, GFP_KERNEL);
}

static void* kcalloc_std(size_t nm, size_t cc) {
    return kcalloc(nm, cc, GFP_KERNEL);
}


void core_init(void) {
    spinlock_init(&debug_lock);
    
    if(!mbd->quiet)
        arch_debug_init();


    libaplus_init(kmalloc_std, kcalloc_std, kfree);    
}