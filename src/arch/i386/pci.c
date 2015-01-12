#include <aplus.h>
#include <aplus/pci.h>
#include <aplus/mm.h>


#include <stdint.h>
#include <string.h>
#include <errno.h>


#define PCI_CONFIG_ADDRESS		0x0CF8
#define PCI_CONFIG_DATA			0x0CFC
#define PCI_MAX_DEVICES			65536
#define PCI_MAX_BUS				255
#define PCI_MAX_DEV				32
#define PCI_MAX_FUNC			8


pci_device_t* pci_devices = NULL;


static int pci_get_address(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
	return 0x80000000 | (bus << 16) | (dev << 11) | (func << 8) | (offset & 0xFC);
}

static uint32_t pci_config_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
	outl(PCI_CONFIG_ADDRESS, pci_get_address(bus, dev, func, offset));
	
	if(offset % 4 == 0)
		return inl(PCI_CONFIG_DATA);

	return (inl(PCI_CONFIG_DATA) >> ((offset % 4) * 8));
}


static uint32_t pci_get_device_id(pci_device_t* device) {
	return pci_config_read(device->bus, device->dev, device->func, 2) & 0xFFFF;
}

static uint32_t pci_get_vendor_id(pci_device_t* device) {
	return pci_config_read(device->bus, device->dev, device->func, 0) & 0xFFFF;
}

static uint32_t pci_get_intr_pin(pci_device_t* device) {
	return pci_config_read(device->bus, device->dev, device->func, 0x3D) & 0xFF;
}

static uint32_t pci_get_intr_line(pci_device_t* device) {
	return pci_config_read(device->bus, device->dev, device->func, 0x3C) & 0xFF;
}

static uint32_t pci_get_revision(pci_device_t* device) {
	return pci_config_read(device->bus, device->dev, device->func, 0x08) & 0xFF;
}

static uint32_t pci_get_class(pci_device_t* device) {
	return pci_config_read(device->bus, device->dev, device->func, 0x0B) & 0xFF;
}

static uint32_t pci_get_header_type(pci_device_t* device) {
	return pci_config_read(device->bus, device->dev, device->func, 0x0E) & 0x7F;
}


static uint32_t pci_get_bar(pci_device_t* device, uint8_t bar) {
	if(bar >= 6)
		return 0;

	uint8_t header = pci_get_header_type(device);
	if(unlikely(header == 0x02 || (header == 0x01 && bar < 2)))
		return 0;

	uint8_t reg = 0x10 + (0x04 * bar);
	return pci_config_read(device->bus, device->dev, device->func, reg);
}

static uint32_t pci_get_iobase(pci_device_t* device) {
	uint8_t bars = 6 - pci_get_header_type(device) * 4;
	uint8_t i = 0;

	while(i < bars) {
		uint32_t bar = pci_get_bar(device, i++);
		if(bar & 0x01)
			return bar & 0xFFFFFFFC;
	}

	return 0;
}

static uint32_t pci_get_membase(pci_device_t* device) {
	uint8_t bars = 6 - pci_get_header_type(device) * 4;
	uint8_t i = 0;

	while(i < bars) {
		uint32_t bar = pci_get_bar(device, i++);
		if((bar & 0x01) == 0)
			return bar & 0xFFFFFFF0;
	}

	return 0;
}

static void pci_load_device(pci_device_t* device, uint8_t bus, uint8_t dev, uint8_t func) {
	device->bus = bus;
	device->dev = dev;
	device->func = func;

	device->vendorID = pci_get_vendor_id(device);
	device->deviceID = pci_get_device_id(device);
	device->revision = pci_get_revision(device);
	device->pci_class = pci_get_class(device);
	device->iobase = pci_get_iobase(device);
	device->membase = pci_get_membase(device);
	device->header = pci_get_header_type(device);
	device->intr_pin = pci_get_intr_pin(device);
	device->intr_line = pci_get_intr_line(device);
}


pci_device_t* pci_find_by_id(uint32_t vendorID, uint32_t deviceID) {
	for(int i = 0; i < PCI_MAX_DEVICES; i++) {
		if(unlikely(pci_devices[i].vendorID != vendorID && pci_devices[i].deviceID != deviceID))
			continue;

		return &pci_devices[i];
	}

	return NULL;
}

int pci_find_by_class(pci_device_t** rdevs, uint32_t class, uint32_t length) {
	int j = 0;	
	for(int i = 0; i < PCI_MAX_DEVICES && j < length; i++) {
		if(unlikely(pci_devices[i].pci_class != class))
			continue;

		rdevs[j++] = &pci_devices[i];
	}

	return j;
}


int pci_init() {
	pci_devices = (pci_device_t*) kmalloc(PCI_MAX_DEVICES * sizeof(pci_device_t));
	memset(pci_devices, 0xFF, PCI_MAX_DEVICES * sizeof(pci_device_t));

	int i = 0;
	for(int bus = 0; bus < PCI_MAX_BUS; bus++) {
		for(int dev = 0; dev < PCI_MAX_DEV; dev++) {
			for(int func = 0; func < PCI_MAX_FUNC; func++) {
				uint32_t vendor = pci_config_read(bus, dev, func, 0) & 0xFFFF;

				if(unlikely(vendor == 0xFFFF || vendor == 0))
					continue;

				pci_load_device(&pci_devices[i], bus, dev, func);

#ifdef PCI_DEBUG
				kprintf("pci: %d:%d.%d: [%x:%x] (rev %x class %x iobase %x mmio %x type %x int %d pin %d)\n",
							pci_devices[i].bus,
							pci_devices[i].dev,
							pci_devices[i].func,
							pci_devices[i].vendorID,
							pci_devices[i].deviceID,
							pci_devices[i].revision,
							pci_devices[i].pci_class,
							pci_devices[i].iobase,
							pci_devices[i].membase,
							pci_devices[i].header,
							pci_devices[i].intr_line,
							pci_devices[i].intr_pin);
#endif

				i++;
			}
		}
	}


	kprintf("pci: loaded %d devices\n", i);

	return 0;
}
