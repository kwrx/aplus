
//
//  events.c
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
#include <aplus/ioctl.h>
#include <aplus/events.h>


#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include <pthread.h>


struct ev_handler {
	int pid;
	int type;
	int raising;
	
	struct ev_handler* next;
};

struct ev_handler* ev_queue = 0;


static int ev_add(int type) {
	struct ev_handler* tmp = ev_queue;
	
	ev_queue = malloc(sizeof(struct ev_handler));
	ev_queue->pid = getpid();
	ev_queue->type = type;
	ev_queue->raising = 0;
	ev_queue->next = tmp;
	
	return 0;
}

static int ev_rem(int type) {
	struct ev_handler* tmp = ev_queue;
	if(!tmp)
		return EV_ERROR;
	
	int pid = getpid();	
	
	if(tmp->pid == pid) {
		if(tmp->type == type) {
			ev_queue = tmp->next;
			return 0;
		}
	}
	
	while(tmp->next) {
		if(tmp->next->pid == pid) {
			if(tmp->next->type == type) {
				tmp->next = tmp->next->next;
				return 0;
			}
		}
			
		
		tmp = tmp->next;
	}
	
	return EV_ERROR;
}

static int ev_tp() {
	struct ev_handler* tmp = ev_queue;
	if(!tmp)
		return EV_ERROR;
		
		
	int pid = getpid();
	while(tmp) {
		if(tmp->pid == pid)
			if(tmp->raising)
				return tmp->type;
		
		tmp = tmp->next;
	}
	
	return EV_ERROR;
}

static int ev_rs(int type) {
	struct ev_handler* tmp = ev_queue;
	if(!tmp)
		return EV_ERROR;
		
		
	while(tmp) {
		if(tmp->type == type) {
			tmp->raising = 1;
			kill(tmp->pid, SIGEVNT);
			tmp->raising = 0;
		}
		
		tmp = tmp->next;
	}
	
	return 0;
}



int events_ioctl(struct inode* ino, int req, void* buf) {
	if(!ino)
		return -1;
		
	switch(req) {
		case IOCTL_EV_SET:
			return ev_add(*(int*) buf);
			break;
			
		case IOCTL_EV_UNSET:
			return ev_rem(*(int*) buf);
			break;
			
		case IOCTL_EV_TYPE:
			return ev_tp();
			break;
			
		case IOCTL_EV_RAISE:
			return ev_rs(*(int*) buf);
			break;
		
		default:
			return -1;
	}
	
	return 0;
}


int init() {
	inode_t* dev = aplus_device_create("/dev/events", S_IFCHR);
	if(!dev) {
		printf("events: could not create device!\n");
		return -1;
	}

	dev->ioctl = events_ioctl;
	return 0;
}

int dnit() {
	unlink("/dev/events");
}