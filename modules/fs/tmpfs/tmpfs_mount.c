#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/vfs.h>
#include <xdev/mm.h>
#include <libc.h>

#include "tmpfs.h"

int tmpfs_mount(struct inode* dev, struct inode* dir) {
	(void) dev;


	dir->unlink = tmpfs_unlink;
	dir->mknod = tmpfs_mknod;

	return E_OK;
}
