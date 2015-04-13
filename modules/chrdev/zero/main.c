#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <aplus.h>
#include <aplus/fs.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>


int zero_read(inode_t* ino, char* buffer, int size) {
	memset(buffer, 0, size);
	return size;
}


int init() {
	inode_t* ino = (inode_t*) devfs_makedevice("zero", S_IFCHR);
	if(!ino) {
		kprintf("zero: cannot create device\n");
		return -1;
	}


	ino->read = zero_read;
	return 0;
}


int dnit() {
	return 0;
}

