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
                                                                        
#ifndef _APLUS_MEMORY_H
#define _APLUS_MEMORY_H

#ifndef __ASSEMBLY__

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>


//* PMM */
#define PML2_PAGESIZE               (134217728UL)               //? 128MiB
#define PML1_PAGESIZE               (4096UL)                    //? 4KiB

#define PML2_MAX_ENTRIES            (16384UL)                   //? 2TiB
#define PML1_MAX_ENTRIES            (4096UL / sizeof(uint64_t))

#define PML1_PREALLOCATED_BITMAPS   (16UL)                      //? 2GiB


//* Heap */
#define GFP_KERNEL                  0
#define GFP_ATOMIC                  1
#define GFP_USER                    2


//* MMIO */
#define mmio_r8(p)                  (*(uint8_t volatile*) (p))
#define mmio_r16(p)                 (*(uint16_t volatile*) (p))
#define mmio_r32(p)                 (*(uint32_t volatile*) (p))
#define mmio_r64(p)                 (*(uint64_t volatile*) (p))

#define mmio_w8(p, v)               { mmio_r8(p) = (uint8_t) (v); }
#define mmio_w16(p, v)              { mmio_r16(p) = (uint16_t) (v); }
#define mmio_w32(p, v)              { mmio_r32(p) = (uint32_t) (v); }
#define mmio_w64(p, v)              { mmio_r64(p) = (uint64_t) (v); }


//* Virtual Memory */
#define ARCH_VMM_MAP_RDWR           (1 << 0)
#define ARCH_VMM_MAP_NOEXEC         (1 << 1)
#define ARCH_VMM_MAP_USER           (1 << 2)
#define ARCH_VMM_MAP_UNCACHED       (1 << 3)
#define ARCH_VMM_MAP_SHARED         (1 << 4)
#define ARCH_VMM_MAP_FIXED          (1 << 5)
#define ARCH_VMM_MAP_DEMAND         (1 << 6)
#define ARCH_VMM_MAP_DISABLED       (1 << 7)
#define ARCH_VMM_MAP_VIDEO_MEMORY   (1 << 8)
#define ARCH_VMM_MAP_WRITE_THROUGH  (1 << 9)

#define ARCH_VMM_MAP_HUGETLB        (1 << 10)
#define ARCH_VMM_MAP_HUGE_2MB       (0 << 11)
#define ARCH_VMM_MAP_HUGE_1GB       (1 << 11)

#define ARCH_VMM_MAP_TYPE_MASK      (3 << 12)
#define ARCH_VMM_MAP_TYPE_PAGE      (0 << 12)
#define ARCH_VMM_MAP_TYPE_MMAP      (1 << 12)
#define ARCH_VMM_MAP_TYPE_COW       (2 << 12)


#define ARCH_VMM_CLONE_DEMAND       (1 << 1)
#define ARCH_VMM_CLONE_USERSPACE    (1 << 2)

#define ARCH_VMM_CLONE_NEW_SPACE    (0)




typedef struct {

    uintptr_t start;
    uintptr_t end;
    uintptr_t fd;
    uintptr_t offset;

} mmap_mapping_t;


typedef struct vmm_address_space {

    uintptr_t pm;
    size_t size;
    size_t refcount;
    
    struct {

        uintptr_t heap_start;
        uintptr_t heap_end;

        mmap_mapping_t mappings[CONFIG_MMAP_MAX];

    } mmap;

    spinlock_t lock;

} vmm_address_space_t;



__BEGIN_DECLS

void pmm_claim_area(uintptr_t, size_t);
void pmm_unclaim_area(uintptr_t, size_t);
uintptr_t pmm_alloc_block();
uintptr_t pmm_alloc_blocks(size_t);
uintptr_t pmm_alloc_blocks_aligned(size_t, uintptr_t);
void pmm_free_block(uintptr_t);
void pmm_free_blocks(uintptr_t, size_t);
uint64_t pmm_get_used_memory();
uint64_t pmm_get_total_memory();
void pmm_init(uintptr_t);


void* kmalloc(size_t, int)         __malloc __alloc_size(1);
void* kcalloc(size_t, size_t, int) __malloc __alloc_size(1, 2);
void* krealloc(void*, size_t, int) __malloc __alloc_size(2);
void kfree(void*);

uint64_t kheap_get_used_memory();

__END_DECLS

#endif
#endif
