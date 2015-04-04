#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/task.h>
#include <aplus/shm.h>

#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>


extern task_t* current_task;


int ramdev_read(inode_t* ino, char* buf, int size) {
	if(!ino)
		return 0;

	if(!buf)
		return 0;

	if(size > ino->size)
		size = ino->size;

	if(ino->position > ino->size)
		ino->position = ino->size;

	if(ino->position + size > ino->size)
		size = ino->size - ino->position;

	if(!size)
		return 0;

	
	if(shm_check_chunk(ino->userdata) < 0)
		return 0;

	shm_chunk_t* chunk = (shm_chunk_t*) ino->userdata;

	memcpy(buf, (void*) (chunk->addr + ino->position), size);	
	ino->position += size;
	return size;
}

int ramdev_write(inode_t* ino, char* buf, int size) {
	if(unlikely(!ino))
		return 0;

	if(unlikely(!buf))
		return 0;

	if(unlikely(size > ino->size))
		size = ino->size;

	if(unlikely(ino->position > ino->size))
		ino->position = ino->size;

	if(unlikely(ino->position + size > ino->size))
		size = ino->size - ino->position;

	if(unlikely(!size))
		return 0;


	if(unlikely(shm_check_chunk(ino->userdata) < 0))
		return 0;

	shm_chunk_t* chunk = (shm_chunk_t*) ino->userdata;

	memcpy((void*) (chunk->addr + ino->position), buf, size);	
	ino->position += size;
	return size;
}

void ramdev_flush(inode_t* ino) {
	if(!ino)
		return;

	if(!ino->userdata)
		return;

	shm_release_chunk(ino->userdata);
}


inode_t* mkramdev(char* path, uint32_t addr, uint32_t size) {
	int fd = sys_open(path, O_CREAT | O_EXCL | O_TRUNC, S_IFCHR);
	if(unlikely(fd < 0))
		return NULL;

	inode_t* ino = (inode_t*) current_task->fd[fd];
	sys_close(fd);

	ino->read = ramdev_read;
	ino->write = ramdev_write;
	ino->flush = ramdev_flush;
	ino->size = size;


	void* shm;
	if(unlikely((shm = (void*) shm_acquire_from_inode(ino, (uint32_t*) &ino->size)) == NULL))
		panic("Cannot acquire Shared Memory Space");


	memcpy(shm, (void*) addr, size);
	return ino;
}

EXPORT_SYMBOL(mkramdev);


