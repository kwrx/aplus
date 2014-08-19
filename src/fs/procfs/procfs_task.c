//
//  procfs_task.c
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
#include <dirent.h>
#include <fcntl.h>

extern struct dirent* procfs_readdir(struct inode* ino);
extern struct inode* procfs_finddir(struct inode* ino, char* name);

inode_t* procfs_task_create(inode_t* parent, task_t* t) {
	inode_t* node = kmalloc(sizeof(inode_t));
	memset(node, 0, sizeof(inode_t));
	
	char* pbuf = kmalloc(16);
	memset(pbuf, 0, 16);
	
	sprintf(pbuf, "%d", t->pid);
	
	node->parent = parent;
	strcpy(node->name, pbuf);
	node->inode = t->pid;
	node->uid = node->gid = node->position = 0;
	node->length = 0;
	node->mask = S_IFDIR;
	
	node->ioctl = 0;
	node->read = 0;
	node->write = 0;
	node->trunc = 0;
	node->allocate = 0;
	node->readdir = procfs_readdir;
	node->finddir = procfs_finddir;
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
	
	
	node->link = 0;
	node->dev = parent->dev;
	
	
	procfs_add_inode(node, procfs_cmdline_create(node, t));
	procfs_add_inode(node, procfs_environ_create(node, t));
	procfs_add_inode(node, procfs_cwd_create(node, t));
	procfs_add_inode(node, procfs_fd_create(node, t));
	procfs_add_inode(node, procfs_uptime_create(node, t));
	
	return node;
}
