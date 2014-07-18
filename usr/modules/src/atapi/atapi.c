//
//  zero.c
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


#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>


#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>

#include "atapi.h"


static int irq_fired = 0;
static void irq_handler(void* unused) {
	irq_fired = 1;
}


static int wait_irq(uint32_t bus) {
	while(irq_fired == 0);
	irq_fired = 0;
}

static int atapi_read_sector(uint32_t bus, uint32_t drive, uint32_t lba, uint8_t* buffer) {
	uint8_t read_cmd[12] = { 0 };
	memset(read_cmd, 0, 12);
	
	read_cmd[0] = 0xA8;
	
	outb(ATA_DRIVE_SELECT(bus), drive & (1 << 4));
	ATA_SELECT_DELAY(bus);
	
	outb(ATA_FEATURES(bus), 0);
	outb(ATA_ADDR2(bus), ATAPI_SECTOR_SIZE & 0xFF);
	outb(ATA_ADDR3(bus), (ATAPI_SECTOR_SIZE >> 8) & 0xFF);
	outb(ATA_COMMAND(bus), 0xA0);
	
	

	int status = 0;
	while((status = inb(ATA_COMMAND(bus))) & 0x80)
		__asm__ __volatile__ ("pause");
		

	while(!((status = inb(ATA_COMMAND(bus))) & 0x8) && !(status & 0x01))
		__asm__ __volatile__ ("pause");
		
	if(status & 0x01)
		return 0;
		
	read_cmd[9] = 1;
	read_cmd[2] = (lba >> 0x18) & 0xFF;
	read_cmd[3] = (lba >> 0x10) & 0xFF;
	read_cmd[4] = (lba >> 0x08) & 0xFF;
	read_cmd[5] = (lba >> 0x00) & 0xFF;
	
	outsw(ATA_DATA(bus), ((uint16_t*) read_cmd), 6);

	wait_irq(bus);
	
	int size = (((int) inb(ATA_ADDR3(bus))) << 8) | (int) (inb(ATA_ADDR2(bus)));
	if(size == 0)
		return 0;
	
	insw(ATA_DATA(bus), ((uint16_t*) buffer), size / 2);
	
	
	while((status = inb(ATA_COMMAND(bus))) & 0x88)
		__asm__ __volatile__ ("pause");
	
	return size;
}

int atapi_read(struct inode* ino, uint32_t length, void* buf) {
	if(!buf)
		return 0;
		
	if(!ino)
		return 0;
		
	
		
	int bus = ino->disk_ptr >> 16;
	int drv = ino->disk_ptr & 0xFF;
	
	for(int i = 0; i < length; i += ATAPI_SECTOR_SIZE) {
		atapi_read_sector(bus, drv, ino->position / ATAPI_SECTOR_SIZE, (uint8_t*) ((uint32_t) buf + i));
		ino->position += ATAPI_SECTOR_SIZE;
	}
	
	
	
	return length;
}


int cmain(int argc, char** argv) {

	irq_set(ATA_IRQ_PRIMARY, irq_handler);
	irq_set(ATA_IRQ_SECONDARY, irq_handler);

	#define create_cd(n, b, d)											\
		inode_t* dev##n = aplus_device_create("/dev/cd" #n, S_IFBLK);	\
		if(!dev##n)														\
			printf("cd%d: could not create device\n", n);				\
		dev##n->length = 0;												\
		dev##n->read = atapi_read;										\
		dev##n->disk_ptr = (b << 16) | (d & 0xFF);						
		
		
	create_cd(0, ATA_BUS_PRIMARY, ATA_DRIVE_MASTER);
	create_cd(1, ATA_BUS_PRIMARY, ATA_DRIVE_SLAVE);
	create_cd(2, ATA_BUS_SECONDARY, ATA_DRIVE_MASTER);
	create_cd(3, ATA_BUS_SECONDARY, ATA_DRIVE_SLAVE);

	return 0;
}


