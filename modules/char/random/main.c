#include <xdev.h>
#include <xdev/module.h>
#include <xdev/vfs.h>
#include <xdev/ipc.h>
#include <xdev/debug.h>
#include <libc.h>

MODULE_NAME("char/random");
MODULE_DEPS("");
MODULE_AUTHOR("WareX");
MODULE_LICENSE("GPL");



static mutex_t rnd_lock = MTX_INIT(MTX_KIND_DEFAULT);

static int random_read(struct inode* inode, void* buf, size_t size) {

	kprintf(INFO, "Size: %d\n", size);

	mutex_lock(&rnd_lock);
	srand(sys_times(NULL));

	char* bc = (char*) buf;
	size_t i;
	for(i = 0; i < size; i++)
		*bc++ = rand() % 256;

	mutex_unlock(&rnd_lock);
	return size;
}

int init(void) {
	inode_t* ino;
	if(unlikely((ino = vfs_mkdev("random", -1, S_IFCHR | 0666)) == NULL))
		return E_ERR;


	ino->read = random_read;
	return E_OK;
}



int dnit(void) {
	return E_OK;
}