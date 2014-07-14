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

static uint8_t echo_enabled = 1;

extern uint32_t videopos;
extern uint32_t videoattr;

extern task_t* kernel_task;
extern task_t* current_task;

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
	
	
	char ch = 0;
	int i = 0;
	while(ch = video_getc()) {
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