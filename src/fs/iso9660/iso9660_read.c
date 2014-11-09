#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/task.h>
#include <aplus/spinlock.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>

#include "iso9660.h"


int iso9660_read(inode_t* ino, char* buf, int size) {
	if(!ino)
		return 0;
		
	if(!buf)
		return 0;
		
	if(size > ino->size)
		size = ino->size;
		
	if(ino->position > ino->size)
		ino->position = ino->size;
		
	if(ino->position + size > ino->size)
		size = ino->size - ino->position;
		
	if(!size)
		return 0;

	inode_t* dev = (inode_t*) devfs_getdevice(ino->dev);
	if(!dev)
		return 0;

	void* tbuf = (void*) kmalloc(((size / ISO9660_SECTOR_SIZE) + 1) * ISO9660_SECTOR_SIZE);
	
	dev->position = (off_t) ino->userdata + (ino->position / ISO9660_SECTOR_SIZE);
	fs_read(dev, tbuf, ((size / ISO9660_SECTOR_SIZE) + 1) * ISO9660_SECTOR_SIZE);

	memcpy(buf, (void*) ((uint32_t) tbuf + (ino->position % ISO9660_SECTOR_SIZE)), size);
	kfree(tbuf);

	return size;
}
