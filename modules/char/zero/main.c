#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <libc.h>

MODULE_NAME("char/zero");
MODULE_DEPS("");
MODULE_AUTHOR("Antonio Natale");
MODULE_LICENSE("GPL");


static int zero_read(struct inode* inode, void* buf, size_t size) {
	char* bc = (char*) buf;
	size_t i;
	for(i = 0; i < size; i++)
		*bc++ = '\0';

	return size;
}

int init(void) {
	inode_t* ino;
	if(unlikely((ino = vfs_mkdev("zero", -1, S_IFCHR | 0666)) == NULL))
		return E_ERR;


	ino->read = zero_read;
	return E_OK;
}



int dnit(void) {
	return E_OK;
}
