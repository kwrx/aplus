#ifdef __rpi__

#include <aplus.h>
#include "rpi.h"

#include <stdint.h>
#include <stddef.h>

uint32_t* kernel_vmm = NULL;
uint32_t* current_vmm = NULL;

void* vmm_alloc(uint32_t* vmm, void* vaddr, size_t size, int flags) {
	return NULL;
}


void vmm_free(uint32_t* vmm, void* vaddr, size_t size) {
	return;
}

int vmm_init() {
	return 0;
}


#endif
