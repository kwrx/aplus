//
//  main.c
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
#include <aplus/fs.h>
#include <aplus/list.h>
#include <grub.h>
#include <aplus/fs.h>
#include <aplus/task.h>
#include <aplus/attribute.h>
#include <aplus/mm.h>

#include <stddef.h>
#include <sys/times.h>
#include <time.h>

#include <aplus/netif.h>
#include <aplus/net/eth.h>
#include <aplus/net/ipv4.h>
#include <aplus/net/ipv6.h>


extern inode_t* vfs_root;
extern task_t* current_task;


static void sysidle() {
	schedule_setpriority(TASK_PRIORITY_LOW);


	for(;;)
		__asm__ ("pause");
}


int main() {
	serial_init();
	mm_init();
	desc_init();
	syscall_init();
	vfs_init();
	schedule_init();
	bufio_init();
	pci_init();
	netif_init();
	
	vfs_map(devfs_mount());


	if(mbd->mods_count == 0)
		panic("no initrd module found");
	

	uint32_t addr = ((uint32_t*) mbd->mods_addr) [0];
	uint32_t endp = ((uint32_t*) mbd->mods_addr) [1];


	kprintf("initrd: module found at addess: 0x%x (%d KB)\n", addr, (endp - addr) / 1024);


	if(!mkramdev("/dev/ram0", addr, endp - addr))
		panic("initrd: cannot create /dev/ram0");


	//if(sys_mount("/dev/ram0", "/dev/ramdisk", "iso9660", 0, 0) != 0)
	//	panic("initrd: cannot mount ramdisk");



	/* TODO: iso9660 support */



/*
	if(fork() == 0)
		execl("/bin/init", "/bin/init", 0);
	else
*/
	sysidle();
}

