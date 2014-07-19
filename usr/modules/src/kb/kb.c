//
//  kb.c
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


#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>


#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/events.h>
#include <aplus/ioctl.h>



volatile int last_scancode = 0;
volatile int scancode = 0;

static void irq_handler(void* unused) {
	last_scancode = inb(0x60);
	
	if(last_scancode & 0x80)
		event_raise(EV_KB_KEYUP);
	else
		event_raise(EV_KB_KEYDOWN);
	
	scancode = last_scancode;
}


static int kb_getscan() {
	__idle();
	while(!scancode)
		__asm__ __volatile__ ("pause");
	
	int ch = scancode;
	scancode = 0;	
		
	__wakeup();
	return ch;
}


int kb_ioctl(struct inode* ino, int req, void* buf) {
	if(!ino)
		return -1;
		
	switch(req) {
		case IOCTL_KB_GETVKEY:
			*(uint32_t*) buf = kb_getscan();
			break;
			
		case IOCTL_KB_GETLASTVKEY:
			*(uint32_t*) buf = last_scancode;
			break;
		default:
			return -1;
	}
	
	return 0;
}


int init() {
	inode_t* dev = aplus_device_create("/dev/kb", S_IFCHR);
	if(!dev) {
		printf("kb: could not create device!\n");
		return -1;
	}

	dev->ioctl = kb_ioctl;
	irq_set(1, irq_handler);
	return 0;
}

int dnit() {
	irq_unset(1);
	unlink("/dev/kb");
	
	return 0;
}


