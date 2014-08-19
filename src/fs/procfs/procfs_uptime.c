//
//  procfs_uptime.c
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


int procfs_uptime_read(struct inode* ino, uint32_t size, void* buf) {
	if(!ino)
		return 0;
		
	if(!size)
		return 0;
		
	if(ino->position > ino->length)
		return 0;
		
	if(ino->position + size > ino->length)
		size = ino->length - ino->position;	
		
	if(size > ino->length)
		return 0;
		
	
	sprintf(buf + ino->position, "%d", ((task_t*) ino->disk_ptr)->clock);
	((char*)buf)[ino->position + size] = 0;

	ino->position += size;
	return size;
}



inode_t* procfs_uptime_create(inode_t* parent, task_t* t) {
	if(!t)
		return 0;
		
	
	inode_t* node = kmalloc(sizeof(inode_t));
	memset(node, 0, sizeof(inode_t));
	
	node->parent = parent;
	strcpy(node->name, "uptime");
	node->inode = fs_geninode(node->name);
	node->uid = node->gid = node->position = 0;
	node->length = BUFSIZ;
	node->mask = S_IFREG;
	
	node->ioctl = 0;
	node->read = procfs_uptime_read;
	node->write = 0;
	node->trunc = 0;
	node->allocate = 0;
	node->readdir = 0;
	node->finddir = 0;
	node->creat = 0;
	node->remove = 0;
	node->destroy = 0;
	
	node->ctime = time(NULL);
	node->mtime = time(NULL);
	node->atime = time(NULL);
	
	node->disk_ptr = t;
	node->links_count = 0;
	node->flock = 0;
	
	memset(node->reserved, 0, INODE_RESERVED_SIZE);
	
	
	node->link = 0;
	node->dev = parent->dev;
	
	return node;
}
