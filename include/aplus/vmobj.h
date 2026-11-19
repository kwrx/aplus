/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2026 Antonino Natale
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

#ifndef _APLUS_VMOBJ_H
#define _APLUS_VMOBJ_H

#ifndef __ASSEMBLY__

    #include <stdatomic.h>
    #include <stddef.h>
    #include <stdint.h>
    #include <sys/types.h>

    #include <aplus.h>
    #include <aplus/ipc.h>
    #include <aplus/memory.h>


typedef struct vmobj {

    _Atomic size_t refcount;

    size_t mapped;
    size_t size;
    size_t pages;
    size_t slots;
    uintptr_t* frames;

    spinlock_t lock;

} vmobj_t;


__BEGIN_DECLS

vmobj_t* vmobj_create(size_t size);
void vmobj_get(vmobj_t* obj);
void vmobj_put(vmobj_t* obj);

size_t vmobj_size(vmobj_t* obj);
int vmobj_resize(vmobj_t* obj, size_t size);

ssize_t vmobj_read(vmobj_t* obj, void* buf, off_t pos, size_t len);
ssize_t vmobj_write(vmobj_t* obj, const void* buf, off_t pos, size_t len);

long vmobj_map(vmobj_t* obj, vmm_address_space_t* space, uintptr_t start, size_t len, off_t offset, int flags);
void vmobj_map_get(vmobj_t* obj);
void vmobj_map_put(vmobj_t* obj);

void vmm_mmap_clone(vmm_address_space_t* dest);
void vmm_mmap_release(vmm_address_space_t* space);

__END_DECLS

#endif
#endif
