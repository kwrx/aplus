#ifdef __rpi__

#include <aplus.h>
#include <aplus/task.h>
#include <aplus/mm.h>
#include <arch/rpi/rpi.h>

#include <stdint.h>
#include <stddef.h>


#define PGSIZE		(0x1000)
#define PGSIZE_4MB	(PGSIZE * 1024)
#define PGFLAGS		(0x00000000)

#define PDSIZE		(4096)
#define PDENTRY(x)	((uint32_t) x >> 20)

#define PTSIZE		(1024)
#define PTENTRY(x)	(((uint32_t) x >> 12) & 0xFF)


#define TTE_NP		0x000
#define TTE_CP		0x001
#define TTE_TM		0x003
#define TTE_MASK	0xFFFFFC00

#define PTE_NP		0x000
#define PTE_SP		0x002
#define PTE_TM		0x003
#define PTE_KRNL	0x550
#define PTE_USER	0xFF0

#define PTE_MASK	0xFFFFF000


extern volatile heap_t* current_heap;
extern uint32_t memsize;
extern uint32_t kernel_low_area_size;

extern task_t* kernel_task;
extern task_t* current_task;

uint32_t* kernel_vmm = NULL;
uint32_t* current_vmm = NULL;


__attribute__((aligned(0x1000)))
static uint32_t __kvmm[PDSIZE];


#define inv_tlb()			\
	__asm__ __volatile__ ("mcr p15, 0, %0, c8, c7, 0" : : "r"(0))

#define inv_tlb_entry(a)	\
	__asm__ __volatile__ ("mcr p15, 0, %0, c8, c7, 1" : : "r"(a))


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
		
		if((*e & TTE_TM) == TTE_NP) {
			uint32_t* table = (uint32_t*) halloc(current_heap, PTSIZE * sizeof(uint32_t));
			if(unlikely(!table)) {
				kprintf("vmm_map(): cannot allocate more table\n");
				return NULL;
			}				

			if(likely(current_vmm))
				vmm_map(current_vmm, table, table, PTSIZE * sizeof(uint32_t), VMM_FLAGS_DEFAULT);
			
			memset(table, 0, PTSIZE * sizeof(uint32_t));
			*e = (uint32_t) table | TTE_CP;	
		}
		
		uint32_t* t = (uint32_t*) (*e & TTE_MASK);
		t[PTENTRY(vframe)] = pframe | PTE_SP | flags;	

		inv_tlb_entry(vframe);
		
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
		if((*e & TTE_CP) != TTE_CP)
			continue;
			
		uint32_t* table = (uint32_t*) (*e & TTE_MASK);
		table[PTENTRY(frame)] = 0;

		inv_tlb_entry(frame);
	}
	
}

void vmm_mapkernel(uint32_t* dest) {
	// Map low area (Kernel reserved)
	//if(unlikely(dest == kernel_vmm))
		vmm_map(dest, (void*) MM_LBASE, (void*) MM_LBASE, MM_LSIZE, VMM_FLAGS_DEFAULT | VMM_FLAGS_USER);
	//else
	//	vmm_map(dest, (void*) MM_LBASE, (void*) MM_LBASE, MM_LSIZE, VMM_FLAGS_DEFAULT);

	// Map all high-memory (Shared Address Space)
	vmm_map(dest, (void*) 0, mm_vaddr((void*) 0), memsize, VMM_FLAGS_DEFAULT | VMM_FLAGS_USER);

	// Map Linear Frame Buffer
	vmm_map(dest, (void*) mbd->lfb.base, (void*) mbd->lfb.base, mbd->lfb.size, VMM_FLAGS_DEFAULT | VMM_FLAGS_USER);

	/* Map peripherals */
	vmm_map(dest, (void*) 0x20000000, (void*) 0x20000000, 0x400000, VMM_FLAGS_DEFAULT);
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

		if((*e & TTE_CP) != TTE_CP)
			continue;

		uint32_t* table = (uint32_t*) (*e & VMM_MASK);
		hfree(current_heap, table[PTENTRY(frame)] & VMM_MASK, PGSIZE);
	}

	vmm_umap(vmm, vaddr, size);
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


uint32_t* vmm_clone(uint32_t* dest, uint32_t* src) {
	return NULL;
}

void vmm_switch(uint32_t* vmm) {
	__asm__ __volatile__ ("mcr p15, 0, %0, c2, c0, 0" : : "r"(vmm));
	inv_tlb();
}

void vmm_enable() {
	__asm__ __volatile__ (
		"mcr p15, 0, %0, c3, c0"
		: : "r"(0x55555555)
	);

	__asm__ __volatile__ (
		"mrc p15, 0, r0, c1, c0	\n"
		"bic r0, #0x0004		\n"
		"bic r0, #0x1000		\n"
		"mcr p15, 0, r0, c1, c0	\n"
		: : : "r0"
	);

	__asm__ __volatile__ (
		"mrc p15, 0, r0, c1, c0	\n"
		"orr r0, #0x00000001	\n"
		"bic r0, #0x00800000	\n"
		"mcr p15, 0, r0, c1, c0	\n"
		"mcr p15, 0, %0, c7, c5, 4 \n"
		: : "r"(0) : "r0"
	);
}

int vmm_init() {
	kernel_vmm = __kvmm;
	memset(kernel_vmm, 0, PDSIZE * sizeof(uint32_t));

	
	//vmm_mapkernel(kernel_vmm);
	//vmm_switch(kernel_vmm);
	//vmm_enable();


	return 0;
}


EXPORT_SYMBOL(vmm_alloc);
EXPORT_SYMBOL(vmm_free);
EXPORT_SYMBOL(vmm_clone);

#endif
