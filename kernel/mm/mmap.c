/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/vmobj.h>



uintptr_t vmm_mmap_find(vmm_address_space_t* space, size_t len, uintptr_t align) {

    DEBUG_ASSERT(space);
    DEBUG_ASSERT(align && !(align & (align - 1)));


    uintptr_t cursor = (space->mmap.heap_start + align - 1) & ~(align - 1);

    for (;;) {

        if (cursor < space->mmap.heap_start || cursor + len < cursor || cursor + len > space->mmap.heap_limit)
            return 0;


        uintptr_t next = cursor;

        for (size_t i = 0; i < CONFIG_MMAP_MAX; i++) {

            const mmap_mapping_t* m = &space->mmap.mappings[i];

            if (m->start && m->start < cursor + len && m->end > cursor && m->end > next)
                next = m->end;
        }

        for (size_t i = 0; i < SHM_ATTACH_MAX; i++) {

            const shm_attach_t* a = &space->shm.attachments[i];

            if (a->addr && a->addr < cursor + len && a->addr + a->size > cursor && a->addr + a->size > next)
                next = a->addr + a->size;
        }

        if (next == cursor)
            return cursor;

        cursor = (next + align - 1) & ~(align - 1);
    }
}


void vmm_mmap_update_top(vmm_address_space_t* space) {

    DEBUG_ASSERT(space);


    uintptr_t top = space->mmap.heap_start;

    for (size_t i = 0; i < CONFIG_MMAP_MAX; i++) {

        if (space->mmap.mappings[i].start && space->mmap.mappings[i].end > top)
            top = space->mmap.mappings[i].end;
    }

    for (size_t i = 0; i < SHM_ATTACH_MAX; i++) {

        if (space->shm.attachments[i].addr && space->shm.attachments[i].addr + space->shm.attachments[i].size > top)
            top = space->shm.attachments[i].addr + space->shm.attachments[i].size;
    }

    space->mmap.heap_end = top;
}


void vmm_mmap_clone(vmm_address_space_t* dest) {

    DEBUG_ASSERT(dest);

    for (size_t i = 0; i < CONFIG_MMAP_MAX; i++) {

        if (dest->mmap.mappings[i].start && dest->mmap.mappings[i].obj) {
            vmobj_map_get(dest->mmap.mappings[i].obj);
        }
    }
}


void vmm_mmap_release(vmm_address_space_t* space) {

    DEBUG_ASSERT(space);

    for (size_t i = 0; i < CONFIG_MMAP_MAX; i++) {

        vmobj_t* obj = space->mmap.mappings[i].obj;

        if (!obj) {
            continue;
        }

        space->mmap.mappings[i].obj = NULL;

        vmobj_map_put(obj);
    }
}


bool vmm_mmap_overlaps(vmm_address_space_t* space, uintptr_t start, uintptr_t end, bool mappings) {

    DEBUG_ASSERT(space);

    if (mappings) {

        for (size_t i = 0; i < CONFIG_MMAP_MAX; i++) {

            const mmap_mapping_t* m = &space->mmap.mappings[i];

            if (m->start && m->start < end && m->end > start) {
                return true;
            }
        }
    }

    for (size_t i = 0; i < SHM_ATTACH_MAX; i++) {

        const shm_attach_t* a = &space->shm.attachments[i];

        if (a->addr && a->addr < end && a->addr + a->size > start) {
            return true;
        }
    }

    return false;
}


long vmm_mmap_unmap(vmm_address_space_t* space, uintptr_t start, uintptr_t end) {

    DEBUG_ASSERT(space);


    mmap_mapping_t* m = space->mmap.mappings;

    for (int i = 0; i < CONFIG_MMAP_MAX; i++) {

        if (m[i].start == 0UL) {
            continue;
        }

        if (m[i].end <= start || m[i].start >= end) {
            continue;
        }


        const uintptr_t lo = m[i].start > start ? m[i].start : start;
        const uintptr_t hi = m[i].end < end ? m[i].end : end;

        int j = -1;

        if (m[i].start < start && m[i].end > end) {

            for (j = 0; j < CONFIG_MMAP_MAX; j++) {

                if (m[j].start == 0UL) {
                    break;
                }
            }

            if (unlikely(j == CONFIG_MMAP_MAX)) {
                vmm_mmap_update_top(space);
                return -ENOMEM;
            }
        }

        arch_vmm_unmap(space, lo, hi - lo);


        if (m[i].start >= start && m[i].end <= end) {

            if (m[i].obj) {
                arch_vmm_release_vmobj(space, m[i].obj);
            }

            memset(&m[i], 0, sizeof(mmap_mapping_t));

        } else if (j >= 0) {

            m[j].start  = end;
            m[j].end    = m[i].end;
            m[j].fd     = m[i].fd;
            m[j].offset = m[i].offset + (end - m[i].start);
            m[j].obj    = m[i].obj;

            if (m[j].obj) {
                vmobj_map_get(m[j].obj);
            }

            m[i].end = start;

        } else if (m[i].start < start) {

            m[i].end = start;

        } else {

            m[i].offset += end - m[i].start;
            m[i].start = end;
        }
    }


    vmm_mmap_update_top(space);

    return 0;
}
