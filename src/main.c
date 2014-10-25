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
#ifdef DEBUG
	debug_init();
#endif

	mm_init();
	desc_init();
	vfs_init();
	schedule_init();
	syscall_init();
	bufio_init();
	pci_init();
	netif_init();
	
	vfs_map(devfs_mount());

	
/*
	if(fork() == 0)
		execl("/bin/init", "/bin/init", 0);
	else
*/
		sysidle();
}
