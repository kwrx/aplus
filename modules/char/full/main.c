#include <xdev.h>
#include <xdev/module.h>
#include <xdev/vfs.h>
#include <libc.h>

MODULE_NAME("char/full");
MODULE_DEPS("");
MODULE_AUTHOR("WareX");
MODULE_LICENSE("GPL");


static int full_read(struct inode* inode, void* buf, size_t size) {
	errno = ENOSPC;
	return 0;
}

int init(void) {
	inode_t* ino;
	if(unlikely((ino = vfs_mkdev("full", -1, S_IFCHR | 0666)) == NULL))
		return E_ERR;


	ino->read = full_read;
	return E_OK;
}



int dnit(void) {
	return E_OK;
}
