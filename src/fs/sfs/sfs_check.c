//
//  sfs_check.c
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

int sfs_check(sfs_superblock_t* sblock) {

#ifdef DEBUG
	kprintf("sfs: superblock dump\n");
	kprintf("\ttimestamp: %u\n", sblock->timestamp);
	kprintf("\tsize_data: %u\n", sblock->size_data);
	kprintf("\tsize_index: %u\n", sblock->size_index);
	kprintf("\tmagic: 0x%x\n", (*(uint32_t*)sblock->magic & ~0xFF));
	kprintf("\tversion: %x\n", sblock->version);
	kprintf("\ttotal_blocks: %u\n", sblock->total_blocks);
	kprintf("\treserved_blocks: %u\n", sblock->reserved_blocks);
	kprintf("\tblock_size: %u\n", sblock->block_size);
	kprintf("\tchecksum: %u\n", sblock->checksum);
#endif

	if((*(uint32_t*)sblock->magic & ~0xFF) == SFS_MAGIC)
		return 0;
		
	return -1;
}
