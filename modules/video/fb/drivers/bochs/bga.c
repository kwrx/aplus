#include <xdev.h>
#include <xdev/module.h>
#include <xdev/vfs.h>
#include <xdev/mm.h>
#include <xdev/debug.h>
#include <libc.h>

#include <aplus/fbdev.h>

#if defined(__i386__)
#include <arch/i386/i386.h>
#include <arch/i386/pci.h>
#include <arch/i386/pci_list.h>
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

#define CHECK_BGA(n) 					(n >= 0xB0C0 || n <= 0xB0C5)
#define BGA_VIDEORAM_SIZE				0x1000000


extern fbdev_t* fbdev;
static fbdev_mode_t fbmode;
static uintptr_t __lfbptr = 0;

#if defined(__i386__) || defined(__x86_64__)


int bga_getvideomode(fbdev_mode_t* m) {
	
	#define rd(i, v)										\
		outw(VBE_DISPI_IOPORT_INDEX, i);					\
		(v) = inw(VBE_DISPI_IOPORT_DATA);
		
	
	rd(VBE_DISPI_INDEX_XRES, m->width);
	rd(VBE_DISPI_INDEX_YRES, m->height);
	rd(VBE_DISPI_INDEX_BPP, m->bpp);
	rd(VBE_DISPI_INDEX_X_OFFSET, m->vx);
	rd(VBE_DISPI_INDEX_Y_OFFSET, m->vy);
	rd(VBE_DISPI_INDEX_VIRT_WIDTH, m->width);
	rd(VBE_DISPI_INDEX_VIRT_HEIGHT, m->height);
	
	m->lfbptr = (void*) __lfbptr;
	memcpy(&fbmode, m, sizeof(fbdev_mode_t));
	
	return E_OK;
}

int bga_setvideomode(fbdev_mode_t* m) {


	#define wr(i, v)										\
		outw(VBE_DISPI_IOPORT_INDEX, i);					\
		outw(VBE_DISPI_IOPORT_DATA, v);


	wr(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
	wr(VBE_DISPI_INDEX_XRES, m->width);
	wr(VBE_DISPI_INDEX_YRES, m->height);
	wr(VBE_DISPI_INDEX_BPP, m->bpp);
	wr(VBE_DISPI_INDEX_X_OFFSET, m->vx);
	wr(VBE_DISPI_INDEX_Y_OFFSET, m->vy);
	wr(VBE_DISPI_INDEX_VIRT_WIDTH, m->width);
	wr(VBE_DISPI_INDEX_VIRT_HEIGHT, m->height);

	m->lfbptr = (void*) __lfbptr;
	memcpy(&fbmode, m, sizeof(fbdev_mode_t));

	wr(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
	fbdev->enabled = 1;
	return E_OK;
}


int bga_update_surface(fbdev_surface_t* surface) {
	uintptr_t stride = fbmode.bpp << 3;
	uintptr_t p = __lfbptr + ((surface->y * fbmode.width * stride) + surface->x * stride);
	uintptr_t k = (uintptr_t) surface->ptr;
	uintptr_t h = surface->height;
	
	register int i, j;
	for(i = p, j = k; h--; i += stride, j += surface->stride)
		memcpy((void*) p, (void*) j, surface->stride);
		
	return 0;
}

int bochs_init(void) {

	outw(VBE_DISPI_IOPORT_INDEX, 0);
	
	int n = inw(VBE_DISPI_IOPORT_DATA);
	if(!(CHECK_BGA(n)))
		return E_ERR;


	__lfbptr = 0;

	void pci_func(uint32_t device, uint16_t vendor_id, uint16_t device_id, void* arg) {
		
		if(likely(!(
			(vendor_id == 0x1234) &&
			(device_id == 0x1111)
		))) return;
		
		__lfbptr = (uintptr_t) pci_read_field(device, PCI_BAR0, 4);
	}

	int i;
	for(i = 0; i < 65536 && !__lfbptr; i++)
		pci_scan(&pci_func, i, NULL);

	if(!__lfbptr)
		return E_ERR;


	#define ALIGN(x)										\
		(((x) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1))
	
	if(__lfbptr) {
		uintptr_t frame = ALIGN(__lfbptr) - PAGE_SIZE;
		uintptr_t end = ALIGN(frame + BGA_VIDEORAM_SIZE);

		for(; frame < end; frame += PAGE_SIZE)
			map_page(frame, frame, 1);
	} else
		return E_ERR;
	


	fbdev->name = "Bochs VBE Extensions";
	fbdev->setvideomode = bga_setvideomode;
	fbdev->getvideomode = bga_getvideomode;
	fbdev->update_surface = bga_update_surface;
	
	return E_OK;
#else
int bga_init(void) {
	return E_ERR;
#endif
}
