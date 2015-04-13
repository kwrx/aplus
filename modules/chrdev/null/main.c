#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <aplus.h>
#include <aplus/fs.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int init() {
	inode_t* ino = (inode_t*) devfs_makedevice("null", S_IFCHR);
	if(!ino) {
		kprintf("null: cannot create device\n");
		return -1;
	}

	
	return 0;
}


int dnit() {
	return 0;
}

