#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/spinlock.h>
#include <aplus/fb.h>
#include <aplus/attribute.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <errno.h>

#include <arch/i386/i386.h>
#include "../../fbdev.h"

#define VGA_IDX		0x3CE
#define VGA_DATA	0x3CF

static uint8_t cirrus_rop[] = {
	0x00, // clear
	0x05, // and
	0x09, // andreverse
	0x0D, // copy        Einfaches Setzen der Farbe
	0x50, // andinverted
	0x06, // noop
	0x59, // xor
	0x6D, // or
	0x95, // equiv
	0x0B, // invert
	0xAD, // orreverse
	0xD0, // copyinverted
	0xD6, // orinverted
	0xDA, // nand
	0x0E, // set
};


static int cirrus_set_rop(int rop) {
	outb(VGA_IDX, 0x32);
	outb(VGA_DATA, cirrus_rop[rop]);
}


void cirrus_fillrect(fb_rect_t* r) {
	cirrus_set_rop(3);

	outw(VGA_IDX, 0x0433);
	outw(VGA_IDX, 0xC030 | ((8) << 9));

	outw(VGA_IDX, ((r->color << 8) & 0xFF00) | 0x01);
	outw(VGA_IDX, ((r->color) & 0xFF00) | 0x11);
	outw(VGA_IDX, ((r->color >> 8) & 0xFF00) | 0x13);
	outw(VGA_IDX, 0x15);

	register int stride = r->surface->stride;
	outw(VGA_IDX, ((stride << 8) & 0xFF00) | 0x24);
	outw(VGA_IDX, ((stride) & 0x1F00) | 0x25);

	outw(VGA_IDX, (((r->w * (r->surface->depth >> 3) - 1) << 8) & 0xFF00) | 0x20);
	outw(VGA_IDX, (((r->w * (r->surface->depth >> 3) - 1) & 0x1F00) | 0x21));
	
	outw(VGA_IDX, (((r->h - 1) << 8) & 0xFF00) | 0x22);
	outw(VGA_IDX, (((r->h - 1) & 0x0700) | 0x23));

	register int dest = stride * r->y + r->x * (r->surface->depth >> 3);
	outw(VGA_IDX, ((dest << 8) & 0xFF00) | 0x28);
	outw(VGA_IDX, ((dest) & 0xFF00) | 0x29);
	outw(VGA_IDX, ((dest >> 8) & 0x3F00) | 0x2A);
}


void cirrus_copyrect(fb_rect_t** rs) {
	return;
}






int cirrus_init() {
	if(!pci_find_by_id(0x1013, 0x00B8))
		return -1;

	outb(VGA_IDX, 0x0E);
	outb(VGA_DATA, 0x20);
	outw(VGA_IDX, 0x8031);

	return 0;
}

int cirrus_dnit() {
	return 0;
}


fbdev_gfx_t cirrus_gfx = {
	"Cirrus Logic GD5446",
	1,
	cirrus_init,
	cirrus_dnit,
	cirrus_fillrect,
	cirrus_copyrect,
	NULL,
	NULL,
	NULL,
	NULL,
}; ATTRIBUTE("gfx", cirrus_gfx);
