#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/spinlock.h>
#include <aplus/task.h>

#include <stdint.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

extern task_t* current_task;
extern task_t* task_queue;
extern task_t* kernel_task;


#define SWAPDIR			"/tmp/swap"
#define SWAPDEV			"/dev/swap"

#define CHUNKSIZ		0x1000

#define CHUNK_IDX(x)	((uint32_t) x / CHUNKSIZ)


static int swap_enabled = 1;
static int swapped = 0;

extern heap_t* current_heap;




static int swap_handler(void*);

static int acquire_chunk(void* address) {

	//kprintf("swap: acquiring 0x%x\n", address);

	char buf[64];
	memset(buf, 0, sizeof(buf));

	ksprintf("%s/%d", SWAPDIR, CHUNK_IDX(address));

	int fd = sys_open(buf, O_CREAT | O_RDWR, 0644);
	if(unlikely(fd < 0))
		return -1;

	sys_write(fd, address, CHUNKSIZ);
	sys_close(fd);

	if(mmzone_add(address, CHUNKSIZ, swap_handler, MMZONE_NO_BUFFER) != 0) {
		sys_unlink(buf);	
		return -1;
	}

	kvfree(address, CHUNKSIZ);
	swapped++;
	return 0;
}


static int release_chunk(mmzone_t* mz) {

	kprintf("swap: releasing 0x%x\n", mz->address);

	char buf[64];
	memset(buf, 0, sizeof(buf));

	ksprintf("%s/%d", SWAPDIR, CHUNK_IDX(mz->address));

	int fd = sys_open(buf, O_RDWR, 0644);
	if(unlikely(fd < 0))
		return -1;

	mz->buffer = (void*) kvmalloc(CHUNKSIZ);
	if(unlikely(!mz->buffer)) {
		errno = ENOMEM;
		return -1;
	}

	sys_read(fd, mz->buffer, CHUNKSIZ);
	sys_close(fd);


	mmzone_validate(mz);
	mmzone_remove(mz->address);

	sys_unlink(buf);
	swapped--;
	return 0;
}


static int swap_handler(void* address) {
	mmzone_t* mz = (mmzone_t*) mmzone_find(address);
	if(unlikely(!mz))
		return MM_ERROR;

	if(release_chunk(mz) != 0)
		return MM_ERROR;

	return MM_OK;
}

int init() {

	swap_enabled = 1;
	swapped = 0;

	for(;; sys_yield()) {
		if(unlikely(!swap_enabled))
			continue;
		

#if 0
		list_foreach_reverse(v, task_queue) {
			task_t* tk = (task_t*) v;

			if(unlikely(tk == kernel_task || tk == current_task))
				continue;

			if(unlikely(tk->image == NULL))
				continue;

			if(unlikely(tk->image->length == 0))
				continue;


			uintptr_t p = tk->image->vaddr;
			for(uintptr i = 0; i < tk->image->length; i += 0x1000, p += 0x1000) {
				if(vmm_accessed(tk->context.cr3, p))
					continue;

				acquire_chunk(p);
			}
		} /* Per-task swapping */
#endif
	}

	return 0;
}

int dnit() {
	return 0;
}
