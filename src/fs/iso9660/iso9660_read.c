#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/task.h>
#include <aplus/spinlock.h>
#include <aplus/mm.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>

#include "iso9660.h"


int iso9660_read(inode_t* ino, char* buf, int size) {
	if(unlikely(!ino))
		return 0;
		
	if(unlikely(!buf))
		return 0;
		
	if(unlikely(size > ino->size))
		size = ino->size;
		
	if(unlikely(ino->position > ino->size))
		ino->position = ino->size;
		
	if(unlikely(ino->position + size > ino->size))
		size = ino->size - ino->position;
		
	if(unlikely(!size))
		return 0;

	inode_t* dev = (inode_t*) devfs_getdevice(ino->dev);
	if(unlikely(!dev))
		return 0;

	
	dev->position = (off_t) ino->userdata + ino->position;
	fs_read(dev, buf, size);

	ino->position += size;
	return size;
}
