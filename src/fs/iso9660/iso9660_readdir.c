
//
//  iso9660_readdir.c
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

struct dirent* iso9660_readdir(struct inode* ino) {
	if(!ino)
		return 0;
		
	if(!ino->dev)
		return 0;
		
	if(!ino->disk_ptr)
		return 0;
		
	__lock
		
	iso9660_dir_t* dir = ino->disk_ptr;
	iso9660_dir_t* nodes = kmalloc(iso9660_getlsb32(dir->length));
	iso9660_dir_t* snodes = nodes;
	
	
	ino->dev->position = iso9660_getlsb32(dir->lba) * ISO9660_SECTOR_SIZE;
	if(fs_read(ino->dev, iso9660_getlsb32(dir->length), nodes) != iso9660_getlsb32(dir->length)) {
		__unlock
		
		kfree(snodes);
		return 0;
	}
	
	
	// Jump "." ".."
	nodes = (iso9660_dir_t*) ((uint32_t) nodes + nodes->size);
	nodes = (iso9660_dir_t*) ((uint32_t) nodes + nodes->size);
	
	for(int i = 0; i < ino->position; i++) {		
		if(nodes->size == 0) {
			__unlock
			
			kfree(snodes);
			return 0;
		}
		
		nodes = (iso9660_dir_t*) ((uint32_t) nodes + nodes->size);
	}
	
	if(nodes->reserved[0] == 0) {
		__unlock
		
		kfree(snodes);
		return 0;
	}
		
	ino->position += 1;
	
	
	
		
	
	struct dirent* ent = kmalloc(sizeof(struct dirent));
	memset(ent, 0, sizeof(struct dirent));
	
	strncpy(ent->d_name, nodes->reserved, nodes->idlen);	
	iso9660_checkname(ent->d_name);
	
	inode_t* f = iso9660_check_inode(ino, ent->d_name);
	if(!f) {
		f = iso9660_finddir(ino, ent->d_name);
		if(!f) {
			__unlock
			
			kfree(ent);
			kfree(snodes);
			return 0;
		}
	}
	
	ent->d_ino = f->inode;
	
	
	__unlock
	
	kfree(snodes);
	return ent;
}