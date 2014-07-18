

//
//  iso9660_read.c
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


int iso9660_read(struct inode* ino, uint32_t size, void* buf) {
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
		
		
		
	void* tbuf = malloc(((size / ISO9660_SECTOR_SIZE) + 1) * ISO9660_SECTOR_SIZE);
	
	ino->dev->position = ino->disk_ptr + (ino->position / ISO9660_SECTOR_SIZE);
	fs_read(ino->dev, ((size / ISO9660_SECTOR_SIZE) + 1) * ISO9660_SECTOR_SIZE, tbuf);
	
	memcpy(buf, (void*) ((uint32_t) tbuf + (ino->position % ISO9660_SECTOR_SIZE)), size);
	free(tbuf);
	
	return size;
}