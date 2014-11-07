//
//  grub.h
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

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


	
	
	
	#define MULTIBOOT_FLAG_MEM 0x001
	#define MULTIBOOT_FLAG_DEVICE 0x002
	#define MULTIBOOT_FLAG_CMDLINE 0x004
	#define MULTIBOOT_FLAG_MODS 0x008
	#define MULTIBOOT_FLAG_AOUT 0x010
	#define MULTIBOOT_FLAG_ELF 0x020
	#define MULTIBOOT_FLAG_MMAP 0x040
	#define MULTIBOOT_FLAG_CONFIG 0x080
	#define MULTIBOOT_FLAG_LOADER 0x100
	#define MULTIBOOT_FLAG_APM 0x200
	#define MULTIBOOT_FLAG_VBE 0x400


	// Informazioni passate con il loader
	extern BootInfo* mbd;
	extern unsigned int magic;

	
#endif	
