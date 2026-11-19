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

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/vmobj.h>


#define VMOBJ_PAGESIZE PML1_PAGESIZE
#define VMOBJ_SIZE_MAX (SIZE_MAX / 4)


static inline uint8_t* __vmobj_page(vmobj_t* obj, size_t page) {
    return (uint8_t*)arch_vmm_p2v(obj->frames[page], ARCH_VMM_AREA_HEAP);
}


static void __vmobj_trim_locked(vmobj_t* obj, size_t keep) {

    while (obj->pages > keep) {

        obj->pages--;

        pmm_free_block(obj->frames[obj->pages]);

        obj->frames[obj->pages] = 0;
    }
}


static int __vmobj_reserve_locked(vmobj_t* obj, size_t pages) {

    if (pages <= obj->pages) {
        return 0;
    }


    if (pages > obj->slots) {

        size_t slots = obj->slots ? obj->slots : 16;

        while (slots < pages) {
            slots *= 2;
        }

        uintptr_t* frames = (uintptr_t*)krealloc(obj->frames, slots * sizeof(uintptr_t), GFP_KERNEL);

        if (unlikely(!frames)) {
            return -ENOMEM;
        }

        obj->frames = frames;
        obj->slots  = slots;
    }


    const size_t had = obj->pages;

    while (obj->pages < pages) {

        uintptr_t frame = pmm_alloc_block();

        if (unlikely(frame == PMM_INVALID_ADDRESS)) {
            __vmobj_trim_locked(obj, had);
            return -ENOMEM;
        }

        memset((void*)arch_vmm_p2v(frame, ARCH_VMM_AREA_HEAP), 0, VMOBJ_PAGESIZE);

        obj->frames[obj->pages++] = frame;
    }

    return 0;
}


static void __vmobj_zero_locked(vmobj_t* obj, size_t from, size_t to) {

    while (from < to) {

        const size_t page = from / VMOBJ_PAGESIZE;
        const size_t off  = from % VMOBJ_PAGESIZE;
        const size_t n    = MIN(VMOBJ_PAGESIZE - off, to - from);

        memset(__vmobj_page(obj, page) + off, 0, n);

        from += n;
    }
}


static int __vmobj_resize_locked(vmobj_t* obj, size_t size) {

    if (unlikely(size > VMOBJ_SIZE_MAX)) {
        return -EFBIG;
    }


    const size_t pages = (size + VMOBJ_PAGESIZE - 1) / VMOBJ_PAGESIZE;

    if (size > obj->size) {

        const size_t had = obj->pages;

        int e = __vmobj_reserve_locked(obj, pages);

        if (unlikely(e < 0)) {
            return e;
        }

        __vmobj_zero_locked(obj, obj->size, MIN(size, had * VMOBJ_PAGESIZE));

    } else if (!obj->mapped) {

        __vmobj_trim_locked(obj, pages);
    }

    obj->size = size;

    return 0;
}


static void __vmobj_copy_locked(vmobj_t* obj, void* buf, size_t pos, size_t len, bool out) {

    uint8_t* p = (uint8_t*)buf;

    while (len) {

        const size_t page = pos / VMOBJ_PAGESIZE;
        const size_t off  = pos % VMOBJ_PAGESIZE;
        const size_t n    = MIN(VMOBJ_PAGESIZE - off, len);

        if (out) {
            memcpy(p, __vmobj_page(obj, page) + off, n);
        } else {
            memcpy(__vmobj_page(obj, page) + off, p, n);
        }

        p += n;
        pos += n;
        len -= n;
    }
}


vmobj_t* vmobj_create(size_t size) {

    vmobj_t* obj = (vmobj_t*)kcalloc(1, sizeof(vmobj_t), GFP_KERNEL);

    if (unlikely(!obj)) {
        return NULL;
    }

    atomic_init(&obj->refcount, 1);
    spinlock_init(&obj->lock);


    if (size) {

        int e = 0;

        scoped_lock(&obj->lock) {
            e = __vmobj_resize_locked(obj, size);
        }

        if (unlikely(e < 0)) {
            vmobj_put(obj);
            return NULL;
        }
    }

    return obj;
}


void vmobj_get(vmobj_t* obj) {

    DEBUG_ASSERT(obj);
    DEBUG_ASSERT(atomic_load(&obj->refcount) > 0);

    atomic_fetch_add(&obj->refcount, 1);
}


void vmobj_put(vmobj_t* obj) {

    DEBUG_ASSERT(obj);
    DEBUG_ASSERT(atomic_load(&obj->refcount) > 0);

    if (atomic_fetch_sub(&obj->refcount, 1) > 1) {
        return;
    }


    DEBUG_ASSERT(obj->mapped == 0);

    __vmobj_trim_locked(obj, 0);

    if (obj->frames) {
        kfree(obj->frames);
    }

    kfree(obj);
}


size_t vmobj_size(vmobj_t* obj) {

    DEBUG_ASSERT(obj);

    size_t size = 0;

    scoped_lock(&obj->lock) {
        size = obj->size;
    }

    return size;
}


int vmobj_resize(vmobj_t* obj, size_t size) {

    DEBUG_ASSERT(obj);

    int e = 0;

    scoped_lock(&obj->lock) {
        e = __vmobj_resize_locked(obj, size);
    }

    return e;
}


ssize_t vmobj_read(vmobj_t* obj, void* buf, off_t pos, size_t len) {

    DEBUG_ASSERT(obj);
    DEBUG_ASSERT(buf);

    if (unlikely(pos < 0)) {
        return -EINVAL;
    }


    scoped_lock(&obj->lock) {

        if ((size_t)pos >= obj->size) {
            return 0;
        }

        if (len > obj->size - (size_t)pos) {
            len = obj->size - (size_t)pos;
        }

        __vmobj_copy_locked(obj, buf, (size_t)pos, len, true);
    }

    return (ssize_t)len;
}


ssize_t vmobj_write(vmobj_t* obj, const void* buf, off_t pos, size_t len) {

    DEBUG_ASSERT(obj);
    DEBUG_ASSERT(buf);

    if (unlikely(pos < 0)) {
        return -EINVAL;
    }

    if (unlikely((size_t)pos + len < (size_t)pos)) {
        return -EFBIG;
    }


    scoped_lock(&obj->lock) {

        if ((size_t)pos + len > obj->size) {

            int e = __vmobj_resize_locked(obj, (size_t)pos + len);

            if (unlikely(e < 0)) {
                return e;
            }
        }

        __vmobj_copy_locked(obj, (void*)buf, (size_t)pos, len, false);
    }

    return (ssize_t)len;
}


void vmobj_map_get(vmobj_t* obj) {

    DEBUG_ASSERT(obj);

    scoped_lock(&obj->lock) {
        obj->mapped++;
    }

    vmobj_get(obj);
}


void vmobj_map_put(vmobj_t* obj) {

    DEBUG_ASSERT(obj);

    scoped_lock(&obj->lock) {

        DEBUG_ASSERT(obj->mapped > 0);

        if (--obj->mapped == 0) {
            __vmobj_trim_locked(obj, (obj->size + VMOBJ_PAGESIZE - 1) / VMOBJ_PAGESIZE);
        }
    }

    vmobj_put(obj);
}


long vmobj_map(vmobj_t* obj, vmm_address_space_t* space, uintptr_t start, size_t len, off_t offset, int flags) {

    DEBUG_ASSERT(obj);
    DEBUG_ASSERT(space);

    if (unlikely(offset < 0 || (offset & (VMOBJ_PAGESIZE - 1)))) {
        return -EINVAL;
    }

    if (unlikely((start & (VMOBJ_PAGESIZE - 1)) || (len & (VMOBJ_PAGESIZE - 1)) || !len)) {
        return -EINVAL;
    }


    const size_t first = (size_t)offset / VMOBJ_PAGESIZE;
    const size_t count = len / VMOBJ_PAGESIZE;

    if (unlikely(first + count < first || (first + count) > VMOBJ_SIZE_MAX / VMOBJ_PAGESIZE)) {
        return -EOVERFLOW;
    }


    scoped_lock(&obj->lock) {

        int e = __vmobj_reserve_locked(obj, first + count);

        if (unlikely(e < 0)) {
            return e;
        }

        obj->mapped++;
    }

    vmobj_get(obj);


    flags &= ~(ARCH_VMM_MAP_TYPE_MASK | ARCH_VMM_MAP_HUGETLB | ARCH_VMM_MAP_HUGE_1GB);
    flags |= ARCH_VMM_MAP_TYPE_SHARED | ARCH_VMM_MAP_FIXED;

    for (size_t i = 0; i < count; i++) {

        uintptr_t frame = 0;

        scoped_lock(&obj->lock) {
            frame = obj->frames[first + i];
        }

        if (likely(arch_vmm_map(space, start + i * VMOBJ_PAGESIZE, frame, VMOBJ_PAGESIZE, flags) != ARCH_VMM_MAP_FAILED)) {
            continue;
        }


        if (i) {
            arch_vmm_unmap(space, start, i * VMOBJ_PAGESIZE);
        }

        arch_vmm_release_vmobj(space, obj);

        return -ENOMEM;
    }

    return 0;
}
