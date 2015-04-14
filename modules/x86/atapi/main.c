#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/spinlock.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifndef __i386__
int init() {
	return 0;
}

#else
#include <arch/i386/i386.h>
#include "atapi.h"

fastlock_t irq_lock;
spinlock_t crd_lock;

static void irq_handler(void* unused) {
	(void) unused;

	fastlock_unlock(&irq_lock);
}

static int irq_wait(void) {
	fastlock_lock(&irq_lock);
	fastlock_unlock(&irq_lock);
}



static int atapi_check_size(uint32_t bus, uint32_t drive) {
	uint8_t c[12];
	memset(c, 0, 12);

	c[0] = ATAPI_CMD_READCAPS;
	
	outb(ATA_DRIVE_SELECT(bus), drive & (1 << 4));
	ATA_SELECT_DELAY(bus);

	outb(ATA_FEATURES(bus), 0);
	outb(ATA_ADDR2(bus), 8);
	outb(ATA_ADDR3(bus), 0);
	outb(ATA_COMMAND(bus), 0xA0);

	int e = 0;
	while((e = inb(ATA_COMMAND(bus)) & 0x80))
		cpu_wait();

	while(!((e = inb(ATA_COMMAND(bus))) & 0x08) && !(e & 0x01))
		cpu_wait();

	if(unlikely(e & 0x01))
		return 0;



	fastlock_lock(&irq_lock);

	outsw(ATA_DATA(bus), ((uint16_t*) c), 6);
	irq_wait();


	int s = (((int) inb(ATA_ADDR3(bus))) << 8) | (int) (inb(ATA_ADDR2(bus)));
	if(unlikely(!s))
		return 0;

	int ss;
	int lb;
	uint64_t buffer;

	insw(ATA_DATA(bus), ((int16_t*) &buffer), s / 2);

	ss = (int) __builtin_bswap64(buffer);
	lb = (int) (__builtin_bswap64(buffer) >> 32);
	
	
	while((e = inb(ATA_COMMAND(bus))) & 0x08)
		cpu_wait();

	
	return (lb + 1) * ss;
}


static int atapi_read_sector(uint32_t bus, uint32_t drive, uint32_t lba, uint8_t* buffer) {
	uint8_t c[12];
	memset(c, 0, 12);

	c[0] = ATAPI_CMD_READ;
	
	outb(ATA_DRIVE_SELECT(bus), drive & (1 << 4));
	ATA_SELECT_DELAY(bus);

	outb(ATA_FEATURES(bus), 0);
	outb(ATA_ADDR2(bus), ATAPI_SECTOR_SIZE & 0xFF);
	outb(ATA_ADDR3(bus), (ATAPI_SECTOR_SIZE >> 8) & 0xFF);
	outb(ATA_COMMAND(bus), 0xA0);

	int e = 0;
	while((e = inb(ATA_COMMAND(bus)) & 0x80))
		cpu_wait();

	while(!((e = inb(ATA_COMMAND(bus))) & 0x08) && !(e & 0x01))
		cpu_wait();

	if(unlikely(e & 0x01))
		return 0;

	c[9] = 1;
	c[2] = (lba >> 0x18) & 0xFF;
	c[3] = (lba >> 0x10) & 0xFF;
	c[4] = (lba >> 0x08) & 0xFF;
	c[5] = (lba >> 0x00) & 0xFF;

	fastlock_lock(&irq_lock);

	outsw(ATA_DATA(bus), ((uint16_t*) c), 6);
	irq_wait();

	int s = (((int) inb(ATA_ADDR3(bus))) << 8) | (int) (inb(ATA_ADDR2(bus)));
	if(unlikely(!s))
		return 0;

	insw(ATA_DATA(bus), ((int16_t*) buffer), s / 2);
	
	while((e = inb(ATA_COMMAND(bus))) & 0x08)
		cpu_wait();

	return s;
}



int atapi_read(inode_t* ino, char* buf, int length) {
	if(unlikely(!buf))
		return 0;

	if(unlikely(!ino))
		return 0;

	if(unlikely(ino->position + length > ino->size))
		length = ino->size - ino->position;

	if(unlikely(length < 0))
		return 0;

	
	int bus = (int) ino->userdata >> 16;
	int drv = (int) ino->userdata & 0xFF;
	int l = 0;
	int i = 0;

	
	spinlock_lock(&crd_lock);

	for(i = 0; i < length; i += ATAPI_SECTOR_SIZE) {
		if(unlikely(atapi_read_sector(bus, drv, ino->position / ATAPI_SECTOR_SIZE, (uint8_t*) ((uint32_t) buf + i)) != ATAPI_SECTOR_SIZE))
			//break;


		l += ATAPI_SECTOR_SIZE;
		ino->position += ATAPI_SECTOR_SIZE;
	}

	
	
		
	spinlock_unlock(&crd_lock);

	return l;
}


int init() {
	fastlock_init(&irq_lock, SPINLOCK_FLAGS_UNLOCKED);
	spinlock_init(&crd_lock, SPINLOCK_FLAGS_UNLOCKED);


	irq_set(ATA_IRQ_PRIMARY, irq_handler);
	irq_set(ATA_IRQ_SECONDARY, irq_handler);


	#define _C(n, b, d) {											\
		inode_t* dev = (inode_t*)devfs_makedevice("cd" #n, S_IFBLK);\
		if(unlikely(!dev)) {										\
			kprintf("atapi: could not create device cd%d\n", n);	\
			return -1;												\
		}															\
		dev->read = atapi_read;										\
		dev->userdata = (void*) ((b << 16) | (d & 0xFF));			\
		dev->size = atapi_check_size(b, d);							\
	}

	_C(0, ATA_BUS_PRIMARY, ATA_DRIVE_MASTER);
	_C(1, ATA_BUS_PRIMARY, ATA_DRIVE_SLAVE);
	_C(2, ATA_BUS_SECONDARY, ATA_DRIVE_MASTER);
	_C(3, ATA_BUS_SECONDARY, ATA_DRIVE_SLAVE);


	return 0;
}

#endif

int dnit() {
	return 0;
}


/* FIXME: ATAPI polling and reading */
