#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <aplus.h>
#include <aplus/fs.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>


int urandom_read(inode_t* ino, char* ptr, int len) {
	int l = len;	
	while(l--)
		*ptr++ = rand() % 0xFF;

	return len;
}


int init() {
	inode_t* ino = (inode_t*) devfs_makedevice("urandom", S_IFCHR);
	if(!ino) {
		kprintf("urandom: cannot create device\n");
		return -1;
	}

	ino->read = urandom_read;
	return 0;
}


int dnit() {
	return 0;
}

