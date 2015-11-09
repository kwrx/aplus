#include <xdev.h>
#include <xdev/module.h>
#include <xdev/vfs.h>
#include <xdev/debug.h>
#include <xdev/mm.h>
#include <libc.h>

#include <fbdev.h>

MODULE_NAME("video/fb");
MODULE_DEPS("");
MODULE_AUTHOR("WareX");
MODULE_LICENSE("GPL");



extern int (*stub_init) ();
extern int (*bochs_init) ();

static int (*hooks[]) (void) = {
	(void*) &stub_init,
#if defined(__i386__) || defined(__x86_64__)
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

	void* p = NULL;
	if(fbdev->setvideomode)
		fbdev->setvideomode(800, 600, 32, 0, 0, &p);


	kprintf(LOG, "fb: initialized \"%s\" at %x\n", fbdev->name, p);
	return 0;
}


int dnit(void) {
	if(fbdev->dnit)
		fbdev->dnit();

	return 0;
}
