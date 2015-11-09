#include <xdev.h>
#include <xdev/module.h>
#include <xdev/vfs.h>
#include <libc.h>

#include <fbdev.h>

#if defined(__i386__)
#include <arch/i386/i386.h>
#elif defined(__x86_64__)
#include <arch/x86_64/x86_64.h>
#endif


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


extern fbdev_t* fbdev;

#if defined(__i386__) || defined(__x86_64__)

int bga_setvideomode(uint16_t width, uint16_t height, uint16_t depth, uint16_t vx, uint16_t vy, void** lfbptr) {


	#define wr(i, v)				\
		outw(VBE_DISPI_IOPORT_INDEX, i);	\
		outw(VBE_DISPI_IOPORT_DATA, v);

	wr(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
	wr(VBE_DISPI_INDEX_XRES, width);
	wr(VBE_DISPI_INDEX_YRES, height);
	wr(VBE_DISPI_INDEX_BPP, depth);
	wr(VBE_DISPI_INDEX_X_OFFSET, vx);
	wr(VBE_DISPI_INDEX_Y_OFFSET, vy);
	wr(VBE_DISPI_INDEX_VIRT_WIDTH, width);
	wr(VBE_DISPI_INDEX_VIRT_HEIGHT, height);



	if(!mbd->lfb.base) {
		uint32_t* vmem = (uint32_t*) 0xA0000;
		vmem[0] = 0x0BADC0DE;

		uintptr_t base;
		for(base = 0xE0000000; base < 0xFF000000; base += 0x1000)
			if(*(uint32_t*) base == 0x0BADC0DE)
				break;

		if(base == 0xFF000000)
			return -1;

		if(lfbptr)
			*lfbptr = (void*) base;
	} else {
		if(lfbptr)
			*lfbptr = (void*) mbd->lfb.base;
	}


	wr(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
	return 0;
}

int bochs_init(void) {

	outw(VBE_DISPI_IOPORT_INDEX, 0);
	
	int n = inw(VBE_DISPI_IOPORT_DATA);
	if(!(CHECK_BGA(n)))
		return -1;


	fbdev->name = "Bochs VBE Extensions";
	fbdev->setvideomode = bga_setvideomode;
	return 0;
#else
int bga_init(void) {
	return -1;
#endif
}
