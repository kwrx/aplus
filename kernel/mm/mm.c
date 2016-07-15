#include <xdev.h>
#include <xdev/ipc.h>
#include <xdev/mm.h>

#define ALIGN(x)		(((x) + (PAGE_ALIGN - 1)) & ~(PAGE_ALIGN - 1))

int mm_init(void) {
	pmm_init();
	slab_init();


	return E_OK;
}
