//
//  pipe.c
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the kfree Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <aplus/vfs.h>
#include <aplus/task.h>

#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

typedef struct pipeinfo {
	void* addr;
	int pos_write;
	int pos_read;
} pipeinfo_t;


int pipe_read(struct inode* ino, uint32_t size, void* buf) {
	if(!ino)
		return 0;
		
	if(!buf)
		return 0;
			
	if(!size)
		return 0;
		
	pipeinfo_t* info = ino->disk_ptr;
	if(!info)
		return 0;
		
	if(info->pos_read + size > info->pos_write) {
		while(info->pos_read + size > info->pos_write)
			task_yield();
	}
		
	
	uint32_t addr = info->addr + (info->pos_read % ino->length);
	for(int i = 0; i < size; i++) {
		if(addr > info->addr + ino->length)
			addr = info->addr;
			
		((uint8_t*) buf) [i] = *(uint8_t*) addr;
		addr += sizeof(uint8_t);
	}
	
	info->pos_read += size;
	
	return size;
}

int pipe_write(struct inode* ino, uint32_t size, void* buf) {
	if(!ino)
		return 0;
		
	if(!buf)
		return 0;
		
		
	if(!size)
		return 0;
		
	pipeinfo_t* info = ino->disk_ptr;
	if(!info)
		return 0;
		
	uint32_t addr = info->addr + (info->pos_write % ino->length);
	for(int i = 0; i < size; i++) {
		if(addr > info->addr + ino->length)
			addr = info->addr;
			
		*(uint8_t*) addr = ((uint8_t*) buf) [i];
		addr += sizeof(uint8_t);
	}
	
	info->pos_write += size;
	
	return size;
}

int pipe_destroy(struct inode* ino) {
	if(!ino)
		return -1;
		
	kfree(((pipeinfo_t*) ino->disk_ptr)->addr);
	kfree(ino->disk_ptr);
	
	return 0;
}


extern task_t* current_task;

inode_t* pipe_create(char* path, uint32_t size) {

	int fd = open(path, O_CREAT | O_EXCL, S_IFCHR);
	if(fd < 0) 
		return 0;
	
	inode_t* t = current_task->fd[fd];
	close(fd);
	
	
	pipeinfo_t* info = kmalloc(sizeof(pipeinfo_t));
	info->addr = kmalloc(size);
	info->pos_read = 0;
	info->pos_write = 0;
	
	memset(info->addr, 0, size);
	
	t->read = pipe_read;
	t->write = pipe_write;
	t->destroy = pipe_destroy;
	t->disk_ptr = info;
	t->length = size;
	
	return t;
}