#include <aplus.h>
#include <aplus/debug.h>
#include <libc.h>

#include <arch/i386/i386.h>
#include <arch/i386/pci.h>

#if DEBUG
#include <arch/i386/pci_list.h>
#endif

static void pci_scan_hit(pci_func_t f, uint32_t dev, void* extra);
static void pci_scan_func(pci_func_t f, int type, int bus, int slot, int func, void* extra);
static void pci_scan_slot(pci_func_t f, int type, int bus, int slot, void * extra);
static void pci_scan_bus(pci_func_t f, int type, int bus, void* extra);


void pci_write_field(uint32_t device, int field, int size, uint32_t value) {
    outl(PCI_ADDRESS_PORT, pci_get_addr(device, field));
    outl(PCI_VALUE_PORT, value);
}


uint32_t pci_read_field(uint32_t device, int field, int size) {
    outl(PCI_ADDRESS_PORT, pci_get_addr(device, field));

    switch(size) {
        case 4:
            return inl(PCI_VALUE_PORT);
        case 2:
            return inw(PCI_VALUE_PORT + (field & 2));
        case 1:
            return inb(PCI_VALUE_PORT + (field & 3));
    }

    return 0xFFFF;
}

uint16_t pci_find_type(uint32_t dev) {
    return (pci_read_field(dev, PCI_CLASS, 1) << 8) | pci_read_field(dev, PCI_SUBCLASS, 1);
}


static void pci_scan_hit(pci_func_t f, uint32_t dev, void* arg) {
    int vend = pci_read_field(dev, PCI_VENDOR_ID, 2);
    int dvid = pci_read_field(dev, PCI_DEVICE_ID, 2);

    f(dev, vend, dvid, arg);
}

static void pci_scan_func(pci_func_t f, int type, int bus, int slot, int func, void* arg) {
    uint32_t dev = pci_box_device(bus, slot, func);
    if(type == -1 || type == pci_find_type(dev))
        pci_scan_hit(f, dev, arg);

    if(pci_find_type(dev) == PCI_TYPE_BRIDGE)
        pci_scan_bus(f, type, pci_read_field(dev, PCI_SECONDARY_BUS, 1), arg);
}

static void pci_scan_slot(pci_func_t f, int type, int bus, int slot, void* arg) {
    uint32_t dev = pci_box_device(bus, slot, 0);
    if(pci_read_field(dev, PCI_VENDOR_ID, 2) == PCI_NONE)
        return;

    pci_scan_func(f, type, bus, slot, 0, arg);

    if(!pci_read_field(dev, PCI_HEADER_TYPE, 1))
        return;

    int i;
    for(i = 1; i < 8; i++) {
        uint32_t dev = pci_box_device(bus, slot, i);
        if(pci_read_field(dev, PCI_VENDOR_ID, 2) != PCI_NONE)
            pci_scan_func(f, type, bus, slot, i, arg);
    }
}

static void pci_scan_bus(pci_func_t f, int type, int bus, void* arg) {
    int i;
    for(i = 0; i < 32; i++)
        pci_scan_slot(f, type, bus, i, arg);
}


void pci_scan(pci_func_t f, int type, void* arg) {
    pci_scan_bus(f, type, 0, arg);

    if(!pci_read_field(0, PCI_HEADER_TYPE, 1))
        return;

    int i;
    for(i = 1; i < 8; i++) {
        uint32_t dev = pci_box_device(0, 0, i);
        if(pci_read_field(dev, PCI_VENDOR_ID, 2) != PCI_NONE)
            pci_scan_bus(f, type, i, arg);
        else
            break;
    }
}


int pci_init(void) {

#if DEBUG
    (void) PciDevSelFlags;
    (void) PciStatusFlags;
    (void) PciCommandFlags;
    (void) PciClassCodeTable;
    (void) PciVenTable;

    void pci_func(uint32_t device, uint16_t vendor_id, uint16_t device_id, void* arg) {
        int i;
        for(i = 0; i < PCI_DEVTABLE_LEN; i++)
            if(
                (PciDevTable[i].VenId == vendor_id)    &&
                (PciDevTable[i].DevId == device_id)
            ) kprintf(LOG "pci: %x:%x - %s - %s\n", vendor_id, device_id, PciDevTable[i].Chip, PciDevTable[i].ChipDesc);

    }

    (void) pci_func;
#endif 

    return 0;
}

EXPORT(pci_scan);
EXPORT(pci_find_type);
EXPORT(pci_write_field);
EXPORT(pci_read_field);