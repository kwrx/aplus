//
//  procfs_filesystems.c
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


void procfs_filesystems_updatebuf(char* buf) {

	memset(buf, 0, BUFSIZ);

	extern uint32_t fsys_start;
	extern uint32_t fsys_end;
	
	fsys_t* fs = (fsys_t*) &fsys_start;
	uint32_t fsys_count = ((uint32_t) &fsys_end - (uint32_t) &fsys_start) / sizeof(fsys_t);
		
	for(int i = 0; i < fsys_count; i++) {
		strcat(buf, "\t");
		strcat(buf, fs[i].name);
		strcat(buf, "\n");
	}
}

int procfs_filesystems_read(struct inode* ino, uint32_t size, void* buf) {
	if(!ino)
		return 0;
		
	if(!size)
		return 0;
		
	if(size > BUFSIZ)
		return 0;
		
	return strlen(strncpy(buf, ino->disk_ptr, size));
}


int procfs_filesystems_destroy(struct inode* ino) {
	if(!ino)
		return -1;
	
	kfree(ino->disk_ptr);
	return 0;
}

inode_t* procfs_filesystems_create(inode_t* parent, task_t* t) {
	if(!t)
		return 0;
		
	char* arg = kmalloc(BUFSIZ);
	memset(arg, 0, BUFSIZ);

	procfs_filesystems_updatebuf(arg);


	inode_t* node = kmalloc(sizeof(inode_t));
	memset(node, 0, sizeof(inode_t));
	
	strcpy(node->name, "filesystems");
	node->inode = fs_nextinode();
	node->uid = node->gid = node->position = 0;
	node->length = BUFSIZ;
	node->mask = S_IFREG;
	
	node->ioctl = 0;
	node->read = procfs_filesystems_read;
	node->write = 0;
	node->trunc = 0;
	node->allocate = 0;
	node->readdir = 0;
	node->finddir = 0;
	node->creat = 0;
	node->remove = 0;
	node->destroy = procfs_filesystems_destroy;
	
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
