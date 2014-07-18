//
//  zero.c
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

int zero_read(struct inode* ino, uint32_t length, void* buf) {
	if(!buf)
		return 0;
		
	if(!ino)
		return 0;
		
	memset(buf, 0, length);
	return length;
}


int cmain(int argc, char** argv) {

	inode_t* dev = aplus_device_create("/dev/zero", S_IFCHR);
	if(!dev) {
		printf("zero: could not create device!\n");
		return -1;
	}

	dev->read = zero_read;
	return 0;
}

