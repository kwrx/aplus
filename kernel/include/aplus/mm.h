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


#ifndef _MM_H
#define _MM_H

#define KMALLOC_MAGIC                        0xCAFE1234

#define __GFP_HIGH                            1
#define __GFP_WAIT                            2

#define GFP_ATOMIC                            (__GFP_HIGH)
#define GFP_KERNEL                            (__GFP_HIGH | __GFP_WAIT)
#define GFP_USER                            (__GFP_WAIT)

#define HF_NULL                                (0)
#define HF_ZERO                                (1)

#define MM_POOL_SIZE                        131072
#define MM_BLOCKSZ                            4096

#define PAGE_SIZE                            (0x1000)


#define P2V(x)                                ((x) + CONFIG_KERNEL_BASE)
#define V2P(x)                                __V2P((virtaddr_t) (x))


#define KCACHE_FREE_ALL                        0
#define KCACHE_FREE_BLOCKS                    1
#define KCACHE_FREE_OLDEST                    2


#ifndef __ASSEMBLY__

#include <aplus.h>
#include <aplus/ipc.h>
#include <aplus/timer.h>
#include <aplus/vfs.h>
#include <libc.h>

typedef uintptr_t physaddr_t;
typedef uintptr_t virtaddr_t;
typedef uint64_t kcache_index_t;

typedef struct {
    uintptr_t used;
    uintptr_t total;
    uint8_t* frames;
} mm_state_t;

typedef struct kcache_block {
    kcache_index_t index;
    void* ptr;
    size_t size;
    spinlock_t lock;
    struct kcache_block* next;
} kcache_block_t;

typedef struct kcache_pool {
    ktime_t last_access;
    size_t cachesize;
    struct kcache_block* blocks;
    struct kcache_pool* next;
} kcache_pool_t;


int slab_init(void);
int pmm_init(void);
int vmm_init(void);
int mm_init(void);

physaddr_t __V2P(virtaddr_t);

void enable_page(virtaddr_t virtaddr);
void disable_page(virtaddr_t virtaddr);
void unmap_page(virtaddr_t virtaddr);
void map_page(virtaddr_t virtaddr, physaddr_t physaddr, int user);
virtaddr_t get_free_pages(int count, int maked, int user);

int vmm_swap_setup(uintptr_t addr, off_t off, size_t size, inode_t* inode, int prot, int flags);
int vmm_swap_remove(uintptr_t addr, size_t size);
int vmm_swap_sync(uintptr_t addr, size_t size, int flags);
int vmm_swap(uintptr_t address);

physaddr_t pmm_alloc_frame(void);
physaddr_t pmm_alloc_frames(int count);
void pmm_free_frame(physaddr_t);
void pmm_free_frames(physaddr_t address, int count);
void pmm_claim(physaddr_t mstart, physaddr_t mend);


void* kmalloc(size_t, int);
void* kvalloc(size_t, int);
void* kcalloc(size_t, size_t, int);
void kfree(void*);

void* std_kmalloc(size_t);
void* std_kcalloc(size_t, size_t);
void std_kfree(void*);


mm_state_t* pmm_state(void);
mm_state_t* kvm_state(void);
mm_state_t* kcache_state(void);


size_t kcache_free(int mode);
void kcache_free_pool(kcache_pool_t* pool);
void kcache_register_pool(kcache_pool_t* pool);
void kcache_unregister_pool(kcache_pool_t* pool);
void kcache_free_block(kcache_pool_t* pool, kcache_index_t index);
int kcache_obtain_block(kcache_pool_t* pool, kcache_index_t index, void** ptr, size_t size);
void kcache_release_block(kcache_pool_t* pool, kcache_index_t index);


#endif

#endif
