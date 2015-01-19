#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/task.h>
#include <aplus/spinlock.h>
#include <aplus/fsys.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>

#include "iso9660.h"

extern struct dirent* iso9660_readdir(inode_t*, int);
extern inode_t* iso9660_finddir(inode_t*, char*);


int iso9660_mount(inode_t* dev, inode_t* ino, int flags) {
	if(unlikely(!dev))
		return -1;
		
	if(unlikely(!ino))
		return -1;
	
	if(unlikely(iso9660_check(dev) != 0)) {
		kprintf("iso9660: (%s) check failed\n", dev->name);
		return -1;
	}

	ino->userdata = (void*) iso9660_getroot();
	ino->readdir = iso9660_readdir;
	ino->finddir = iso9660_finddir;

	
	return 0;
}

FSYS(iso9660, iso9660_mount);
