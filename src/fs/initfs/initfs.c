//
//  initfs.c
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
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>


#define INITFS_MAGIC		0x2BADC0DE

typedef struct initfs_inode {
	uint32_t magic;
	char name[256];
	uint32_t length;
	uint32_t offset;
} initfs_inode_t;


int initfs_read(struct inode* ino, uint32_t size, void* buf) {
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
		
	ino->dev->position = ino->disk_ptr + ino->position;
	int len = fs_read(ino->dev, size, buf);
	ino->position += len;
	return len;
}

struct dirent* initfs_readdir(struct inode* ino) {
	if(!ino)
		return 0;
				
	uint32_t index = ino->position;
	uint32_t i = 0;
	
	initfs_inode_t* tmp = kmalloc(sizeof(initfs_inode_t));
	memset(tmp, 0, sizeof(initfs_inode_t));
	
	ino->dev->position = index * sizeof(initfs_inode_t);
	fs_read(ino->dev, sizeof(initfs_inode_t), tmp);
		
	if(tmp->magic != INITFS_MAGIC) {
		kfree(tmp);
		return 0;
	}
	
	
	struct dirent* ent = kmalloc(sizeof(struct dirent));
	strcpy(ent->d_name, tmp->name);
	ent->d_ino = fs_nextinode();
	
	kfree(tmp);

	return ent;
}

struct inode* initfs_finddir(struct inode* ino, char* name) {
	if(!name)
		return 0;
		
	if(!ino)
		return 0;
		
	if(name[0] == 0)
		return 0;
		
	if(strcmp(name, ".") == 0)
		return ino;
		
	if(strcmp(name, "..") == 0)
		return ino->parent;
		
	initfs_inode_t* tmp = kmalloc(sizeof(initfs_inode_t));
	memset(tmp, 0, sizeof(initfs_inode_t));
	
	ino->dev->position = 0;
	
	while(1) {
		fs_read(ino->dev, sizeof(initfs_inode_t), tmp);
		if(tmp->magic != INITFS_MAGIC) {
			kfree(tmp);
			return 0;
		}
			
		if(strcmp(tmp->name, name) == 0) {
			inode_t* f = kmalloc(sizeof(inode_t));
			memset(f, 0, sizeof(inode_t));
			
			strcpy(f->name, name);
			f->inode = fs_nextinode();
			f->uid = f->gid = f->position = 0;
			f->mask = S_IFREG;
			
			f->length = tmp->length;
			f->disk_ptr = tmp->offset;

			f->ioctl = 0;
			f->read = initfs_read;
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
			
			f->links_count = 0;
			f->flock = 0;
			
			memset(f->reserved, 0, INODE_RESERVED_SIZE);
			
			f->parent = ino;
			f->link = 0;
			f->dev = ino->dev;
			
			kfree(tmp);
			
			return f;
		}
	}
	
	kfree(tmp);
	return 0;
}