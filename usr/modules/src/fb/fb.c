//
//  fb.c
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

#include <grub.h>
#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/ioctl.h>


#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>



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

#define VBE_DEFAULT_LFB 0xE0000000
#define CHECK_BGA(n) (n < 0xB0C0 || n > 0xB0C5)


static int width, height, bpp;

static bga_write(uint16_t index, uint16_t value) {
	outw(VBE_DISPI_IOPORT_INDEX, index);
	outw(VBE_DISPI_IOPORT_DATA, value);
}

static int video_reset() {
	int fd = open("/dev/tty0", O_RDONLY, 0644);
	if(fd < 0)
		return -1;
		
	ioctl(fd, IOCTL_TTY_RESET, 0);
	close(fd);
	
	return 0;
}

int fb_ioctl(struct inode* ino, int req, void* buf) {
	switch(req) {
		case IOCTL_FB_DISABLE:
			bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
			break;
		case IOCTL_FB_ENABLE:
			bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
			break;
		case IOCTL_FB_SETWIDTH:
			width = *(int*) buf;
			bga_write(VBE_DISPI_INDEX_XRES, width);
			break;
		case IOCTL_FB_SETHEIGHT:
			height = *(int*) buf;
			bga_write(VBE_DISPI_INDEX_YRES, height);
			break;
		case IOCTL_FB_SETBPP:
			bpp = *(int*) buf;
			bga_write(VBE_DISPI_INDEX_BPP, bpp);
			break;
		case IOCTL_FB_GETWIDTH:
			*(int*) buf = width;
			break;
		case IOCTL_FB_GETHEIGHT:
			*(int*) buf = height;
			break;
		case IOCTL_FB_GETBPP:
			*(int*) buf = bpp;
			break;
		case IOCTL_FB_GETLFB:
			*(int*) buf = VBE_DEFAULT_LFB;
			break;
		case IOCTL_FB_SETVX:
			bga_write(VBE_DISPI_INDEX_X_OFFSET, *(int*) buf);
			break;
		case IOCTL_FB_SETVY:
			bga_write(VBE_DISPI_INDEX_Y_OFFSET, *(int*) buf);
			break;
		case IOCTL_FB_SETVW:
			bga_write(VBE_DISPI_INDEX_VIRT_WIDTH, *(int*) buf);
			break;
		case IOCTL_FB_SETVH:
			bga_write(VBE_DISPI_INDEX_VIRT_HEIGHT, *(int*) buf);
			break;
		case IOCTL_FB_CLOSE:
			return video_reset();
		default:
			return -1;
	}
	
	return 0;
}


int cmain(int argc, char** argv) {
	
	inode_t* dev = aplus_device_create("/dev/fb0", S_IFBLK);
	if(!dev) {
		printf("fb: could not create device file!\n");
		return -1;
	}

	dev->ioctl = fb_ioctl;	
	return 0;
}

