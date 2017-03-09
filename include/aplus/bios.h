#ifndef _APLUS_BIOS_H
#define _APLUS_BIOS_H

#if !(defined(__i386__) || defined(__x86_64__))
#   error "(libbios) bios.h: invalid platform!"
#endif

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct vesa_info {
	char signature[4];
	unsigned short version;
	unsigned int oem_string_ptr;
	unsigned int capabilities;
	unsigned int video_mode_ptr;
	unsigned short total_memory;
	unsigned int oem_software_rev;
	char *oem_vendor_name;
	char *oem_product_name;
	char *oem_product_rev;
} vesa_info_t;

typedef struct vesa_mode_info {
	unsigned short attributes;
	unsigned char winA,winB;
	unsigned short granularity;
	unsigned short winsize;
	unsigned short segmentA, segmentB;
	unsigned int realFctPtr;
	unsigned short pitch;

	unsigned short Xres, Yres;
	unsigned char Wchar, Ychar, planes, bpp, banks;
	unsigned char memory_model, bank_size, image_pages;
	unsigned char reserved0;

	unsigned char red_mask, red_position;
	unsigned char green_mask, green_position;
	unsigned char blue_mask, blue_position;
	unsigned char rsv_mask, rsv_position;
	unsigned char directcolor_attributes;

	unsigned int physbase;
	unsigned int reserved1;
	short reserved2;
} __attribute__ ((packed)) vesa_mode_info_t;


void bios_print_string(const char* s);
int bios_get_drive_params(int drive, uint32_t* cyls, uint32_t* heads, uint32_t* sects);
int bios_read_disk(int drive, int cyl, int head, int sect, int nsect, void* buffer);
int bios_vesa_get_info(struct vesa_info* info);
int bios_vesa_get_mode_info(int mode, struct vesa_mode_info* info);
int bios_vesa_set_mode(int mode);

#ifdef __cplusplus
}
#endif

#endif