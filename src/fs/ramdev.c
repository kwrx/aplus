#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/task.h>
#include <aplus/bufio.h>

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

	bufio_seek(ino->userdata, ino->position, SEEK_SET);
	size = bufio_read(ino->userdata, buf, size);
	ino->position += size;
	return size;
}

int ramdev_write(inode_t* ino, char* buf, int size) {
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

	bufio_seek(ino->userdata, ino->position, SEEK_SET);
	size = bufio_write(ino->userdata, buf, size);
	ino->position += size;
	return size;
}

void ramdev_flush(inode_t* ino) {
	if(!ino)
		return;

	if(!ino->userdata)
		return;

	bufio_free(ino->userdata);
}


inode_t* mkramdev(char* path, uint32_t addr, uint32_t size) {
	int fd = sys_open(path, O_CREAT | O_EXCL | O_TRUNC, S_IFCHR);
	if(fd < 0)
		return NULL;

	inode_t* ino = (inode_t*) current_task->fd[fd];
	sys_close(fd);

	ino->read = ramdev_read;
	ino->write = ramdev_write;
	ino->flush = ramdev_flush;
	ino->userdata = (void*) bufio_alloc_raw((void*) addr, size);
	ino->size = size;

	return ino;
}


