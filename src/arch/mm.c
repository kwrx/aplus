
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

#include <stdint.h>
#include <stddef.h>
#include <aplus.h>
#include <aplus/spinlock.h>

#include <grub.h>

#define CHUNK_MAGIC		0x1234
#define CHUNK_SIZE		0x1000


extern uint32_t end_kernel;

typedef struct chunk {
	uint16_t magic;
	uint32_t size;
} __attribute__((packed)) chunk_t;


typedef uint32_t pdirectory_t;
typedef uint32_t ptable_t;

uint32_t mm_totalmemory = 0;
uint32_t mm_usedmemory = 0;

pdirectory_t* mm_directory = 0;
pdirectory_t* kernel_directory = 0;


void* mm_alloc(int count) {
	static uint32_t maddr = 0;
	uint32_t addr = maddr;
	
	maddr += (count * CHUNK_SIZE);
	mm_usedmemory += (count * CHUNK_SIZE);
		
	end_kernel = maddr;
	return (void*) addr;
}

void mm_free(void* block, int count) {
	mm_usedmemory -= (count * CHUNK_SIZE);
	
	/* Nothing */
}




void mm_enablepaging() {
	write_cr4(read_cr4() & ~0x00000010);
	write_cr0(read_cr0() | 0x80000000);
}

void mm_disablepaging() {
	write_cr0(read_cr0() & ~0x80000000);
}

void vmm_switch(void* pdir) {
	if(!pdir)
		return;
		
	pdir = (void*) ((uint32_t)pdir & ~0xFFF);
	
	mm_directory = pdir;
	write_cr3(mm_directory);
}


void vmm_map(pdirectory_t* p, void* phys, void* virt) {

	if(!p)
		return;

	virt = (void*)((uint32_t) virt & ~0xFFF);
	phys = (void*)((uint32_t) phys & ~0xFFF);
	
	uint32_t* e = &p[((uint32_t) virt) >> 22];
	
	if(*e == 0) {
		ptable_t* table = (ptable_t*) mm_alloc(1);
		if(!table)
			return;
			
		vmm_map(mm_directory, table, table);
		memset(table, 0, CHUNK_SIZE);
		
		*e = (uint32_t) table | 3;
	}
	
	ptable_t* t = (ptable_t*) (*e & ~0xFFF);
	t[((uint32_t) virt) << 10 >> 10 >> 12] = (uint32_t) phys | 3;
	
}

void vmm_unmap(pdirectory_t* p, void* virt) {

	if(!p)
		return;
		
	virt = (void*)((uint32_t) virt & ~0xFFF);

	
	uint32_t* e = &p[((((uint32_t) virt) >> 22) & 0x3FF)];
	if((*e & 1) != 1)
		return;
	
	ptable_t* table = (ptable_t*) (*e & ~0xFFF);
	table[((((uint32_t) virt) >> 12) & 0x3FF)] = 0;
}

pdirectory_t* mm_create_addrspace() {
	pdirectory_t* ret = mm_alloc(1);
	if(!ret)
		return 0;
		
		
	vmm_map(mm_directory, ret, ret);	
	memset(ret, 0, CHUNK_SIZE);
		
	return ret;
}

void mm_map_kernel(pdirectory_t* p) {
	if(!kernel_directory)
		return;
		
	memcpy(p, kernel_directory, CHUNK_SIZE);
}



void* kmalloc(size_t size) {
	if(size == 0)
		return 0;
		
	__lock	
		
	int chunks = size / 4096 + 1;
	
	void* ch = mm_alloc(chunks);
	for(int i = 0, frame = (uint32_t)ch; i < chunks; i++, frame += 4096)
		vmm_map(mm_directory, frame, frame);
							
	/* chunk_t* head = (chunk_t*) ((uint32_t)ch);
	head->magic = CHUNK_MAGIC;
	head->size = size;
	
	ch = (void*) ((uint32_t) head + sizeof(chunk_t)); */
				
	__unlock
	
	return ch;
}


void kfree(void* ptr) {
	if(!ptr)
		return;
		
	chunk_t* head = (chunk_t*) ((uint32_t) ptr - sizeof(chunk_t));
	if(head->magic != CHUNK_MAGIC)
		return;
		
	__lock
		
	int chunks = head->size / 4096 + 1;	
	head->magic = 0;
	head->size = 0;
	
	mm_free(head, chunks);
	
	for(int i = 0, frame = (uint32_t)head; i < chunks; i++, frame += 4096)
		vmm_unmap(mm_directory, frame);
		
	__unlock
}


int mm_init() {
	mm_totalmemory = (mbd->mem_lower + mbd->mem_upper) * 1024;
	mm_usedmemory = 0;

	mm_alloc(end_kernel / 4096 + 1);
	
	 /* Kernel Space: 0 ~ 2GB */
	kernel_directory = mm_create_addrspace();
	
	uint32_t i = 0;
	while(i < 0x80000000) {
		vmm_map(kernel_directory, i, i);
		i += 4096;
	}
	
	
	vmm_switch(kernel_directory);
	mm_enablepaging();
}


void pagefault_handler(regs_t* r) {
	uint32_t addr = read_cr2();
	
	int present = r->err_code & 1;
	int rw = r->err_code & 2;
	int us = r->err_code & 4;
	int reserved = r->err_code & 8;
	int id = r->err_code & 16;
	
	kprintf("Page fault (");
	if(!present)
		kprintf("not present, ");
		
	if(rw)
		kprintf("read-only, ");
		
	if(us)
		kprintf("user-mode, ");
		
	if(reserved)
		kprintf("reserved, ");
		
	kprintf("\b\b) at address 0x%x\n", addr);
	panic("Page fault");
}
