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

#include <arch/i386/i386.h>
#include "../../fbdev.h"


int __stub_gfx_init(void) {
	return 0;
}

fbdev_gfx_t stub_gfx = {
	"VGA Controller",
	0,
	__stub_gfx_init,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};
fbdev_gfx_t* __stub_gfx = &stub_gfx;

