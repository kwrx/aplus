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

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/list.h>
#include <aplus/task.h>

#include <grub.h>



/**
 *	\see aplus/mm.h
 */
#ifdef CHUNKS_CHECKING
#define CHUNK_MAGIC		0x12345678
#endif


uint32_t memsize;
heap_t* current_heap;

extern uint32_t* current_vmm;
extern uint32_t* kernel_vmm;

extern task_t* current_task;
extern task_t* kernel_task;

extern list_t* vmm_queue;


typedef struct block {
	uint16_t magic;
	size_t size;

#ifdef CHUNKS_CHECKING
	char* file;
	int line;
#endif
} __attribute__((packed)) block_t;




#ifdef CHUNKS_CHECKING
static void __check_all_chunks_overflow() {
	if(current_task != kernel_task)
		return;

	schedule_disable();
	vmm_disable();
	
	int e = 0;
	for(uint32_t frame = 0; frame < current_heap->size; frame += 0x1000) {
		block_t* block = (block_t*) frame;
		if(block->magic != BLKMAGIC)
			continue;


		if(*(uint32_t*) (frame + block->size + sizeof(block_t)) != CHUNK_MAGIC) {
			kprintf("mm: chunk overflow at 0x%x (%x) allocated from %s (%d)\n", frame, block->size, block->file, block->line);
			e++;

		 	*(uint32_t*) (frame + block->size + sizeof(block_t)) = CHUNK_MAGIC;
		}

		frame += block->size & ~0xFFF;
	}
	
	if(e)
		kprintf("There was %d chunks overflow\n", e);

	vmm_enable();
	schedule_enable();
}
#endif


/**
 *	\brief Alloc memory from Kernel Heap.
 * 	\param size Size of data to alloc.
 *	\return Virtual Address of data.
 */
#ifdef CHUNKS_CHECKING
void* __kmalloc(size_t size, char* file, int line)
#else
void* kmalloc(size_t size) 
#endif
{

	size += sizeof(block_t);

	void* addr = (void*) halloc(current_heap, size);
	if(!addr)
		panic("halloc(): failed!");


	addr = mm_vaddr(addr);

	block_t* block = (block_t*) addr;
	block->magic = BLKMAGIC;
	block->size = size;

#ifdef CHUNKS_CHECKING
	block->file = file;
	block->line = line;

	*(uint32_t*) ((uint32_t) block + size + sizeof(block_t)) = CHUNK_MAGIC;

	__check_all_chunks_overflow();
#endif

	return (void*) ((uint32_t) block + sizeof(block_t));
}


void* kvmalloc(size_t size) {
	void* addr = (void*) halloc(current_heap, size);
	if(!addr)
		panic("halloc(): failed!");


	addr = mm_vaddr(addr);
	return addr;
}

/**
 *	\brief Free memory from Kernel Heap.
 * 	\param ptr Pointer to data allocated.
 */
void kfree(void* ptr) {
	if(!ptr)
		return;
		
	
	block_t* block = (block_t*) ptr;
	block--;

	if(block->magic != BLKMAGIC)
		return;


	size_t size = block->size;
	block->size = 0;
	block->magic = 0;
	
#ifdef CHUNKS_CHECKING
	block->file = 0;
	block->line = 0;
#endif
	
	hfree(current_heap, mm_paddr(mm_align(ptr)), size);
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

	if(block->magic != BLKMAGIC)
		return NULL;
		
	void* newptr = kmalloc(size);
	if(!newptr)
		return NULL;
		
	if(size > block->size)
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

	memsize = (mbd->mem_upper + mbd->mem_lower) * 1024;
	if(memsize > VMM_MAX_MEMORY)
		memsize = VMM_MAX_MEMORY;


	kheap_init();
	vmm_init();

	return 0;
}
