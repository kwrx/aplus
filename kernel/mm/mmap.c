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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>



/**
 * @brief Finds the lowest range of the mmap window clear of every mapping and shared memory attachment; call with the space locked.
 *
 * @param space The address space.
 * @param len The length of the range, a multiple of @p align.
 * @param align The alignment the range starts on, a power of two.
 * @return The start of the range, or 0 when the window has no room for it.
 */
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


/**
 * @brief Moves the top of the mmap window down to the end of its highest mapping or attachment; call with the space locked.
 *
 * @param space The address space.
 */
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
