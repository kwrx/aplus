#include <xdev.h>
#include <xdev/module.h>
#include <xdev/vfs.h>
#include <xdev/mm.h>
#include <libc.h>

#include <aplus/gshm.h>

/* Global Shared Memory */

MODULE_NAME("char/gshm");
MODULE_DEPS("");
MODULE_AUTHOR("Antonio Natale");
MODULE_LICENSE("GPL");

static int gshm_ioctl(struct inode* inode, int req, void* ptr) {
	#define cp(x)				\
		if(unlikely(!x)) {		\
			errno = EINVAL;		\
			return E_ERR;		\
		}
	
	switch(req) {
		case GSHMIOCTL_ALLOC:
			cp(ptr);
			*((uintptr_t*) ptr) = (uintptr_t) kmalloc(*((uintptr_t*) ptr), GFP_USER);
			
			break;
		case GSHMIOCTL_FREE:
			cp(ptr);
			kfree(ptr);
				
			break;
		default:
			errno = EINVAL;
			return E_ERR;
	}
	
	return E_OK;
}

int init(void) {
	inode_t* ino;
	if(unlikely((ino = vfs_mkdev("gshm", -1, S_IFCHR | 0666)) == NULL))
		return E_ERR;


	ino->ioctl = gshm_ioctl;
	return E_OK;
}



int dnit(void) {
	return E_OK;
}
