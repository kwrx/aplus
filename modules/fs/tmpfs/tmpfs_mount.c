#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "tmpfs.h"

int tmpfs_mount(struct inode* dev, struct inode* dir) {
	(void) dev;


	dir->unlink = tmpfs_unlink;
	dir->mknod = tmpfs_mknod;

	return E_OK;
}
