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


    /**
     * @brief System V shared memory: a segment is a list of frames owned by the segment itself.
     *
     * Attaching maps them with ARCH_VMM_MAP_TYPE_SHARED, so neither a fork nor a teardown touches them.
     */


    /**
     * @brief Segments alive at once, system-wide; an id is a slot index plus a sequence number.
     */
    #define SHM_SEGMENT_MAX 64

    /**
     * @brief Largest single segment, set just above the 33MiB a full-screen 32-bit surface at 4K takes.
     */
    #define SHM_SEGMENT_SIZE_MAX (64UL * 1024UL * 1024UL)

    /**
     * @brief Physical memory every live segment may hold between them, which nothing reclaims under pressure.
     */
    #define SHM_TOTAL_MAX (192UL * 1024UL * 1024UL)


struct shmid_ds;

__BEGIN_DECLS

long shm_get(key_t key, size_t size, int flags);
long shm_attach(int id, uintptr_t addr, int flags);
long shm_detach(uintptr_t addr);
long shm_control(int id, int cmd, struct shmid_ds* buf);


/**
 * @brief Gives an address space a reference to every segment another one has attached.
 *
 * @param parent The address space being cloned.
 * @param dest The address space receiving the references.
 */
void shm_address_space_clone(vmm_address_space_t* parent, vmm_address_space_t* dest) __nonnull(1, 2);


/**
 * @brief Drops every attachment an address space still holds, as if it had called shmdt(2) on each.
 *
 * @param space The address space being torn down.
 */
void shm_address_space_release(vmm_address_space_t* space) __nonnull(1);

__END_DECLS

#endif
#endif
