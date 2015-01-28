#ifdef __rpi__

#include <aplus.h>
#include "rpi.h"

uint32_t lfbio[] = {
	LFB_WIDTH,
	LFB_HEIGHT,
	LFB_WIDTH,
	LFB_HEIGHT,
	0,
	LFB_DEPTH,
	0,
	0,
	0,
	0,
};



int lfb_init() {
	int i;
	uint32_t* p = (uint32_t*) &lfbio;

	for(i = 0; i < sizeof(lfbio); i += sizeof(uint32_t))
		mmio_w32(LFBIO_BASE + i, *p++);

	mail_send(LFBIO_BASE, LFBIO_BOX);
	mail_recv(LFBIO_BOX);


	mbd->lfb.width = mmio_r32(LFBIO_BASE + 0);
	mbd->lfb.height = mmio_r32(LFBIO_BASE + 4);
	mbd->lfb.pitch = mmio_r32(LFBIO_BASE + 16);
	mbd->lfb.depth = mmio_r32(LFBIO_BASE + 20);	
	mbd->lfb.base = mmio_r32(LFBIO_BASE + 32);
	mbd->lfb.size = mmio_r32(LFBIO_BASE + 36);


	kprintf("lfb: %dx%dx%d at 0x%x\n", mbd->lfb.width, mbd->lfb.height, mbd->lfb.depth, mbd->lfb.base);

	return 0;
}

#endif
