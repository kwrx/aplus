#include <xdev.h>
#include <xdev/module.h>
#include <xdev/vfs.h>
#include <xdev/debug.h>
#include <xdev/mm.h>
#include <libc.h>

#include <aplus/fbdev.h>

extern fbdev_t* fbdev;

static fbdev_mode_t fb_settings = {
	1280,
	768,
	32,
	0,
	0,
	NULL
};

int fb_ioctl(struct inode* inode, int req, void* ptr) {
	
	#define cp(x)				\
		if(unlikely(!x)) {		\
			errno = EINVAL;		\
			return E_ERR;		\
		}
	
	switch(req) {
		case FBIOCTL_GETMODE:
			cp(ptr);
			memcpy(ptr, &fb_settings, sizeof(fb_settings));
			break;
		case FBIOCTL_SETMODE:
			cp(ptr);
			memcpy(&fb_settings, ptr, sizeof(fb_settings));
			break;
		case FBIOCTL_ENABLE:
			cp(fbdev);
			return fbdev->setvideomode(
				fb_settings.width,
				fb_settings.height,
				fb_settings.bpp,
				fb_settings.vx,
				fb_settings.vy,
				&fb_settings.lfbptr
			);
		default:
			errno = EINVAL;
			return E_ERR;
	}
	
	return E_OK;
}