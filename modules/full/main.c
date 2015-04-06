#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <aplus.h>
#include <aplus/fs.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <errno.h>


int full_read(inode_t* ino, char* ptr, int len) {
	return len;
}

int full_write(inode_t* ino, char* ptr, int len) {
	errno = ENOSPC;	
	return 0;
}

int init() {
	inode_t* ino = (inode_t*) devfs_makedevice("full", S_IFCHR);
	if(!ino) {
		kprintf("full: cannot create device\n");
		return -1;
	}

	ino->write = full_write;
	return 0;
}


int dnit() {
	return 0;
}

