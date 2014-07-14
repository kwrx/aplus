//
//  initrd.c
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

#include <grub.h>
#include <aplus.h>
#include <aplus/vfs.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

int initrd_init() {

	if(mbd->mods_count == 0) {
		kprintf("initrd: no module found!\n");
		return -1;
	}

	uint32_t addr = ((uint32_t*)mbd->mods_addr)[0];
	uint32_t end = ((uint32_t*)mbd->mods_addr)[1];
	
	kprintf("initrd: found at address: 0x%x (%d bytes)\n", addr, end - addr);
	
	if(!ramdev_create("/dev/ram0", addr, end - addr)) {
		kprintf("initrd: cannot create ram device!\n");
		return -1;
	}
		
	if(mount("/dev/ram0", "/ramdisk", "initfs", 0, 0) != 0) {
		kprintf("initrd: cannot mount ramdisk\n");
		return -1;
	}
	
	return 0;
}
