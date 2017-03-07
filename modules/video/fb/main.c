#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/debug.h>
#include <aplus/mm.h>
#include <libc.h>

#include <aplus/fbdev.h>

MODULE_NAME("video/fb");
MODULE_DEPS("");
MODULE_AUTHOR("Antonio Natale");
MODULE_LICENSE("GPL");



extern int (*stub_init) ();
extern int (*bochs_init) ();
extern int (*vesa_init) ();

extern int (*fb_window_init) ();




static int (*hooks[]) (void) = {
	(void*) &stub_init,
#if defined(__i386__) || defined(__x86_64__)
	(void*) &vesa_init,
	(void*) &bochs_init,
#endif
};

static fbdev_t __fbdev;
fbdev_t* fbdev = &__fbdev;


int init(void) {
	memset(fbdev, 0, sizeof(fbdev_t));

	int i;
	for(i = sizeof(hooks) / sizeof(void*); i > 0 ; i--)
		if(hooks[i - 1] () == 0)
			break;

	inode_t* ino;
	if(unlikely((ino = vfs_mkdev("fb", 0, S_IFCHR | 0666)) == NULL))
		return E_ERR;


	extern int fb_ioctl(struct inode*, int, void*);
	ino->ioctl = fb_ioctl;
	
	return E_OK;
}


int dnit(void) {
	if(fbdev->dnit)
		fbdev->dnit();

	return E_OK;
}
