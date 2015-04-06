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

#ifdef __i386__

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/list.h>

#include "i386.h"


#define PGSIZE		(0x1000)
#define PGSIZE_4MB	(PGSIZE * 1024)
#define PGFLAGS		(0x00000000)

#define PDSIZE		(1024)
#define PDENTRY(x)	((uint32_t) x >> 22)

#define PTSIZE		(1024)
#define PTENTRY(x)	((uint32_t) x << 10 >> 10 >> 12)

extern volatile heap_t* current_heap;
extern uint32_t memsize;
extern uint32_t kernel_low_area_size;

extern task_t* kernel_task;
extern task_t* current_task;

uint32_t* current_vmm;
uint32_t* kernel_vmm;



__attribute__((aligned(0x1000)))
static uint32_t __kvmm[PDSIZE];



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
	if(unlikely(!pd))
		return paddr;

#ifdef DEBUG
	if(unlikely(!flags))
		panic("vmm_map() flags cannot be null");
#endif
		
	paddr = mm_align(paddr);
	vaddr = mm_align(vaddr);
		
	int pages = (len / PGSIZE) + 1;
	uint32_t pframe = (uint32_t) paddr;
	uint32_t vframe = (uint32_t) vaddr;

	
	for(int i = 0; i < pages; i++) {
		uint32_t* e = &pd[PDENTRY(vframe)];
		
		if(*e == 0) {
			uint32_t* table = (uint32_t*) halloc(current_heap, PTSIZE * sizeof(uint32_t));
			if(unlikely(!table)) {
				kprintf("vmm_map(): cannot allocate more table\n");
				return NULL;
			}				

			if(likely(current_vmm))
				vmm_map(current_vmm, table, table, PTSIZE * sizeof(uint32_t), VMM_FLAGS_DEFAULT);
			
			memset(table, 0, PTSIZE * sizeof(uint32_t));
			*e = (uint32_t) table | flags;	
		}
		
		uint32_t* t = (uint32_t*) (*e & VMM_MASK);
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
			
		uint32_t* table = (uint32_t*) (*e & VMM_MASK);
		table[PTENTRY(frame)] = 0;
	}
	
}


void* vmm_alloc(uint32_t* vmm, void* vaddr, size_t size, int flags) {
	void* paddr = (void*) halloc(current_heap, size);
	if(!paddr)
		return NULL;

	vmm_map(vmm, paddr, vaddr, size, flags);
	return paddr;
}

void vmm_free(uint32_t* vmm, void* vaddr, size_t size) {

	int pages = (size / PGSIZE) + 1;
	uint32_t frame = (uint32_t) vaddr;
	uint32_t* pd = (uint32_t*) current_vmm;

	for(int i = 0; i < pages; i++) {
		uint32_t* e = &pd[PDENTRY(frame)];

		if((*e & 1) != 1)
			continue;

		uint32_t* table = (uint32_t*) (*e & VMM_MASK);
		hfree(current_heap, table[PTENTRY(frame)] & VMM_MASK, PGSIZE);
	}

	vmm_umap(vmm, vaddr, size);
}

void vmm_mapkernel(uint32_t* dest) {
	memcpy(dest, kernel_vmm, PDSIZE * sizeof(uint32_t));

	/*
	// Map low area (Kernel reserved)
	//if(unlikely(dest == kernel_vmm))
		vmm_map(dest, (void*) MM_LBASE, (void*) MM_LBASE, MM_LSIZE, VMM_FLAGS_DEFAULT | VMM_FLAGS_USER);
	//else
	//	vmm_map(dest, (void*) MM_LBASE, (void*) MM_LBASE, MM_LSIZE, VMM_FLAGS_DEFAULT);

	// Map all high-memory (Shared Address Space)
	vmm_map(dest, (void*) 0, mm_vaddr((void*) 0), memsize, VMM_FLAGS_DEFAULT | VMM_FLAGS_USER);

	// Map Linear Frame Buffer
	vmm_map(dest, (void*) mbd->lfb.base, (void*) mbd->lfb.base, mbd->lfb.size, VMM_FLAGS_DEFAULT | VMM_FLAGS_USER);
	*/
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






static uint32_t __clone_table(uint32_t* tsrc) {
	uint32_t* tdst = (uint32_t*) kmalloc(PTSIZE * sizeof(uint32_t));
	memset(tdst, 0, PTSIZE * sizeof(uint32_t));

	for(int i = 0; i < 1024; i++) {
		if(likely(tsrc[i])) {
			tdst[i] = (uint32_t) halloc(current_heap, PTSIZE * sizeof(uint32_t));
			if(unlikely(!tdst[i])) {
				kprintf("__clone_table(): cannot allocate more table\n");
				return 0;
			}


			if(likely(current_vmm))
				vmm_map(current_vmm, (uint32_t*) tdst[i], (uint32_t*) tdst[i], PTSIZE * sizeof(uint32_t), VMM_FLAGS_DEFAULT);
			
			memcpy((void*) tdst[i], (void*) (tsrc[i] & VMM_MASK), PTSIZE * sizeof(uint32_t));
			tdst[i] |= tsrc[i] & ~(VMM_MASK);
		}
	}

	return (uint32_t) tdst;
}

uint32_t* vmm_clone(uint32_t* dest, uint32_t* src) {
	if(!dest)
		dest = vmm_create();
	
	for(int i = 0; i < 1024; i++) {
		if(src[i] == 0)
			continue;

		if(src[i] == kernel_vmm[i])
			dest[i] = src[i];
		else
			dest[i] = __clone_table((uint32_t*) (src[i] & VMM_MASK)) | (src[i] & ~(VMM_MASK));
	}

	return dest;
}



int vmm_init() {

	kernel_vmm = __kvmm;
	memset(kernel_vmm, 0, PDSIZE * sizeof(uint32_t));

	vmm_map(kernel_vmm, (void*) MM_LBASE, (void*) MM_LBASE, MM_LSIZE, VMM_FLAGS_DEFAULT | VMM_FLAGS_USER);
	vmm_map(kernel_vmm, (void*) 0, mm_vaddr((void*) 0), memsize, VMM_FLAGS_DEFAULT | VMM_FLAGS_USER);
	vmm_map(kernel_vmm, (void*) mbd->lfb.base, (void*) mbd->lfb.base, mbd->lfb.size, VMM_FLAGS_DEFAULT | VMM_FLAGS_USER);

	
	//vmm_mapkernel(kernel_vmm);
	vmm_switch(kernel_vmm);
	vmm_enable();

	return 0;
}


EXPORT_SYMBOL(vmm_alloc);
EXPORT_SYMBOL(vmm_free);
EXPORT_SYMBOL(vmm_clone);

#endif

