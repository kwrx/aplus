//
//  paging.c
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
#include <string.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/list.h>

#include <grub.h>

#define PGSIZE		(BLKSIZE)
#define PGFLAGS		(0x00000000)

#define PDSIZE		(1024)
#define PDENTRY(x)	((uint32_t) x >> 22)

#define PTSIZE		(1024)
#define PTENTRY(x)	((uint32_t) x << 10 >> 10 >> 12)

extern volatile heap_t* current_heap;
extern uint32_t memsize;

uint32_t* current_vmm;
uint32_t* kernel_vmm;

list_t* vmm_queue;




void vmm_switch(uint32_t* addr) {
	current_vmm = addr;
	
	write_cr3((uint32_t) mm_paddr((void*) addr));
}

void vmm_enable() {
	write_cr4(read_cr4() & ~0x00000010);
	write_cr0(read_cr0() | 0x80000000);
}

void vmm_disable() {
	write_cr0(read_cr0() & ~0x80000000);
}




void* vmm_map(uint32_t* pd, void* paddr, void* vaddr, size_t len, int flags) {	
	if(!pd)
		return paddr;
		
	paddr = mm_align(paddr);
	vaddr = mm_align(vaddr);
		
	int pages = (len / PGSIZE) + 1;
	uint32_t pframe = (uint32_t) paddr;
	uint32_t vframe = (uint32_t) vaddr;
	
	for(int i = 0; i < pages; i++) {
		uint32_t* e = &pd[PDENTRY(vframe)];
		
		if(*e == 0) {
			uint32_t* table = (uint32_t*) halloc(current_heap, PTSIZE * sizeof(uint32_t));
			if(!table)
				panic("vmm_map(): cannot allocate more table\n");
				
			if(current_vmm)
				vmm_map(current_vmm, table, table, PTSIZE * sizeof(uint32_t), VMM_FLAGS_DEFAULT);
			
			memset(table, 0, PTSIZE * sizeof(uint32_t));
			*e = (uint32_t) table | flags;	
		}
		
		uint32_t* t = (uint32_t*) (*e & ~0xFFF);
		t[PTENTRY(vframe)] = pframe | flags;
		
		pframe += PGSIZE;
		vframe += PGSIZE;
	}
	
	return vaddr;
}


void vmm_umap(uint32_t* pd, void* addr, size_t len) {
	if(!pd)
		return;
		
	addr = mm_align(addr);
		
	int pages = (len / PGSIZE) + 1;
	
	
	for(uint32_t i = 0, frame = (uint32_t) addr; i < pages; i++, frame += PGSIZE) {
		uint32_t* e = &pd[PDENTRY(frame)];
		if((*e & 1) != 1)
			continue;
			
		uint32_t* table = (uint32_t*) (*e & ~0xFFF);
		table[PTENTRY(frame)] = 0;
	}
	
}


void* vmm_alloc(void* vaddr, size_t size, int flags) {
	void* paddr = (void*) halloc(current_heap, size);

	vmm_map(current_vmm, paddr, vaddr, size, flags);

	return paddr;
}

void vmm_mapkernel(uint32_t* dest) {
	// Map 8MB to low area (kernel reserved)
	vmm_map(dest, (void*) MM_LBASE, (void*) MM_LBASE, MM_LSIZE, VMM_FLAGS_DEFAULT);

	// Map all high-memory (kernel reserved)
	vmm_map(dest, (void*) 0, mm_vaddr((void*) 0), memsize, VMM_FLAGS_DEFAULT);

	// Map Linear Frame Buffer
	vmm_map(dest, (void*) 0xE0000000, (void*) 0xE0000000, 0x10000000, VMM_FLAGS_DEFAULT | VMM_FLAGS_USER);
}



uint32_t* vmm_create() {
	uint32_t* addr = (uint32_t*) mm_align((void*) kmalloc(PDSIZE * sizeof(uint32_t)));
	if(!addr)
		return NULL;
		

	memset(addr, 0, PDSIZE * sizeof(uint32_t));
	return addr;
}

void vmm_destroy(uint32_t* vmm) {
	memset(vmm, 0, PDSIZE * sizeof(uint32_t));
}

int vmm_init() {

	kernel_vmm = halloc(current_heap, PDSIZE * sizeof(uint32_t));
	if(!kernel_vmm)
		panic("Could not initialize VMM");
	

	memset(kernel_vmm, 0, PDSIZE * sizeof(uint32_t));
	
	vmm_mapkernel(kernel_vmm);
	vmm_switch(kernel_vmm);
	vmm_enable();

	list_init(vmm_queue);
	return 0;
}



