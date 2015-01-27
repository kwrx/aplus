#ifdef __rpi__

#include <aplus.h>
#include <stdint.h>

int arch_init() {
	serial_init();
	lfb_init();

	return 0;
}


void go_usermode() {
	return;
}


void cpu_idle() {
	
}

void cpu_wait() {
	
}



#define ATAG_NONE       0x00000000
#define ATAG_CORE       0x54410001
#define ATAG_MEM        0x54410002
#define ATAG_VIDEOTEXT  0x54410003
#define ATAG_RAMDISK    0x54410004
#define ATAG_INITRD2    0x54420005
#define ATAG_SERIAL     0x54410006
#define ATAG_REVISION   0x54410007
#define ATAG_VIDEOLFB   0x54410008
#define ATAG_CMDLINE    0x5441000

typedef struct atag {
	struct {
		uint32_t size;
		uint32_t tag;
	} header;

	union {
		struct {
			uint32_t flags;
			uint32_t pagesize;
			uint32_t rootdev;
		} core;

		struct {
			uint32_t size;
			uint32_t start;
		} memory;

		struct {
			uint8_t x;
			uint8_t y;
			uint16_t page;
			uint8_t mode;
			uint8_t cols;
			uint16_t ega_bx;
			uint8_t lines;
			uint8_t isvga;
			uint16_t points;
		} video;

		struct {
			uint32_t flags;
			uint32_t size;
			uint32_t start;
		} ramdisk;

		struct {
			uint32_t start;
			uint32_t size;
		} initrd2;

		struct {
			uint32_t low;
			uint32_t high;
		} serial;

		struct {
			uint32_t rev;
		} revision;

		struct {
			uint16_t width;
			uint16_t height;
			uint16_t depth;
			uint16_t pitch;
			uint32_t base;
			uint32_t size;
		
			struct {
				uint8_t size;
				uint8_t pos;
			} red;

			struct {
				uint8_t size;
				uint8_t pos;
			} green;

			struct {
				uint8_t size;
				uint8_t pos;
			} blue;

			uint8_t rsvd_size;
			uint8_t rsvd_pos;
		} lfb;


		struct {
			char args[1];
		} cmdline;
	} c;
} atag_t;


void rpi_save_args(int unused, int armtype, atag_t* atags) {
	#define t_next(t)		((atag_t*) ((uint32_t) (t)) + (t)->header.size)

	if(unlikely(atags))
		atags = (atag_t*) 0x100;

	for(;;) {
		switch(atags->header.tag) {
			case ATAG_CORE:
				mbd->memory.pagesize = atags->c.core.pagesize;
				mbd->flags = atags->c.core.flags;				
				break;
			case ATAG_MEM:
				mbd->memory.size = atags->c.memory.size;
				break;
			case ATAG_RAMDISK:
				mbd->ramdisk.ptr = atags->c.ramdisk.start;
				mbd->ramdisk.size = atags->c.ramdisk.size;
				mbd->memory.start = mbd->ramdisk.ptr + mbd->ramdisk.size;
				break;
			case ATAG_VIDEOLFB:
				mbd->lfb.width = atags->c.lfb.width;
				mbd->lfb.height = atags->c.lfb.height;
				mbd->lfb.depth = atags->c.lfb.depth;
				mbd->lfb.pitch = atags->c.lfb.pitch;
				mbd->lfb.base = atags->c.lfb.base;
				mbd->lfb.size = atags->c.lfb.size;
				break;
			case ATAG_CMDLINE:
				mbd->cmdline.args = atags->c.cmdline.args;
				mbd->cmdline.length = strlen(mbd->cmdline.args);
				break;
			case ATAG_VIDEOTEXT:
			case ATAG_INITRD2:
			case ATAG_SERIAL:
			case ATAG_REVISION:
				break;
			case ATAG_NONE:
			default:
				atags = (atag_t*) 0x8000;
				break;
		}

		if(unlikely(atags >= (atag_t*) 0x8000))
			break;

		atags = t_next(atags);
	}
}


#endif
