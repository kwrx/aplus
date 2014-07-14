//
//  procfs_meminfo.c
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

#include <aplus/vfs.h>
#include <aplus/task.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>


static void procfs_meminfo_updatebuf(char* buf) {

	extern uint32_t mm_totalmemory;
	extern uint32_t mm_usedmemory;

	uint32_t mem_total = mm_totalmemory / 1024;
	uint32_t mem_used = mm_usedmemory / 1024;
	
	memset(buf, 0, BUFSIZ);

	sprintf	(
				buf, 
				"MemTotal:\t%8d kB\nMemFree:\t%8d kB\nHighTotal:\t%8d kB\nHighFree:\t%8d kB\nLowTotal:\t%8d kB\nLowFree:\t%8d kB\nSwapTotal:\t%8d kB\nSwapFree:\t%8d kB",
				mem_total,
				mem_total - mem_used,
				(0),
				(0),
				mem_total,
				mem_total - mem_used,
				(0),
				(0)
			);
}

int procfs_meminfo_read(struct inode* ino, uint32_t size, void* buf) {
	if(!ino)
		return 0;
		
	if(!size)
		return 0;
		
	if(size > BUFSIZ)
		return 0;
		
	procfs_meminfo_updatebuf(ino->disk_ptr);
	return strlen(strncpy(buf, ino->disk_ptr, size));
}


int procfs_meminfo_destroy(struct inode* ino) {
	if(!ino)
		return -1;
	
	kfree(ino->disk_ptr);
	return 0;
}

inode_t* procfs_meminfo_create(inode_t* parent) {
		
	char* arg = kmalloc(BUFSIZ);
	memset(arg, 0, BUFSIZ);

	inode_t* node = kmalloc(sizeof(inode_t));
	memset(node, 0, sizeof(inode_t));
	
	strcpy(node->name, "meminfo");
	node->inode = fs_nextinode();
	node->uid = node->gid = node->position = 0;
	node->length = BUFSIZ;
	node->mask = S_IFREG;
	
	node->ioctl = 0;
	node->read = procfs_meminfo_read;
	node->write = 0;
	node->trunc = 0;
	node->allocate = 0;
	node->readdir = 0;
	node->finddir = 0;
	node->creat = 0;
	node->remove = 0;
	node->destroy = procfs_meminfo_destroy;
	
	node->ctime = time(NULL);
	node->mtime = time(NULL);
	node->atime = time(NULL);
	
	node->disk_ptr = arg;
	node->links_count = 0;
	node->flock = 0;
	
	memset(node->reserved, 0, INODE_RESERVED_SIZE);
	
	node->parent = parent;
	node->link = 0;
	node->dev = parent->dev;
	
	return node;
}
