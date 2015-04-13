#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/spinlock.h>
#include <aplus/fb.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <errno.h>

#include "fbdev.h"



extern fbdev_gfx_t* __stub_gfx;


#define __FB_WIDTH		800
#define __FB_HEIGHT		600
#define __FB_DEPTH		16

fb_videomode_t fbvm = {
	.vmode = FBVM_LFB,
	.width = __FB_WIDTH,
	.height = __FB_HEIGHT,
	.depth = __FB_DEPTH,
	.vx = 0,
	.vy = 0,
	.vwidth = __FB_WIDTH,
	.vheight = __FB_HEIGHT,
	.pitch = 0,
	.size = 0,
	.base = 0,
};


fbdev_gfx_t* fbgfx = NULL;
spinlock_t fb_lock = 0;

int fb_ioctl(inode_t* ino, int req, void* buf) {
	if(unlikely(!ino)) {
		errno = EINVAL;
		return -1;
	}

	spinlock_lock(&fb_lock);
	int e = 0;

	switch(req) {
		case FBIO_DPMS_ACTIVE_OFF:
		case FBIO_DPMS_SUSPEND:
		case FBIO_DPMS_STANDBY:
			errno = ENOSYS;
			e = -1;
			break;
		case FBIO_VMGET:
			if(unlikely(!buf)) {
				errno = EINVAL;
				e = -1;
				break;
			}
		
			memcpy(buf, &fbvm, sizeof(fb_videomode_t));
			break;
		case FBIO_VMSET:
			if(unlikely(!buf)) {
				errno = EINVAL;
				e = -1;
				break;
			}

			if(unlikely(fbgfx == NULL)) {
				errno = ENOSYS;
				e = -1;
				break;
			}

			if(unlikely(fbgfx->setvideomode == NULL)) {
				errno = ENOSYS;
				e = -1;
				break;
			}

			if(likely(fbgfx->setvideomode((fb_videomode_t*) buf) == 0))
				memcpy(&fbvm, buf, sizeof(fb_videomode_t));
			break;
		case FBIO_HW_AVAILABLE:
			if(unlikely(fbgfx == NULL)) {
				errno = ENOSYS;
				e = -1;
				break;
			}

			if(fbgfx->hw_isavail)
				break;

			e = -1;
			break;
		case FBIO_HW_FILLRECT:
			if(unlikely(fbgfx == NULL)) {
				errno = ENOSYS;
				e = -1;
				break;
			}

			if(unlikely(fbgfx->hw_fillrect == NULL)) {
				errno = ENOSYS;
				e = -1;
				break;
			}

			fbgfx->hw_fillrect((fb_rect_t*) buf);
			break;
		case FBIO_HW_COPYRECT:
			if(unlikely(fbgfx == NULL)) {
				errno = ENOSYS;
				e = -1;
				break;
			}

			if(unlikely(fbgfx->hw_copyrect == NULL)) {
				errno = ENOSYS;
				e = -1;
				break;
			}

			fbgfx->hw_copyrect((fb_rect_t**) buf);
			break;
		case FBIO_HW_SURFACE_ALLOC:
			if(unlikely(fbgfx == NULL)) {
				errno = ENOSYS;
				e = -1;
				break;
			}

			if(unlikely(fbgfx->hw_surface_alloc == NULL)) {
				errno = ENOSYS;
				e = -1;
				break;
			}

			fbgfx->hw_surface_alloc((fb_surface_t*) buf);
			break;
		case FBIO_HW_SURFACE_DESTROY:
			if(unlikely(fbgfx == NULL)) {
				errno = ENOSYS;
				e = -1;
				break;
			}
			
			if(unlikely(fbgfx->hw_surface_destroy == NULL)) {
				errno = ENOSYS;
				e = -1;
				break;
			}

			fbgfx->hw_surface_destroy((fb_surface_t*) buf);
			break;
	}

	spinlock_unlock(&fb_lock);
	return e;
}


int fb_read(inode_t* ino, char* buf, int length) {
	if(unlikely(!ino || !buf || !length || !fbvm.base)) {
		errno = EINVAL;
		return 0;
	}

	if(unlikely(ino->position + length > fbvm.size))
		length = fbvm.size - ino->position;

	if(unlikely(length < 0))
		return 0;

	memcpy(buf, (void*) (ino->position + fbvm.base), length);
	ino->position += length;
	return length;
}

int fb_write(inode_t* ino, char* buf, int length) {
	if(unlikely(!ino || !buf || !length || !fbvm.base)) {
		errno = EINVAL;
		return 0;
	}

	if(unlikely(ino->position + length > fbvm.size))
		length = fbvm.size - ino->position;

	if(unlikely(length < 0))
		return 0;

	memcpy((void*) (ino->position + fbvm.base), buf, length);
	ino->position += length;
	return length;
}

int init() {
	spinlock_init(&fb_lock, SPINLOCK_FLAGS_UNLOCKED);	
	spinlock_lock(&fb_lock);
	
	spinlock_unlock(&fb_lock);

	
	list_t* gfx = attribute("gfx");
	list_foreach(v, gfx) {
		fbdev_gfx_t* fx = (fbdev_gfx_t*) v;

		if(unlikely(!fx->init))
			continue;

		if(fx->init() == 0) {
			fbgfx = fx;
			break;
		}
	}

	list_destroy(gfx);

	if(likely(fbgfx)) {
		if(likely(fbgfx->setvideomode))
			fbgfx->setvideomode(&fbvm);
	} else
		fbgfx = __stub_gfx;


	inode_t* ino = (inode_t*) devfs_makedevice("fb0", S_IFBLK);
	if(unlikely(!ino)) {
		kprintf("fb0: could not create device!\n");
		return -1;
	}

	ino->read = fb_read;
	ino->write = fb_write;
	ino->ioctl = fb_ioctl;

	kprintf("fb0: %s (%dx%dx%d) HW: %d\n", fbgfx->name, fbvm.width, fbvm.height, fbvm.depth, fbgfx->hw_isavail);
	return 0;
}


int dnit() {
	return 0;
}

