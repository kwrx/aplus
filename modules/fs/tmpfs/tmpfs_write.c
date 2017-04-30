#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "tmpfs.h"


int tmpfs_write(struct inode* inode, void* ptr, size_t len) {
	if(unlikely(!inode))
		return 0;

	if(unlikely(!ptr))
		return 0;

	if(unlikely(!len))
		return 0;

	if(inode->position + (off64_t) len > inode->size) {
		void* np = (void*) kmalloc(inode->position + len, GFP_USER);
		

		if(likely(inode->userdata)) {
			memcpy(np, inode->userdata, inode->size);		
			kfree(inode->userdata);
		}

		inode->size = inode->position + (off64_t) len;
		inode->userdata = np;
	}

	memcpy((void*) ((uintptr_t) inode->userdata + (uintptr_t) inode->position), ptr, len);
	inode->position += len;

	return len;
}