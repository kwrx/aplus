#ifdef __rpi__
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

#include <arch/rpi/rpi.h>
#include "../../fbdev.h"


volatile struct lfbio_t {
	volatile uint32_t width;
	volatile uint32_t height;
	volatile uint32_t vwidth;
	volatile uint32_t vheight;
	volatile uint32_t pitch;
	volatile uint32_t depth;
	volatile uint32_t vx;
	volatile uint32_t vy;
	volatile uint32_t base;
	volatile uint32_t size;
} __packed *lfbio = (volatile struct lfbio_t*) LFBIO_BASE;


int rpi_setvideomode(fb_videomode_t* vm) {
	if(vm->vmode == FBVM_TEXT)
		return -1;

	if(lfbio == NULL)
		return -1;

	if(
		vm->width == 0	||
		vm->height == 0 ||
		vm->depth == 0
	) return -1;

	
	lfbio->width = vm->width;
	lfbio->height = vm->height;
	lfbio->depth = vm->depth;
	lfbio->vwidth = vm->vwidth;
	lfbio->vheight = vm->vheight;
	lfbio->vx = vm->vx;
	lfbio->vy = vm->vy;
	
	mail_send(LFBIO_BASE, LFBIO_BOX);
	mail_recv(LFBIO_BOX);

	vm->pitch = lfbio->pitch;
	vm->base = lfbio->base;
	vm->size = lfbio->size;

	return 0;
}

int rpi_init(void) {
	return 0;
}

int rpi_dnit(void) {
	return 0;
}

fbdev_gfx_t rpi_gfx = {
	"RasperryPi VGA",
	0,
	rpi_init,
	rpi_dnit,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	rpi_setvideomode,
}; ATTRIBUTE("gfx", bga_gfx);

#endif
