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
                                                                        
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/hal.h>




#define PTR_TO_HEADER(p)    \
    ((struct kmalloc_header*) ((uintptr_t) p - sizeof(struct kmalloc_header)))



struct kmalloc_header {

    union {
        struct {
            char magic[4];
            uint32_t size;
        };

        uint64_t __padding[2];
    };

    char ptr[0];
} __packed;




__malloc
__alloc_size(1)
void* kmalloc(size_t size, int gfp) {

    DEBUG_ASSERT(size);


    size += sizeof(struct kmalloc_header);

    if(size & (PML1_PAGESIZE - 1))
        size = (size & ~(PML1_PAGESIZE - 1)) + PML1_PAGESIZE;


    struct kmalloc_header* h;

    if(size == PML1_PAGESIZE)
        h = (struct kmalloc_header*) arch_vmm_p2v(pmm_alloc_block(), ARCH_VMM_AREA_HEAP);
    else
        h = (struct kmalloc_header*) arch_vmm_p2v(pmm_alloc_blocks(size / PML1_PAGESIZE), ARCH_VMM_AREA_HEAP);


    DEBUG_ASSERT(memcmp(h->magic, "USED", 4) != 0);

    h->magic[0] = 'U';
    h->magic[1] = 'S';
    h->magic[2] = 'E';
    h->magic[3] = 'D';

    h->size = (size / PML1_PAGESIZE);


    return (void*) &h->ptr;
    
}


__malloc
__alloc_size(1, 2)
void* kcalloc(size_t n, size_t m, int gfp) {

    DEBUG_ASSERT(n);
    DEBUG_ASSERT(m);

    void* h = kmalloc(n * m, gfp);
    if(likely(h))
        memset(h, 0, n * m);

    return h;

}


__malloc
__alloc_size(2)
void* krealloc(void* address, size_t size, int gfp) {

    DEBUG_ASSERT(address || size);

    if(address) {

        DEBUG_ASSERT(memcmp(PTR_TO_HEADER(address)->magic, "USED", 4) == 0);

        if(!size)
            return kfree(address), NULL;
        

        void* p = kmalloc(size, gfp);
        if(unlikely(!p))
            return NULL;

        memcpy(p, address, PTR_TO_HEADER(address)->size);
        
        kfree(address);
        return p;

    }
     
     
    return kmalloc(size, gfp);

}


void kfree(void* address) {

    DEBUG_ASSERT(address);
    DEBUG_ASSERT(memcmp(PTR_TO_HEADER(address)->magic, "USED", 4) == 0);

    PTR_TO_HEADER(address)->magic[0] = 'F';
    PTR_TO_HEADER(address)->magic[1] = 'R';
    PTR_TO_HEADER(address)->magic[2] = 'E';
    PTR_TO_HEADER(address)->magic[3] = 'E';

    pmm_free_blocks(arch_vmm_v2p((uintptr_t) PTR_TO_HEADER(address), ARCH_VMM_AREA_HEAP), PTR_TO_HEADER(address)->size);

}