/*
 * Author(s):
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

#ifndef _APLUS_SHM_H
#define _APLUS_SHM_H

#ifndef __ASSEMBLY__

    #include <stddef.h>
    #include <stdint.h>
    #include <sys/types.h>

    #include <aplus.h>
    #include <aplus/memory.h>


    //* System V shared memory.
    //*
    //* A segment is a list of physical frames owned by the segment itself rather than by any
    //* address space that has it attached. Attaching maps those frames with
    //* ARCH_VMM_MAP_TYPE_SHARED, which is what keeps fork(2) from copying them and address
    //* space teardown from freeing them; the frames go back to the physical allocator only
    //* once the segment has been marked for removal and the last attachment is gone.


    //? Segments alive at once, system-wide. An id is the slot index plus a sequence number,
    //? the classic System V scheme, so an id is never silently reused by a later segment that
    //? happens to land in the same slot.
    #define SHM_SEGMENT_MAX 64

    //? Largest single segment. A full-screen 32-bit surface at 4K is 33MiB, so the cap is set
    //? just above that: shared memory here exists to carry window surfaces.
    #define SHM_SEGMENT_SIZE_MAX (64UL * 1024UL * 1024UL)

    //? Physical memory every live segment may hold between them. Unlike an anonymous mapping
    //? this memory is never reclaimed under pressure and is not charged to any one process,
    //? so it needs a ceiling of its own.
    #define SHM_TOTAL_MAX (192UL * 1024UL * 1024UL)


struct shmid_ds;

__BEGIN_DECLS

long shm_get(key_t key, size_t size, int flags);
long shm_attach(int id, uintptr_t addr, int flags);
long shm_detach(uintptr_t addr);
long shm_control(int id, int cmd, struct shmid_ds* buf);


/*!
 * @brief shm_address_space_clone().
 *        Give @dest a reference to every segment @parent has attached.
 *
 * The page table entries themselves are duplicated by the clone walk; this is the
 * bookkeeping behind them.
 */
void shm_address_space_clone(vmm_address_space_t* parent, vmm_address_space_t* dest) __nonnull(1, 2);


/*!
 * @brief shm_address_space_release().
 *        Drop every attachment @space still holds, as if it had called shmdt(2) on each.
 *
 * Called when an address space is torn down, so that a process that exits or execs without
 * detaching does not pin a segment forever.
 */
void shm_address_space_release(vmm_address_space_t* space) __nonnull(1);

__END_DECLS

#endif
#endif
