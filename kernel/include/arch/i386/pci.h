#ifndef _PCI_H
#define _PCI_H

#include <arch/i386/i386.h>

#define PCI_VENDOR_ID        0x00
#define PCI_DEVICE_ID        0x02
#define PCI_COMMAND        0x04
#define PCI_STATUS        0x06
#define PCI_REVISION_ID        0x08


#define PCI_PROG_IF        0x09
#define PCI_SUBCLASS        0x0A
#define PCI_CLASS        0x0B
#define PCI_CACHE_LINE_SIZE    0x0C
#define PCI_LATENCY_TIMER    0x0D
#define PCI_HEADER_TYPE        0x0E
#define PCI_BIST        0x0F

#define PCI_BAR0        0x10
#define PCI_BAR1        0x14
#define PCI_BAR2        0x18
#define PCI_BAR3        0x1C
#define PCI_BAR4        0x20
#define PCI_BAR5        0x24

#define PCI_SUBSYSID    0x2C
#define PCI_SUBVENID    0x2E

#define PCI_INTERRUPT_LINE    0x3C
#define PCI_SECONDARY_BUS    0x09


#define PCI_HEADER_TYPE_DEVICE    0
#define PCI_HEADER_TYPE_BRIDGE    1
#define PCI_HEADER_TYPE_CARDBUS    2

#define PCI_TYPE_BRIDGE        0x0604
#define PCI_TYPE_SATA        0x0106

#define PCI_ADDRESS_PORT    0xCF8
#define PCI_VALUE_PORT        0xCFC

#define PCI_NONE        0xFFFF


#ifndef __ASSEMBLY__

#define pci_extract_bus(x)    \
    ((uint8_t) (x >> 16))
#define pci_extract_slot(x)    \
    ((uint8_t) (x >> 8))
#define pci_extract_func(x)    \
    ((uint8_t) (x))


#define pci_get_addr(x, y)            \
    (0x80000000             |    \
    (pci_extract_bus(x) << 16)    |    \
    (pci_extract_slot(x) << 11)    |    \
    (pci_extract_func(x) << 8)    |    \
    ((field) & 0xFC))


#define pci_box_device(x, y, z)            \
    ((uint32_t) ((x << 16) | (y << 8) | z))


typedef void (*pci_func_t)(uint32_t device, uint16_t vendor_id, uint16_t device_id, void * extra);

uint32_t pci_read_field(uint32_t device, int field, int size);
void pci_write_field(uint32_t device, int field, int size, uint32_t value);
uint16_t pci_find_type(uint32_t dev);

void pci_scan(pci_func_t f, int type, void* extra);

#endif
#endif
