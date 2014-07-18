//
//  procfs_cwd.c
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

inode_t* procfs_cwd_create(inode_t* parent, task_t* t) {
		
	if(!t)
		return 0;
	

	inode_t* node = kmalloc(sizeof(inode_t));
	memset(node, 0, sizeof(inode_t));
	
	node->parent = parent;
	strcpy(node->name, "cwd");
	node->inode = fs_geninode(node->name);
	node->uid = node->gid = node->position = 0;
	node->length = 0;
	node->mask = S_IFLNK;
	
	node->ioctl = 0;
	node->read = 0;
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
	
	node->disk_ptr = 0;
	node->links_count = 0;
	node->flock = 0;
	
	memset(node->reserved, 0, INODE_RESERVED_SIZE);
	
	
	node->link = t->cwd;
	node->dev = parent->dev;
	
	return node;
}
