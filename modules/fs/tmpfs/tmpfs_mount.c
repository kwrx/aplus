#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "tmpfs.h"

int tmpfs_mount(struct inode* dev, struct inode* dir) {
	(void) dev;


	dir->open = NULL;
	dir->close = NULL;
	dir->unlink = tmpfs_unlink;
	dir->mknod = tmpfs_mknod;
	dir->finddir = NULL;

	return E_OK;
}
