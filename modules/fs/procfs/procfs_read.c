#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/vfs.h>
#include <xdev/mm.h>
#include <libc.h>

#include "procfs.h"


int procfs_read(struct inode* inode, void* ptr, size_t len) {
	if(unlikely(!inode))
		return E_ERR;

	if(unlikely(!ptr))
		return E_ERR;

	if(unlikely(!inode->userdata))
		return 0;

	if(inode->position + len > inode->size)
		len = inode->size - inode->position;

	if(unlikely(!len))
		return 0;

	memcpy(ptr, (void*) ((uintptr_t) inode->userdata + (uintptr_t) inode->position), len);
	inode->position += len;

	return len;
}
