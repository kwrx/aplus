#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/vfs.h>
#include <xdev/mm.h>
#include <libc.h>

#include "iso9660.h"

int iso9660_read(inode_t* ino, void* buf, size_t size) {
	if(unlikely(!ino))
		return 0;
		
	if(unlikely(!buf))
		return 0;
		
	if(unlikely((off64_t) size > ino->size))
		size = ino->size;
		
	if(unlikely(ino->position > ino->size))
		ino->position = ino->size;
		
	if(unlikely(ino->position + (off64_t) size > ino->size))
		size = ino->size - ino->position;
		
	if(unlikely(!size))
		return 0;

	iso9660_t* ctx = (iso9660_t*) ino->userdata;
	if(unlikely(!ctx))
		return 0;

	
	ctx->dev->position = (iso9660_getlsb32(ctx->dir.lba) * ISO9660_SECTOR_SIZE) + ino->position;
	size = vfs_read(ctx->dev, buf, size);


	ino->position += (off64_t) size;
	return size;
}
