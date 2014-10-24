#ifndef _PCI_H
#define _PCI_H

#include <aplus.h>
#include <stdint.h>


#define PCI_CLASS_OLD			0x00
#define PCI_CLASS_STORAGE		0x01
#define PCI_CLASS_NETWORK		0x02
#define PCI_CLASS_DISPLAY		0x03
#define PCI_CLASS_MULTIMEDIA	0x04
#define PCI_CLASS_MEMORY		0x05
#define PCI_CLASS_BRIDGE		0x06
#define PCI_CLASS_SIMCOM		0x07
#define PCI_CLASS_SYSPERIPHERAL	0x08
#define PCI_CLASS_INPUT			0x09
#define PCI_CLASS_DOCK			0x0A
#define PCI_CLASS_PROCESSOR		0x0B
#define PCI_CLASS_SERIAL		0x0C
#define PCI_CLASS_MISC			0xFF

typedef struct pci_device {
	uint16_t vendorID;
	uint16_t deviceID;

	uint8_t bus;
	uint8_t dev;
	uint8_t func;
	uint8_t revision;

	uint32_t pci_class;
	uint32_t iobase;
	uint32_t membase;
	
	uint8_t header;
	uint8_t intr_pin;
	uint8_t	intr_line;
} pci_device_t;

#endif
