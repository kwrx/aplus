//
//  procfs_fd.c
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
struct dirent* procfs_fd_readdir(struct inode* ino) {
	if(!ino)
		return 0;
		
	uint32_t* fd = ino->disk_ptr;
	if(!fd)
		return 0;
		
			
	for(int i = 0, c = 0, index = ino->position; i < TASK_MAX_FD; i++) {
		if(fd[i])
			c++;
			
		if(c == index + 1) {
			struct dirent* ent = kmalloc(sizeof(struct dirent));
			memset(ent, 0, sizeof(struct dirent));
			
			char* pbuf = kmalloc(16);
			memset(pbuf, 0, 16);
			
			sprintf(pbuf, "%d", i);
			
			strcpy(ent->d_name, pbuf);
			ent->d_ino = fs_nextinode();
			
			ino->position += 1;
			return ent;
		}
	}
	
	return 0;
}


struct inode* procfs_fd_finddir(struct inode* ino, char* name) {
	if(!ino)
		return 0;
	
	if(!name)
		return 0;
	
	if(name[0] == 0)
		return 0;
		
	if(strcmp(name, ".") == 0)
		return ino;
		
	if(strcmp(name, "..") == 0)
		return ino->parent;
	
	if(!ino->dev)
		return 0;
		
	uint32_t* fd = ino->disk_ptr;
	if(!fd)
		return 0;
		
			
	for(int i = 0; i < TASK_MAX_FD; i++) {
		if(fd[i]) {
			static char pbuf[16];
			memset(pbuf, 0, 16);
			
			sprintf(pbuf, "%d", i);
			
			if(strcmp(name, pbuf) == 0) {
				inode_t* f = kmalloc(sizeof(inode_t));
				memset(f, 0, sizeof(inode_t));
				
				strcpy(f->name, name);
				f->inode = fs_nextinode();
				f->uid = f->gid = f->length = f->position = 0;
				f->mask = S_IFLNK;
				
				f->ioctl = 0;
				f->read = 0;
				f->write = 0;
				f->trunc = 0;
				f->allocate = 0;
				f->destroy = 0;
				f->readdir = 0;
				f->finddir = 0;
				f->creat = 0;
				f->remove = 0;
				
				f->ctime = time(NULL);
				f->mtime = time(NULL);
				f->atime = time(NULL);
				
				f->disk_ptr = 0;
				f->links_count = 0;
				f->flock = 0;
				
				memset(f->reserved, 0, INODE_RESERVED_SIZE);
				
				f->parent = ino;
				f->link = (inode_t*) fd[i];
				f->dev = ino->dev;
				
				return f;
			}
		}
	}
	
	return 0;
}



inode_t* procfs_fd_create(inode_t* parent, task_t* t) {
	inode_t* node = kmalloc(sizeof(inode_t));
	memset(node, 0, sizeof(inode_t));
	
	
	strcpy(node->name, "fd");
	node->inode = fs_nextinode();
	node->uid = node->gid = node->position = 0;
	node->length = 0;
	node->mask = S_IFDIR;
	
	node->ioctl = 0;
	node->read = 0;
	node->write = 0;
	node->trunc = 0;
	node->allocate = 0;
	node->readdir = procfs_fd_readdir;
	node->finddir = procfs_fd_finddir;
	node->creat = 0;
	node->remove = 0;
	node->destroy = 0;
	
	node->ctime = time(NULL);
	node->mtime = time(NULL);
	node->atime = time(NULL);
	
	node->disk_ptr = t->fd;
	node->links_count = 0;
	node->flock = 0;
	
	memset(node->reserved, 0, INODE_RESERVED_SIZE);
	
	node->parent = parent;
	node->link = 0;
	node->dev = parent->dev;
	
	
	return node;
}