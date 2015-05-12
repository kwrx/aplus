#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/list.h>
#include <aplus/task.h>

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <errno.h>

extern task_t* current_task;

list_t* mmzones = NULL;

void mmzone_invalidate(mmzone_t* mmzone) {
	if(unlikely(!mmzone))
		return;

	/* it causes page fault */
	vmm_map(mmzone->ctx, mm_paddr(mmzone->buffer), mmzone->address, mmzone->length, VMM_PROT_NONE);
	vmm_flush(NULL);
}

void mmzone_validate(mmzone_t* mmzone) {
	if(unlikely(!mmzone))
		return;

	vmm_map(mmzone->ctx, mm_paddr(mmzone->buffer), mmzone->address, mmzone->length, VMM_PROT_READ | VMM_PROT_WRITE | VMM_PROT_EXEC);
	vmm_flush(NULL);
}


mmzone_t* mmzone_find(void* address) {
	list_foreach_reverse(v, mmzones) {
		mmzone_t* mz = (mmzone_t*) v;
		
		if(
			((uint32_t) address >= (uint32_t) mz->address) &&
			((uint32_t) address < (uint32_t) mz->address + mz->length)
		) return mz;
	}

	return NULL;
}

int mmzone_add(void* address, size_t length, int (*handler) (void*), int flags) {
	if(unlikely(mmzones == NULL))
		{ list_init(mmzones); }

	mmzone_t* mz = (mmzone_t*) kmalloc(sizeof(mmzone_t));
	mz->address = address;
	mz->length = length;
	mz->handler = handler;
	mz->ctx = (void*) current_task->context.cr3;
	mz->flags = flags;

	if(flags & MMZONE_NO_BUFFER)
		mz->buffer = NULL;
	else
		mz->buffer = (void*) kvmalloc(length);

	mmzone_invalidate(mz);

#ifdef MMZONE_DEBUG
	kprintf("mmzone: added 0x%x (%d Bytes) at 0x%x\n", address, length, handler);
#endif

	return list_add(mmzones, (listval_t) mz);
}


int mmzone_remove(void* address) {
	mmzone_t* mz = mmzone_find(address);
	if(unlikely(!mz))
		return -1;


	if(!(mz->flags & MMZONE_NO_BUFFER))
		kvfree(mz->buffer, mz->length);

	kfree(mz);

#ifdef MMZONE_DEBUG
	kprintf("mmzone: removed 0x%x (%d Bytes) at 0x%x\n", mz->address, mz->length, mz->handler);
#endif


	return list_remove(mmzones, (listval_t) mz);
}


EXPORT_SYMBOL(mmzone_add);
EXPORT_SYMBOL(mmzone_remove);
EXPORT_SYMBOL(mmzone_find);
EXPORT_SYMBOL(mmzone_validate);
EXPORT_SYMBOL(mmzone_invalidate);

