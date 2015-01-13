#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/shm.h>
#include <aplus/mm.h>
#include <aplus/spinlock.h>
#include <aplus/list.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>

extern task_t* current_task;


int shm_check_chunk(shm_chunk_t* chunk) {
	if(!chunk)
		return -1;

	if(chunk->magic != SHM_MAGIC)
		return -1;

	return 0;
}



shm_chunk_t* shm_acquire_chunk(uint32_t* size) {
	if(!size)
		return NULL;

	shm_chunk_t* chunk = (shm_chunk_t*) kmalloc(sizeof(shm_chunk_t));
	chunk->magic = SHM_MAGIC;
	chunk->addr = (uint32_t) kmalloc(*size);
	chunk->size = *size;
	chunk->refcount = 1;

	if(!current_task->shmmap) {
		list_init(current_task->shmmap);
	}

	list_add(current_task->shmmap, (listval_t) chunk);
	return chunk;
}


void* shm_acquire_from_inode(inode_t* ino, uint32_t* size) {
	if(unlikely(!size))
		return NULL;

	if(unlikely(!ino))
		return NULL;

	shm_chunk_t* chunk = NULL;

	if(ino->userdata) {
		if(((shm_chunk_t*) ino->userdata)->magic != SHM_MAGIC)
			return NULL;

		chunk = (shm_chunk_t*) ino->userdata;
		chunk->refcount++;

		*size = chunk->size;

		if(!current_task->shmmap) {
			list_init(current_task->shmmap);
		}

		list_add(current_task->shmmap, (listval_t) chunk);


	} else {
		chunk = shm_acquire_chunk(size);
		ino->userdata = (void*) chunk;
	}

#ifdef SHM_DEBUG
	kprintf("shm: acquired chunk \"%s\" at 0x%x (%d Bytes) (ref: %d)\n", path, chunk->addr, *size, chunk->refcount);
#endif

	return (void*) chunk->addr;
}

void* shm_acquire(const char* path, uint32_t* size) {
	if(unlikely(!size))
		return NULL;

	int fd = sys_open(path, O_RDONLY, 0644);
	if(unlikely(fd < 0))
		return NULL;

	inode_t* ino = current_task->fd[fd];
	sys_close(fd);

	return shm_acquire_from_inode(ino, size);
}

int shm_release_chunk(shm_chunk_t* chunk) {
#ifdef SHM_DEBUG
	kprintf("shm: release chunk at 0x%x (%d Bytes) (ref: %d)\n", chunk->addr, chunk->size, chunk->refcount);
#endif

	if(!list_empty(current_task->shmmap))
		list_remove(current_task->shmmap, (listval_t) chunk);

	if((--chunk->refcount) > 0)
		return 0;
	
	kfree((void*) chunk->addr);
	kfree((void*) chunk);

	return 0;
}

int shm_release(const char* path) {
	int fd = sys_open(path, O_RDONLY, 0644);
	if(fd < 0)
		return -1;

	inode_t* ino = current_task->fd[fd];
	sys_close(fd);

	
	if(shm_check_chunk((shm_chunk_t*) ino->userdata) < 0)
		return -1;

	return shm_release_chunk((shm_chunk_t*) ino->userdata);
}
