//
//  sfs_mount.c
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
#include <string.h>
#include <stdint.h>
#include <dirent.h>

#include "sfs.h"

#if 0
extern struct dirent* sfs_readdir(struct inode* ino);
extern struct inode* sfs_finddir(struct inode* ino, char* name);
extern struct inode* sfs_creat(struct inode* ino, char* name, int mode);
extern int sfs_remove(struct inode* ino, char* name);
#endif

int sfs_mount(struct inode* dev, struct inode* ino, int flags) {

	if(!dev)
		return -1;
		
	if(!ino)
		return -1;
		
	void* sb = kmalloc(sizeof(sfs_superblock_t));
	memset(sb, 0, sizeof(sfs_superblock_t));
		
	if(fs_read(dev, sizeof(sfs_superblock_t), sb) < sizeof(sfs_superblock_t)) {
		kfree(sb);
		return -1;
	}
		
	if(sfs_check(sb) != 0) {
		kfree(sb);
		return -1;
	}
	
#if 0	
	ino->readdir = sfs_readdir;
	ino->finddir = sfs_finddir;
	ino->creat = sfs_creat;
	ino->remove = sfs_remove;
	ino->reserved[0] = sb;
#endif

	return 0;
}

FSYS_DECLARE(sfs);
