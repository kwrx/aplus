#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/spinlock.h>
#include <aplus/pci.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifndef __i386__
int init() {
	return 0;
}

#else
#include <arch/i386/i386.h>
#include "sb16.h"


static pci_device_t* sb16_device = NULL;

static void irq_handler(void* unused) {
	(void) unused;

	kprintf("sb16: irq fired!\n");	
}

int init() {
	if(!(sb16_device = (pci_device_t*) pci_find_by_id(1274, 5000)))	
		return 0;
	

	irq_set(sb16_device->intr_line, irq_handler);

	outb(sb16_device->iobase + SB16_DSP_RESET, 1);
	for(int i = 0; i < 100000; i++)
		cpu_wait();

	outb(sb16_device->iobase + SB16_DSP_RESET, 0);
	for(int i = 0; i < 1000; i++)
		if(inb(sb16_device->iobase + SB16_DSP_READ) == SB16_DSP_READY)
			break;

	while((inb(sb16_device->iobase + SB16_DSP_WRITE) & 0x80) != 0)
		cpu_wait();

	outb(sb16_device->iobase + SB16_DSP_WRITE, SB16_DSP_VERSION);

	while((inb(sb16_device->iobase + SB16_DSP_STATUS) & 0x80) == 0)
		cpu_wait();


	/*
	kprintf("vermax: %d\n", inb(sb16_device->iobase + SB16_DSP_READ));
	kprintf("vermin: %d\n", inb(sb16_device->iobase + SB16_DSP_READ));


	for(int i = 0; i < 1000000; i++) {
		outb(sb16_device->iobase + SB16_DSP_WRITE, 0x10);
		outb(sb16_device->iobase + SB16_DSP_WRITE, i % 255);
	}
	*/

	/* TODO: to finish... */

	return 0;
}

#endif


int dnit() {
	return 0;
}
