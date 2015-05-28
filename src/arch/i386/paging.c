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

#include <arch/i386/i386.h>


#define PGSIZE		(0x1000)
#define PGSIZE_4MB	(PGSIZE * 1024)
#define PGFLAGS		(0x00000000)

#define PDSIZE		(1024)
#define PDENTRY(x)	((uint32_t) x >> 22)

#define PTSIZE		(1024)
#define PTENTRY(x)	((uint32_t) x << 10 >> 10 >> 12)


#define X86_MMU_PRESENT			1
#define X86_MMU_RDWR			2
#define X86_MMU_USER			4
#define X86_MMU_WRITETROUGH		8
#define X86_MMU_NOTCACHED		16
#define X86_MMU_ACCESSED		32
#define X86_MMU_PAGESIZE		64


extern volatile heap_t* current_heap;
extern uint32_t memsize;
extern uint32_t kernel_low_area_size;

extern task_t* kernel_task;
extern task_t* current_task;

uint32_t* current_vmm = NULL;
uint32_t* kernel_vmm = NULL;

static int vmm_enabled = 0;


__attribute__((aligned(0x1000)))
static uint32_t __kvmm[PDSIZE];


static int vmm_flags(int flags) {
	register int r = 0;

	if(flags & VMM_PROT_READ)
		r |= X86_MMU_PRESENT;

	if(flags & VMM_PROT_WRITE)
		r |= X86_MMU_RDWR;

	if(flags & VMM_PROT_EXEC)
		r |= X86_MMU_USER;

	return r;
}


void* vmm_p2v(uint32_t* pd, void* phys) {
	(void) pd;

	if((uint32_t) phys < MM_VBASE)
		return (void*) ((uint32_t) phys + MM_VBASE);

	return phys;
}

void* vmm_v2p(uint32_t* pd, void* virt) {
	if(unlikely(!pd))
		return NULL;

	uint32_t vframe = (uint32_t) mm_align(virt);
	if(pd[PDENTRY(vframe)] == 0)
		return NULL;

	uint32_t* t = (uint32_t*) (pd[PDENTRY(vframe)] & VMM_MASK);
	if(likely(vmm_enabled))
		t = vmm_p2v(pd, t);

	return (void*) (t[PTENTRY(vframe)] & VMM_MASK);
}


void vmm_switch(uint32_t* addr) {
	current_vmm = addr;
	
	write_cr3((uint32_t) mm_paddr((void*) addr));
}


void vmm_enable() {
	write_cr4(read_cr4() & ~0x00000010);
	write_cr0(read_cr0() | 0x80000000);

	vmm_enabled = 1;
}

void vmm_disable() {
	write_cr0(read_cr0() & ~0x80000000);
	
	vmm_enabled = 0;
}


void vmm_flush(void* addr) {
	if(addr)
		__asm__ __volatile__ ("invlpg [%0]" : : "r" (addr) : "memory");
	else
		write_cr3(read_cr3());
}


int vmm_accessed(uint32_t* pd, void* vaddr) {
	if(unlikely(!pd && current_vmm))
		pd = current_vmm;
	else
		return -1;
	

	vaddr = mm_align(vaddr);
	uint32_t vframe = (uint32_t) vaddr;

	uint32_t* e = &pd[PDENTRY(vframe)];
	if(*e == 0)
		return -1;

	uint32_t* t = (uint32_t*) (*e & VMM_MASK);
	if(t == 0)
		return -1;

	if(likely(vmm_enabled))
		t = (uint32_t*) mm_vaddr(t);


	register int f = t[PTENTRY(vframe)] & X86_MMU_ACCESSED;
	t[PTENTRY(vframe)] &= ~X86_MMU_ACCESSED;
	
	return f ? 1 : 0;
}


void* vmm_map(uint32_t* pd, void* paddr, void* vaddr, size_t len, int flags) {	
	if(unlikely(!pd))
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
			if(unlikely(!table)) {
				kprintf("vmm_map(): cannot allocate more table\n");
				return NULL;
			}

			
			if(likely(vmm_enabled))
				table = (uint32_t*) mm_vaddr(table);

			
			memset(table, 0, PTSIZE * sizeof(uint32_t));
			*e = (uint32_t) mm_paddr(table) | vmm_flags(VMM_PROT_READ | VMM_PROT_WRITE | VMM_PROT_EXEC);	
		}
		
		uint32_t* t = (uint32_t*) (*e & VMM_MASK);
		if(likely(vmm_enabled))
			t = (uint32_t*) mm_vaddr(t);

		t[PTENTRY(vframe)] = pframe | vmm_flags(flags);
		
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
		if(likely(vmm_enabled))
			table = (uint32_t*) mm_vaddr(table);

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
		if(likely(vmm_enabled))
			table = (uint32_t*) mm_vaddr(table);

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
	uint32_t* addr = (uint32_t*) kvmalloc(PDSIZE * sizeof(uint32_t));
	if(!addr)
		return NULL;
		

	memset(addr, 0, PDSIZE * sizeof(uint32_t));
	return addr;
}

void vmm_destroy(uint32_t* vmm) {
	memset(vmm, 0, PDSIZE * sizeof(uint32_t));
}






static uint32_t __clone_table(uint32_t* tsrc, int flags) {
	uint32_t* tdst = (uint32_t*) kvmalloc(PTSIZE * sizeof(uint32_t));
	memset(tdst, 0, PTSIZE * sizeof(uint32_t));

	for(int i = 0; i < 1024; i++) {
		if(likely(tsrc[i])) {
			uint32_t* table;
			tdst[i] = (uint32_t) halloc(current_heap, PTSIZE * sizeof(uint32_t));
			if(unlikely(!tdst[i])) {
				kprintf("__clone_table(): cannot allocate more table\n");
				return 0;
			}

			table = (uint32_t*) tdst[i];
			if(likely(vmm_enabled))
				table = (uint32_t*) mm_vaddr(table);
			
			memcpy(table, (void*) tsrc[i], PTSIZE * sizeof(uint32_t));
			tdst[i] |= flags;
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
			dest[i] = __clone_table((uint32_t*) (src[i] & VMM_MASK), src[i] & ~(VMM_MASK));
	}

	return dest;
}



int vmm_init() {

	kernel_vmm = __kvmm;
	memset(kernel_vmm, 0, PDSIZE * sizeof(uint32_t));

	vmm_map(kernel_vmm, (void*) MM_LBASE, (void*) MM_LBASE, MM_LSIZE, VMM_PROT_READ | VMM_PROT_WRITE | VMM_PROT_EXEC);
	vmm_map(kernel_vmm, (void*) 0, (void*) mm_vaddr((void*) 0), memsize, VMM_PROT_READ | VMM_PROT_WRITE | VMM_PROT_EXEC);
	vmm_map(kernel_vmm, (void*) 0xE0000000, (void*) 0xE0000000, 0x1F000000, VMM_PROT_READ | VMM_PROT_WRITE | VMM_PROT_EXEC);

	if(likely(mbd->lfb.base && mbd->lfb.size))
		vmm_map(kernel_vmm, (void*) mbd->lfb.base, (void*) mbd->lfb.base, mbd->lfb.size, VMM_PROT_READ | VMM_PROT_WRITE | VMM_PROT_EXEC);

	
	//vmm_mapkernel(kernel_vmm);
	vmm_switch(kernel_vmm);
	vmm_enable();

	return 0;
}




void pagefault_handler(regs_t* r) {
	uint32_t faultaddr = read_cr2();
	if(mmfault(faultaddr) == MM_OK)
		return;

#ifdef DEBUG
	kprintf("mmu: Page fault at 0x%x (%x, %s, %s, %s, %s, %s)\n",
		faultaddr,
		r->err_code,
		r->err_code & 0x01 ? "P" : "N/A",
		r->err_code & 0x02 ? "W" : "R",
		r->err_code & 0x04 ? "U" : "S",
		r->err_code & 0x08 ? "RSVD" : "\b\b",
		r->err_code & 0x10 ? "I/D" : "\b\b" 
	);
#endif

	 
	arch_panic("Page fault", r);
}




EXPORT_SYMBOL(vmm_alloc);
EXPORT_SYMBOL(vmm_free);
EXPORT_SYMBOL(vmm_clone);
EXPORT_SYMBOL(vmm_switch);
EXPORT_SYMBOL(vmm_accessed);
EXPORT_SYMBOL(vmm_flush);
EXPORT_SYMBOL(vmm_p2v);
EXPORT_SYMBOL(vmm_v2p);

#endif

