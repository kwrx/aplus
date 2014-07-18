//  iso9660_finddir.c
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
#include <aplus/spinlock.h>

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "iso9660.h"

extern int iso9660_read(struct inode* ino, uint32_t size, void* buf);
extern struct dirent* iso9660_readdir(struct inode* ino);

struct inode* iso9660_finddir(struct inode* ino, char* name) {
	if(!ino)
		return 0;
		
	if(!ino->dev)
		return 0;
		
	if(!ino->disk_ptr)
		return 0;
		
	if(name[0] == 0)
		return 0;
		
	if(strcmp(name, ".") == 0)
		return ino;
		
	if(strcmp(name, "..") == 0)
		return ino->parent;
		
		
	__lock
	
	inode_t* f = iso9660_check_inode(ino, name);
	if(f) {
		__unlock

		return f;
	}
	
		
	iso9660_dir_t* dir = ino->disk_ptr;
	iso9660_dir_t* nodes = kmalloc(iso9660_getlsb32(dir->length));
	iso9660_dir_t* snodes = nodes;
	
	
	ino->dev->position = iso9660_getlsb32(dir->lba) * ISO9660_SECTOR_SIZE;
	if(fs_read(ino->dev, iso9660_getlsb32(dir->length), nodes) != iso9660_getlsb32(dir->length)) {
		__unlock
		
		kfree(snodes);
		return 0;
	}
	
	
	
	for(;;) {
		if(nodes->size == 0) {
			__unlock
			
			kfree(snodes);
			return 0;
		}
		
		char* nodename = kmalloc(nodes->idlen);
		memset(nodename, 0, nodes->idlen);
		
		strncpy(nodename, nodes->reserved, nodes->idlen);
		iso9660_checkname(nodename);
		
		if(strcmp(nodename, name) == 0)
			break;
			
		kfree(nodename);
		nodes = (iso9660_dir_t*) ((uint32_t) nodes + nodes->size);
	}
	
	
		
	f = kmalloc(sizeof(inode_t));
	memset(f, 0, sizeof(inode_t));
			
	f->parent = ino;
	strcpy(f->name, name);
	f->inode = fs_geninode(f->name);
	f->uid = f->gid = f->position = 0;
			
	f->length = iso9660_getlsb32(nodes->length);
	

	f->ioctl = 0;
	f->write = 0;
	f->trunc = 0;
	f->allocate = 0;
	f->destroy = 0;
	f->creat = 0;
	f->remove = 0;
			
	if(nodes->flags & ISO9660_FLAGS_DIRECTORY) {
		f->readdir = iso9660_readdir;
		f->finddir = iso9660_finddir;
		f->mask = S_IFDIR;
		
		iso9660_dir_t* fentry = kmalloc(ISO9660_SECTOR_SIZE);
		memset(fentry, 0, ISO9660_SECTOR_SIZE);
		
		ino->dev->position = iso9660_getlsb32(nodes->lba) * ISO9660_SECTOR_SIZE;
		fs_read(ino->dev, ISO9660_SECTOR_SIZE, fentry);
		
		f->disk_ptr = fentry;
	} else {
		f->read = iso9660_read;
		f->mask = S_IFREG;
		
		f->disk_ptr = iso9660_getlsb32(nodes->lba) * ISO9660_SECTOR_SIZE;
	}	
			
	f->ctime = time(NULL);
	f->mtime = time(NULL);
	f->atime = time(NULL);
			
	f->links_count = 0;
	f->flock = 0;
			
	memset(f->reserved, 0, INODE_RESERVED_SIZE);
			

	f->link = 0;
	f->dev = ino->dev;
	
	iso9660_add_inode(f);
	
	__unlock
	
	kfree(snodes);
	return f;
}
