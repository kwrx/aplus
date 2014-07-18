//
//  iso9660_mount.c
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

#include "iso9660.h"

extern struct dirent* iso9660_readdir(struct inode* ino);
extern struct inode* iso9660_finddir(struct inode* ino, char* name);


int iso9660_mount(struct inode* dev, struct inode* ino, int flags) {

	if(!dev)
		return -1;
		
	if(!ino)
		return -1;
	
	if(iso9660_check(dev) != 0) {
		kprintf("iso9660: (%s) check failed\n", dev->name);
		return -1;
	}
	
	ino->disk_ptr = iso9660_getroot();
	
	iso9660_dir_t* dir = (iso9660_dir_t*) ino->disk_ptr;
	
	ino->readdir = iso9660_readdir;
	ino->finddir = iso9660_finddir;
	ino->creat = 0;
	ino->remove = 0;
	
	return 0;
}

FSYS_DECLARE(iso9660);
