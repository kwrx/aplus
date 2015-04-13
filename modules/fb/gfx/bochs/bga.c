#ifdef __i386__

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

#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA 0x01CF
#define VBE_DISPI_INDEX_ID 0x0
#define VBE_DISPI_INDEX_XRES 0x1
#define VBE_DISPI_INDEX_YRES 0x2
#define VBE_DISPI_INDEX_BPP 0x3
#define VBE_DISPI_INDEX_ENABLE 0x4
#define VBE_DISPI_INDEX_BANK 0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH 0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 0x7
#define VBE_DISPI_INDEX_X_OFFSET 0x8
#define VBE_DISPI_INDEX_Y_OFFSET 0x9

#define VBE_DISPI_DISABLED 0x00
#define VBE_DISPI_ENABLED 0x01
#define VBE_DISPI_GETCAPS 0x02
#define VBE_DISPI_8BIT_DAC 0x20
#define VBE_DISPI_LFB_ENABLED 0x40
#define VBE_DISPI_NOCLEARMEM 0x80

#define CHECK_BGA(n) (n >= 0xB0C0 || n <= 0xB0C5)


#define wr(i, v)	{							\
		outw(VBE_DISPI_IOPORT_INDEX, i);		\
		outw(VBE_DISPI_IOPORT_DATA, v);			\
	}


int bga_setvideomode(fb_videomode_t* vm) {
	if(vm->vmode == FBVM_TEXT)
		return -1;

	if(
		vm->width == 0	||
		vm->height == 0 ||
		vm->depth == 0
	) return -1;

	wr(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
	wr(VBE_DISPI_INDEX_XRES, vm->width);
	wr(VBE_DISPI_INDEX_YRES, vm->height);
	wr(VBE_DISPI_INDEX_BPP, vm->depth);
	wr(VBE_DISPI_INDEX_X_OFFSET, vm->vx);
	wr(VBE_DISPI_INDEX_Y_OFFSET, vm->vy);
	wr(VBE_DISPI_INDEX_VIRT_WIDTH, vm->vwidth);
	wr(VBE_DISPI_INDEX_VIRT_HEIGHT, vm->vheight);



	if(!mbd->lfb.base) {
		uint32_t* vmem = (uint32_t*) 0xA0000;
		vmem[0] = 0x0BADC0DE;

		for(vm->base = 0xE0000000; vm->base < 0xFF000000; vm->base += 0x1000)
			if(*(uint32_t*) vm->base == 0x0BADC0DE)
				break;

		if(vm->base == 0xFF000000)
			return -1;
	} else
		vm->base = mbd->lfb.base;

	wr(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);



	vm->size = vm->width * vm->height * (vm->depth / 8);
	vm->pitch = vm->width * (vm->depth / 8);

	return 0;
}

int bga_init(void) {
	outw(VBE_DISPI_IOPORT_INDEX, 0);
	int n = inw(VBE_DISPI_IOPORT_DATA);

	if(!CHECK_BGA(n))
		return -1;

	return 0;
}

int bga_dnit(void) {
	return 0;
}

fbdev_gfx_t bga_gfx = {
	"Bochs VBE Extensions",
	0,
	bga_init,
	bga_dnit,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	bga_setvideomode,
}; ATTRIBUTE("gfx", bga_gfx);

#endif
