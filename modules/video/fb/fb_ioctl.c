#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/debug.h>
#include <aplus/mm.h>
#include <libc.h>

#include <aplus/fbdev.h>

extern fbdev_t* fbdev;


int fb_ioctl(struct inode* inode, int req, void* ptr) {
	
	#define cp(x)				\
		if(unlikely(!x)) {		\
			errno = EINVAL;		\
			return E_ERR;		\
		}
	
	switch(req) {
		case FBIOCTL_GETMODE:
			cp(ptr);
			
			if(fbdev->getvideomode)
				return fbdev->getvideomode(ptr);
			
			break;
		case FBIOCTL_SETMODE:
			cp(ptr);
			
			if(fbdev->setvideomode)
				return fbdev->setvideomode(ptr);
				
			break;
		default:
			errno = EINVAL;
			return E_ERR;
	}
	
	errno = ENOSYS;
	return E_ERR;
}