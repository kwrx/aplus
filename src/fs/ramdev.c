//
//  ramdev.c
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

#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>


int ramdev_read(struct inode* ino, uint32_t size, void* buf) {
	if(!ino)
		return 0;
		
	if(!buf)
		return 0;
		
	if(size > ino->length)
		size = ino->length;
		
	if(ino->position > ino->length)
		ino->position = ino->length;
		
	if(ino->position + size > ino->length)
		size = ino->length - ino->position;
		
	if(!size)
		return 0;
		
	memcpy(buf, ino->disk_ptr + ino->position, size);
	ino->position += size;
	return size;
}

int ramdev_write(struct inode* ino, uint32_t size, void* buf) {
	if(!ino)
		return 0;
		
	if(!buf)
		return 0;
		
	if(size > ino->length)
		size = ino->length;
		
	if(ino->position > ino->length)
		ino->position = ino->length;
		
	if(ino->position + size > ino->length)
		size = ino->length - ino->position;
		
	if(!size)
		return 0;
		
	memcpy(ino->disk_ptr + ino->position, buf, size);
	ino->position += size;
	return size;
}

int ramdev_destroy(struct inode* ino) {
	if(!ino)
		return -1;
		
	kfree(ino->disk_ptr);
	return 0;
}


extern task_t* current_task;

inode_t* ramdev_create(char* path, uint32_t addr, uint32_t size) {

	int fd = open(path, O_CREAT | O_EXCL, S_IFCHR);
	if(fd < 0) 
		return 0;
	
	inode_t* t = current_task->fd[fd];
	close(fd);
	

	t->read = ramdev_read;
	t->write = ramdev_write;
	t->destroy = ramdev_destroy;
	t->disk_ptr = addr;
	t->length = size;
	
	kprintf("ramdev: created \"%s\" at 0x%x (%d Bytes)\n", path, addr, size);


	return t;
}