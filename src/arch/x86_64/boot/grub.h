#ifndef _GRUB_H
#define _GRUB_H


typedef struct vesaInfo_t {
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
} vesaInfo;

typedef struct vesaModeInfo_t {
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
} __attribute__ ((packed)) vesaModeInfo;

typedef struct BootInfo_t {
	unsigned int flags;
	unsigned int mem_lower;
	unsigned int mem_upper;
	unsigned int boot_device;
	unsigned int cmdline;
	unsigned int mods_count;
	unsigned int** mods_addr;
	unsigned int num;
	unsigned int size;
	unsigned int addr;
	unsigned int shndx;
	unsigned int mmap_length;
	unsigned int mmap_addr;
	unsigned int drives_length;
	unsigned int drives_addr;
	unsigned int config_table;
	unsigned int boot_loader_name;
	unsigned int apm_table;
	unsigned int vbe_control_info;
	vesaModeInfo* vbe_mode_info;
	vesaInfo* vbe_mode;
	unsigned int vbe_interface_seg;
	unsigned int vbe_interface_off;
	unsigned int vbe_interface_len;
} BootInfo;

#endif	
