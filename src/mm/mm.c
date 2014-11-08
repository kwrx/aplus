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


#include <aplus/mm.h>
#include <aplus/list.h>

#include <grub.h>



uint32_t memsize;
volatile heap_t* current_heap;

extern uint32_t* current_vmm;
extern uint32_t* kernel_vmm;

extern list_t* vmm_queue;


typedef struct block {
	uint16_t magic;
	size_t size;
} __attribute__((packed)) block_t;


void* kmalloc(size_t size) {
	void* addr = (void*) halloc(current_heap, size);
	if(!addr)
		panic("halloc(): failed!");


	addr = mm_vaddr(addr);

	block_t* block = (block_t*) addr;
	block->magic = BLKMAGIC;
	block->size = size;

	return (void*) ((uint32_t) block + sizeof(block_t));
}


void kfree(void* ptr) {
	if(!ptr)
		return;
		
	
	block_t* block = (block_t*) ptr;
	if(block->magic != BLKMAGIC)
		return;
		
	size_t size = block->size;
	block->size = 0;
	block->magic = 0;
	
	
	hfree(current_heap, mm_paddr(mm_align(ptr)), size);
}


void* krealloc(void* ptr, size_t size) {
	if(ptr == NULL)
		return kmalloc(size);
		
	if(size == 0) {
		kfree(ptr);
		return NULL;
	}	

	block_t* block = (block_t*) ptr;
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




void mm_setheap(heap_t* heap) {
	current_heap = heap;
}

volatile heap_t* mm_getheap() {
	return current_heap;
}


int mm_init() {

	memsize = (mbd->mem_upper + mbd->mem_lower) * 1024;
	if(memsize > VMM_MAX_MEMORY)
		memsize = VMM_MAX_MEMORY;


	kheap_init();
	vmm_init();

	return 0;
}
