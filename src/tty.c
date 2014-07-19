//
//  tty.c
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

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include <aplus.h>
#include <aplus/task.h>
#include <aplus/vfs.h>
#include <aplus/ioctl.h>



uint8_t __default_vkscan[512] = {
	0x00, 0x1B, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
	0x27, 0xEC, 0x08, 0x09, 0x71, 0x77, 0x65, 0x72, 0x74, 0x79, 0x75, 0x69,
	0x6F, 0x70, 0xE8, 0x2B, '\n', 0x00, 0x61, 0x73, 0x64, 0x66, 0x67, 0x68,
	0x6A, 0x6B, 0x6C, 0xF2, 0xE0, 0x5C, 0x00, 0xF9, 0x7A, 0x78, 0x63, 0x76,
	0x62, 0x6E, 0x6D, 0x2C, 0x2E, 0x2D, 0x00, 0x2A, 0x00, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x2D, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x1B, 0x21, 0x22, 0xA3, 0x24, 0x25, 0x26,
	0x2F, 0x28, 0x29, 0x3D, 0x3F, 0x5E, 0x08, 0x09, 0x51, 0x57, 0x45, 0x52,
	0x54, 0x59, 0x55, 0x49, 0x4F, 0x50, 0xE9, 0x2A, '\n', 0x00, 0x41, 0x53,
	0x44, 0x46, 0x47, 0x48, 0x4A, 0x4B, 0x4C, 0xE7, 0xB0, 0x7C, 0x00, 0xA7,
	0x5A, 0x58, 0x43, 0x56, 0x42, 0x4E, 0x4D, 0x3B, 0x3A, 0x5F, 0x00, 0x2A,
	0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2D, 0x00, 0x00, 0x00, 0x2B, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

uint8_t* vkscan = __default_vkscan;
static uint8_t shift, ctrl, alt, caps_lock;



static uint8_t echo_enabled = 1;

extern uint32_t videopos;
extern uint32_t videoattr;

extern task_t* kernel_task;
extern task_t* current_task;





static char kbtochar(uint8_t scan) {
	if((scan & 0x80)) {
		if((scan & 0x7F) == 0x2A || (scan & 0x7F) == 0x36)
			shift = 0;
			
		if((scan & 0x7F) == 0x1D)
			ctrl = 0;
			
		if((scan & 0x7F) == 0x38)
			alt = 0;
	}
	else if (scan == 0x2A || scan == 0x36)
		shift = 1;
		
	else if (scan == 0x1D)
		ctrl = 1;
		
	else if (scan == 0x38)
		alt = 1;
		
	else if(scan == 0x3A)
		caps_lock = !(caps_lock);
	
	else
		if(caps_lock)
			return toupper(vkscan[scan + (shift ? 256 : 0)]);
		else
			return vkscan[scan + (shift ? 256 : 0)];
		
	return 0;
}


uint8_t tty_getc() {
	int fd = open("/dev/kb", O_RDONLY, 0644);
	if(fd < 0)
		return 0;
		
	int ch;
	ioctl(fd, IOCTL_KB_GETVKEY, &ch);	
	close(fd);
	
	return kbtochar(ch);
}

int tty_ioctl(struct inode* ino, int req, void* buf) {
	if(!ino)
		return -1;
		
	switch(req) {
		case IOCTL_TTY_SETATTR:
			videoattr = *(uint32_t*) buf & 0xFF;
			break;
		
		case IOCTL_TTY_GETATTR:
			*(uint32_t*) buf = videoattr & 0xFF;
			break;
			
		case IOCTL_TTY_SETPOSITION:
			videopos = *(uint32_t*) buf;
			break;
			
		case IOCTL_TTY_GETPOSITION:
			*(uint32_t*) buf = videopos;
			break;
					
		case IOCTL_TTY_ECHO_ENABLED:
			echo_enabled = *(uint32_t*) buf;
			break;
			
		case IOCTL_TTY_CLEAR:
			video_clear();
			break;
			
		case IOCTL_TTY_RESET:
			video_init();
			break;
			
		default:
			return -1;
	}
		
	return 0;
}

int tty_read(struct inode* ino, uint32_t length, void* buf) {
	if(!buf)
		return 0;
		
	if(!ino)
		return 0;
	
	
	int i = 0;
	while(1) {
		char ch = tty_getc();
		
		if(ch == 0)
			continue;
		
		if(ch == '\n')
			break;
			
		else if(ch == '\b')
			if(i > 0)
				i--;
			else
				continue;
		else if(i > length)
			continue;
		else
			((uint8_t*) buf) [i++] = ch;
		
		if(echo_enabled)
			video_putc(ch);
	
	}
	
	video_putc('\n');
	((uint8_t*) buf) [i] = 0;
	
	
	ino->position = videopos;
	return i;
}

int tty_write(struct inode* ino, uint32_t length, void* buf) {
	if(!buf)
		return 0;
		
	if(!ino)
		return 0;
				
	for(int i = 0; i < length; i++)
		video_putc(((uint8_t*) buf) [i]);
	
	ino->position = videopos;
	return length;
}

int tty_trunc(struct inode* ino) {
	video_clear();
	
	ino->position = videopos;
	return 0;
}

int tty_init() {	
	
	int fd = open("/dev/tty0", O_CREAT | O_EXCL, S_IFCHR);
	if(fd < 0) {
		kprintf("tty: could not create /dev/tty0 device\n");
		return -1;
	}
	
	inode_t* dev = current_task->fd[fd];
	close(fd);
	
	dev->length = 0;

	dev->ioctl = tty_ioctl;
	dev->read = tty_read;
	dev->write = tty_write;
	dev->trunc = tty_trunc;

	kernel_task->fd[STDIN_FILENO] = dev;
	kernel_task->fd[STDOUT_FILENO] = dev;
	kernel_task->fd[STDERR_FILENO] = dev;
	
	return 0;
}