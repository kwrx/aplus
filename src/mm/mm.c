//
//  mm.c
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/list.h>
#include <aplus/task.h>

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <errno.h>



uint32_t memsize;
uint32_t kernel_low_area_size;

heap_t* current_heap;

extern uint32_t* current_vmm;
extern uint32_t* kernel_vmm;

extern task_t* current_task;
extern task_t* kernel_task;

extern list_t* vmm_queue;


typedef struct block {
	uint16_t magic;
	size_t size;
} __attribute__((packed)) block_t;



#ifdef MM_DEBUG
void* __kmalloc(size_t size, char* file, int line) {
#else
void* kmalloc(size_t size) {
#endif


	void* addr = (void*) halloc(current_heap, size + sizeof(block_t));
	if(unlikely(!addr)) {

#ifdef MM_DEBUG
		kprintf("kmalloc: cannot allocate RAM for %d bytes (%s:%d)\n", size, file, line);
#endif

		errno = ENOMEM;
		return NULL;
	}


	addr = mm_vaddr(addr);

	block_t* block = (block_t*) addr;
	block->magic = BLKMAGIC;
	block->size = size;

	return (void*) ((uint32_t) block + sizeof(block_t));
}


void* malloc(size_t size) {
	return kmalloc(size);
}

void* kvmalloc(size_t size) {
	void* addr = (void*) halloc(current_heap, size + sizeof(block_t));
	if(unlikely(!addr))
		panic("kvmalloc: halloc() failed!");


	addr = mm_vaddr(addr);
	return addr;
}

/**
 *	\brief Free memory from Kernel Heap.
 * 	\param ptr Pointer to data allocated.
 */
void kfree(void* ptr) {
	if(unlikely(!ptr))
		return;
		
	
	block_t* block = (block_t*) ptr;
	block--;

	if(unlikely(block->magic != BLKMAGIC))
		return;


	size_t size = block->size;
	block->size = 0;
	block->magic = 0;
	
	hfree(current_heap, mm_paddr(mm_align(ptr)), size + sizeof(block_t));
}


void free(void* ptr) {
	kfree(ptr);
}

/**
 *	\brief Realloc memory from Kernel Heap.
 * 	\param ptr Pointer to data allocated.
 * 	\param size Size of data to alloc.
 *	\return Virtual Address of data.
 */
void* krealloc(void* ptr, size_t size) {
	if(ptr == NULL)
		return kmalloc(size);
		
	if(size == 0) {
		kfree(ptr);
		return NULL;
	}	

	block_t* block = (block_t*) ptr;
	block--;

	if(unlikely(block->magic != BLKMAGIC))
		return NULL;
		
	void* newptr = kmalloc(size);
	if(unlikely(!newptr))
		return NULL;
		
	if(unlikely(size > block->size))
		size = block->size;
		
	memcpy(newptr, ptr, size);
	kfree(ptr);
	
	return newptr;
}



/**
 *	\brief Set current heap for memory operations.
 */
void mm_setheap(heap_t* heap) {
	current_heap = heap;
}

/**
 *	\brief Get current heap for memory operations.
 */
heap_t* mm_getheap() {
	return current_heap;
}


/**
 *	\brief Initialize MMU.
 */
int mm_init() {

	memsize = (uint32_t) mbd->memory.size;
	if(memsize > VMM_MAX_MEMORY)
		memsize = VMM_MAX_MEMORY;

	kernel_low_area_size = mbd->memory.start;
	kernel_low_area_size &= ~0xFFFFF;
	kernel_low_area_size += 0x100000;

	kprintf("mm: low area size %d MB\n", kernel_low_area_size / 1024 / 1024);
	kprintf("mm: memory size %d MB\n", memsize / 1024 / 1024);

	kheap_init();
	vmm_init();

	return 0;
}


EXPORT_SYMBOL(kmalloc);
EXPORT_SYMBOL(krealloc);
EXPORT_SYMBOL(kfree);
