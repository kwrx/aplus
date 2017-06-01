#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/ipc.h>
#include <aplus/intr.h>
#include <aplus/timer.h>
#include <aplus/mm.h>
#include <libc.h>

MODULE_NAME("pc/block/ata");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#if defined(__i386__) || defined(__x86_64__)
#	if defined(__i386__)
#		include <arch/i386/i386.h>
#		include <arch/i386/pci.h>
#	elif defined(__x86_64__)
#		include <arch/x86_64/x86_64.h>
#		include <arch/x86_64/pci.h>
#	endif

#include "ata.h"

static uintptr_t ata_pci = 0;

struct ata_device {
	int io_base;
	int control;
	int slave;
	ata_identify_t identify;
	
	struct {
		uintptr_t offset;
		uint16_t bytes;
		uint16_t last;
	} __packed *dma_prdt;

	uintptr_t dma_prdt_phys;
	uint8_t* dma_start;
	uintptr_t dma_start_phys;
	uint32_t bar4;

	spinlock_t lock;
	uint8_t* cache;
};


static struct ata_device ata_primary_master = {
	.io_base = 0x1F0,
	.control = 0x3F6,
	.slave = 0
};

static struct ata_device ata_secondary_master = {
	.io_base = 0x1F0,
	.control = 0x3F6,
	.slave = 1
};
static struct ata_device ata_primary_slave = {
	.io_base = 0x170,
	.control = 0x376,
	.slave = 0
};
static struct ata_device ata_secondary_slave = {
	.io_base = 0x170,
	.control = 0x376,
	.slave = 1
};


static void find_ata_pci(uint32_t device, uint16_t vendorid, uint16_t deviceid, void* arg) {
	if((vendorid == 0x8086) && (deviceid & 0x7010))
		*((uintptr_t*) arg) = device;
}


static uint64_t ata_max_offset(struct ata_device* dev) {
	if(dev->identify.sectors_48)
		return dev->identify.sectors_48 * ATA_SECTOR_SIZE;

	return dev->identify.sectors_28 * ATA_SECTOR_SIZE;
}

static void ata_io_wait(struct ata_device* dev) {
	inb(dev->io_base + ATA_REG_ALTSTATUS);
	inb(dev->io_base + ATA_REG_ALTSTATUS);
	inb(dev->io_base + ATA_REG_ALTSTATUS);
	inb(dev->io_base + ATA_REG_ALTSTATUS);
}

static int ata_status_wait(struct ata_device* dev, int timeout) {
	int e, i = 0;
	if(unlikely(timeout > 0))
		while((e = inb(dev->io_base + ATA_REG_STATUS)) & ATA_SR_BSY && (i < timeout))
			i++;
	else
		while((e = inb(dev->io_base + ATA_REG_STATUS)) & ATA_SR_BSY);

	return e;
}

static int ata_wait(struct ata_device* dev, int advanced) {
	ata_io_wait(dev);
	int e = ata_status_wait(dev, -1);

	if(unlikely(advanced)) {
		e = inb(dev->io_base + ATA_REG_STATUS);

		if(e & ATA_SR_ERR)
			return E_ERR;

		if(e & ATA_SR_DF)
			return E_ERR;

		if(!(e & ATA_SR_DRQ))
			return E_ERR;
	}

	return E_OK;
}


static void ata_soft_reset(struct ata_device* dev) {
	outb(dev->control, 0x04);
	ata_io_wait(dev);
	outb(dev->control, 0x00);
}




static void irq_handler_1(void* unused) {
	(void) unused;

	inb(ata_primary_master.io_base + ATA_REG_STATUS);
}

static void irq_handler_2(void* unused) {
	(void) unused;

	inb(ata_secondary_master.io_base + ATA_REG_STATUS);
}


static void ata_device_init(struct ata_device* dev) {
	outb(dev->io_base + 1, 1);
	outb(dev->control, 0);
	outb(dev->io_base + ATA_REG_HDDEVSEL, 0xA0 | dev->slave << 4);
	
	ata_io_wait(dev);

	outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

	ata_io_wait(dev);
	int e = inb(dev->io_base + ATA_REG_COMMAND);
	ata_wait(dev, 0);



	uint16_t* buf = (uint16_t*) &dev->identify;

	int i;
	for(i = 0; i < 256; i++)
		buf[i] = inw(dev->io_base);
	
	
	char* ptr = (char*) &dev->identify.model;
	for(i = 0; i < 39; i += 2) {
		register char t = ptr[i + 1];
		ptr[i + 1] = ptr[i];
		ptr[i] = t;
	}

#if 0
	kprintf(LOG "[ATA] Device Name:  %s\n", dev->identify.model);
	kprintf(LOG "[ATA] Sectors (48): %lld\n", dev->identify.sectors_48);
	kprintf(LOG "[ATA] Sectors (28): %d\n", dev->identify.sectors_28);
#endif

	dev->dma_prdt = (void*) kvalloc(8, GFP_KERNEL);
	dev->dma_start = (void*) kvalloc(4096, GFP_KERNEL);

	dev->dma_prdt_phys = V2P(dev->dma_prdt);
	dev->dma_start_phys = V2P(dev->dma_start);

	dev->dma_prdt[0].offset = dev->dma_start_phys;
	dev->dma_prdt[0].bytes = ATA_SECTOR_SIZE;
	dev->dma_prdt[0].last = 0x8000;

	
	uint16_t cmd = pci_read_field(ata_pci, PCI_COMMAND, 4);
	if(!(cmd & (1 << 2))) 
		pci_write_field(ata_pci, PCI_COMMAND, 4, cmd | (1 << 2));
	
	dev->bar4 = pci_read_field(ata_pci, PCI_BAR4, 4);

	if(dev->bar4 & 1)
		dev->bar4 &= 0xFFFFFFFC;


	dev->cache = (uint8_t*) kmalloc(ATA_CACHE_SIZE, GFP_KERNEL);
	memset(dev->cache, 0, ATA_CACHE_SIZE);
}



static void ata_device_read_sector(struct ata_device* dev, uint32_t lba, void* buf, size_t count) {
	uint16_t bus = dev->io_base;
	uint8_t slave = dev->slave;

	outb(bus + ATA_REG_CONTROL, 0x00);	
	outb(bus + ATA_REG_HDDEVSEL, 0x40 | (slave << 4));
	
	ata_wait(dev, 0);

	outb(bus + ATA_REG_SECCOUNT0, (count >> 8) & 0xFF);
	outb(bus + ATA_REG_LBA0, (lba & 0xFF00000) >> 24);
	outb(bus + ATA_REG_LBA1, (lba & 0xFF0000000) >> 32);
	outb(bus + ATA_REG_LBA2, 0);
	outb(bus + ATA_REG_SECCOUNT0, count & 0xFF);
	outb(bus + ATA_REG_LBA0, lba & 0xFF);
	outb(bus + ATA_REG_LBA1, (lba & 0xFF00) >> 8);
	outb(bus + ATA_REG_LBA2, (lba & 0xFF0000) >> 16);
	outb(bus + ATA_REG_COMMAND, ATA_CMD_READ_PIO_EXT);

	ata_wait(dev, 0);
	
	int i;
	uintptr_t b = (uintptr_t) buf;
	for(i = 0; i < count; i++, b += ATA_SECTOR_SIZE) {
		insw(bus, b, ATA_SECTOR_SIZE >> 1);
		ata_io_wait(dev);
		ata_wait(dev, 0);
	}
	
	outb(bus + 0x07, ATA_CMD_CACHE_FLUSH);	
	ata_wait(dev, 0);
}

#if 0
static void ata_device_read_sector(struct ata_device* dev, uint32_t lba, void* buf, size_t count) {
	uint16_t bus = dev->io_base;
	uint8_t slave = dev->slave;

	ata_wait(dev, 0);
	outb(dev->bar4, 0x00);
	outl(dev->bar4 + 0x04, dev->dma_prdt_phys);
	outb(dev->bar4 + 0x02, inb(dev->bar4 + 0x02) | 0x04 | 0x02);
	outb(dev->bar4, 0x08);
	ata_wait(dev, 0);

	outb(bus + ATA_REG_SECCOUNT0, (count >> 8) & 0xFF);
	outb(bus + ATA_REG_LBA0, (lba & 0xFF00000) >> 24);
	outb(bus + ATA_REG_LBA1, (lba & 0xFF0000000) >> 32);
	outb(bus + ATA_REG_LBA2, 0);
	outb(bus + ATA_REG_SECCOUNT0, count & 0xFF);
	outb(bus + ATA_REG_LBA0, lba & 0xFF);
	outb(bus + ATA_REG_LBA1, (lba & 0xFF00) >> 8);
	outb(bus + ATA_REG_LBA2, (lba & 0xFF0000) >> 16);
	ata_wait(dev, 0);

	outb(bus + ATA_REG_COMMAND, ATA_CMD_READ_DMA_EXT);
	ata_io_wait(dev);

	outb(dev->bar4, 0x08 | 0x01);

	while(1) {
		int s1 = inb(dev->bar4 + 0x02);
		int s2 = inb(dev->io_base + ATA_REG_STATUS);

		if(!(s1 & 0x04))
			continue;
		if(!(s2 & ATA_SR_BSY))
			break;
	}

	memcpy(buf, dev->dma_start, count * ATA_SECTOR_SIZE);
	outb(dev->bar4 + 0x02, inb(dev->bar4 + 0x02) | 0x04 | 0x02);
}
#endif

static void ata_device_write_sector(struct ata_device* dev, uint32_t lba, void* buf, size_t count) {
	uint16_t bus = dev->io_base;
	uint8_t slave = dev->slave;

	outb(bus + ATA_REG_CONTROL, 0x00);	
	outb(bus + ATA_REG_HDDEVSEL, 0x40 | (slave << 4));
	
	ata_wait(dev, 0);

	outb(bus + ATA_REG_SECCOUNT0, (count >> 8) & 0xFF);
	outb(bus + ATA_REG_LBA0, (lba & 0xFF00000) >> 24);
	outb(bus + ATA_REG_LBA1, (lba & 0xFF0000000) >> 32);
	outb(bus + ATA_REG_LBA2, 0);
	outb(bus + ATA_REG_SECCOUNT0, count & 0xFF);
	outb(bus + ATA_REG_LBA0, lba & 0xFF);
	outb(bus + ATA_REG_LBA1, (lba & 0xFF00) >> 8);
	outb(bus + ATA_REG_LBA2, (lba & 0xFF0000) >> 16);
	outb(bus + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO_EXT);

	ata_wait(dev, 0);
	
	int i;
	uintptr_t b = (uintptr_t) buf;
	for(i = 0; i < count; i++, b += ATA_SECTOR_SIZE) {
		outsw(bus, (void*) b, ATA_SECTOR_SIZE >> 1);
		ata_io_wait(dev);
		ata_wait(dev, 0);
	}
	
	outb(bus + 0x07, ATA_CMD_CACHE_FLUSH);	
	ata_wait(dev, 0);
}


static int ata_read(inode_t* ino, void* buffer, size_t size) {
	if(unlikely(ino->position > ino->size))
		return 0;

	if(unlikely((ino->position + size) > ino->size))
		size = ino->size - ino->position;

	if(unlikely(!size))
		return 0;


	struct ata_device* dev = (struct ata_device*) ino->userdata;

	long sb = ino->position / ATA_SECTOR_SIZE;
	long eb = (ino->position + size - 1) / ATA_SECTOR_SIZE;
	off64_t xoff = 0;

	if(ino->position % ATA_SECTOR_SIZE) {
		long p;
		p = ATA_SECTOR_SIZE - (ino->position % ATA_SECTOR_SIZE);
		p = p > size ? size : p;

		spinlock_lock(&dev->lock);
		ata_device_read_sector(dev, sb, dev->cache, 1);

		memcpy(buffer, (void*) ((uintptr_t) dev->cache + ((uintptr_t) ino->position % ATA_SECTOR_SIZE)), p);
		xoff += p;
		sb++;

		spinlock_unlock(&dev->lock);
	}

	if(((ino->position + size) % ATA_SECTOR_SIZE) && (sb <= eb)) {
		long p = (ino->position + size) % ATA_SECTOR_SIZE;

		spinlock_lock(&dev->lock);
		ata_device_read_sector(dev, eb, dev->cache, 1);

		memcpy((void*) ((uintptr_t) buffer + size - p), dev->cache, p);
		eb--;

		spinlock_unlock(&dev->lock);
	}


	long i = eb - sb + 1;
	if(likely(i > 0))
		ata_device_read_sector(dev, sb, (void*) ((uintptr_t) buffer + (uintptr_t) xoff), i);


	ino->position += size;
	return size;
}

static int ata_write(inode_t* ino, void* buffer, size_t size) {
	if(unlikely(ino->position > ino->size))
		return 0;

	if(unlikely((ino->position + size) > ino->size))
		size = ino->size - ino->position;

	if(unlikely(!size))
		return 0;


	struct ata_device* dev = (struct ata_device*) ino->userdata;

	long sb = ino->position / ATA_SECTOR_SIZE;
	long eb = (ino->position + size - 1) / ATA_SECTOR_SIZE;
	off64_t xoff = 0;


	if(ino->position % ATA_SECTOR_SIZE) {
		long p;
		p = ATA_SECTOR_SIZE - (ino->position % ATA_SECTOR_SIZE);
		p = p > size ? size : p;


		spinlock_lock(&dev->lock);

		ata_device_read_sector(dev, sb, dev->cache, 1);
		memcpy((void*) ((uintptr_t) dev->cache + ((uintptr_t) ino->position % ATA_SECTOR_SIZE)), buffer, p);
		ata_device_write_sector(dev, sb, dev->cache, 1);

		xoff += p;
		sb++;

		spinlock_unlock(&dev->lock);
	}

	if(((ino->position + size) % ATA_SECTOR_SIZE) && (sb <= eb)) {
		long p = (ino->position + size) % ATA_SECTOR_SIZE;


		spinlock_lock(&dev->lock);

		ata_device_read_sector(dev, eb, dev->cache, 1);
		memcpy(dev->cache, (void*) ((uintptr_t) buffer + size - p), p);
		ata_device_write_sector(dev, eb, dev->cache, 1);

		eb--;
		spinlock_unlock(&dev->lock);
	}


	long i = eb - sb + 1;
	if(likely(i > 0))
		ata_device_write_sector(dev, sb, (void*) ((uintptr_t) buffer + (uintptr_t) xoff), i);

	ino->position += size;
	return size;
}



static void atapi_device_read_sector(struct ata_device* dev, uint32_t lba, void* buf) {
	uint8_t pk[12] = { 0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	
	uint16_t bus = dev->io_base;
	uint8_t slave = dev->slave;


	outb(bus + ATA_REG_CONTROL, 0x02);
	
	ata_wait(dev, 0);
	outb(bus + ATA_REG_HDDEVSEL, 0xE0 | (slave << 4) | (lba & 0x0F000000) >> 24);
	ata_wait(dev, 0);

	outb(bus + ATA_REG_FEATURES, 0x00);
	outb(bus + ATA_REG_LBA1, ATAPI_SECTOR_SIZE & 0xFF);
	outb(bus + ATA_REG_LBA2, ATAPI_SECTOR_SIZE >> 8);
	outb(bus + ATA_REG_COMMAND, ATA_CMD_PACKET);

	ata_wait(dev, 0);
	
	pk[9] = 1;
	pk[2] = (lba >> 0x18) & 0xFF;
	pk[3] = (lba >> 0x10) & 0xFF;
	pk[4] = (lba >> 0x08) & 0xFF;
	pk[5] = (lba >> 0x00) & 0xFF;

	outsw(bus + ATA_REG_DATA, (uint16_t*) pk, 6);
	ata_wait(dev, 0);
	insw(bus + ATA_REG_DATA, buf, ATAPI_SECTOR_SIZE / 2);
	
	outb(bus + 0x07, ATA_CMD_CACHE_FLUSH);	
	ata_wait(dev, 0);
}


static int atapi_read(inode_t* ino, void* buffer, size_t size) {
	if(ino->position > ino->size)
		return 0;

	if(ino->position + size > ino->size)
		size = ino->size - ino->position;

	if(!size)
		return 0;

	struct ata_device* dev = (struct ata_device*) ino->userdata;

	long sb = ino->position / ATAPI_SECTOR_SIZE;
	long eb = (ino->position + size - 1) / ATAPI_SECTOR_SIZE;
	off64_t xoff = 0;

	if(ino->position % ATAPI_SECTOR_SIZE) {
		long p;
		p = ATAPI_SECTOR_SIZE - (ino->position % ATAPI_SECTOR_SIZE);
		p = p > size ? size : p;	

		spinlock_lock(&dev->lock);
		atapi_device_read_sector(dev, sb, dev->cache);

		memcpy(buffer, (void*) ((uintptr_t) dev->cache + ((uintptr_t) ino->position % ATAPI_SECTOR_SIZE)), p);
		xoff += p;
		sb++;

		spinlock_unlock(&dev->lock);
	}


	if(((ino->position + size) % ATAPI_SECTOR_SIZE) && (sb <= eb)) {
		long p = (ino->position + size) % ATAPI_SECTOR_SIZE;

		spinlock_lock(&dev->lock);
		atapi_device_read_sector(dev, eb, dev->cache);

		memcpy((void*) ((uintptr_t) buffer + size - p), dev->cache, p);
		eb--;

		spinlock_unlock(&dev->lock);
	}

	while(sb <= eb) {
		atapi_device_read_sector(dev, sb, (void*) ((uintptr_t) buffer + (uintptr_t) xoff));
		xoff += ATAPI_SECTOR_SIZE;
		sb++;
	}

	ino->position += size;
	return size;
}



static int ata_device_detect(struct ata_device* dev) {
	ata_soft_reset(dev);
	ata_io_wait(dev);

	outb(dev->io_base + ATA_REG_HDDEVSEL, 0xA0 | dev->slave << 4);

	ata_io_wait(dev);
	ata_status_wait(dev, 10000);

	uint8_t cl = inb(dev->io_base + ATA_REG_LBA1);
	uint8_t ch = inb(dev->io_base + ATA_REG_LBA2);
	uint8_t c = 0, d = 0;

	if(cl == 0xFF && ch == 0xFF)
		return 0;

	if(
		(cl == 0x00 && ch == 0x00) ||
		(cl == 0x3C && ch == 0xC3)
	) {

		spinlock_init(&dev->lock);
		ata_device_init(dev);

		inode_t* inode = vfs_mkdev("hd", c++, S_IFBLK | 0660);
		inode->size = ata_max_offset(dev);
		inode->read = ata_read;
		inode->write = ata_write;
		inode->userdata = (void*) dev;

		mbr_init(inode);
	} else if(
		(cl == 0x14 && ch == 0xEB)	/* ATAPI  */
		//(cl == 0x69 && ch == 0x96) 	/* SATAPI */
	) {
		spinlock_init(&dev->lock);
		ata_device_init(dev);

		inode_t* inode = vfs_mkdev("cd", d++, S_IFBLK | 0440);
		inode->size = 700 * 1024 * 1024; //ata_max_offset(dev);
		inode->read = atapi_read;
		inode->write = NULL;
		inode->userdata = (void*) dev;
	}
}

int init(void) {
	
	pci_scan(&find_ata_pci, -1, &ata_pci);

	irq_enable(ATA_IRQ_PRIMARY, irq_handler_1);
	irq_enable(ATA_IRQ_SECONDARY, irq_handler_2);

	ata_device_detect(&ata_primary_master);
	ata_device_detect(&ata_secondary_master);
	ata_device_detect(&ata_primary_slave);
	ata_device_detect(&ata_secondary_slave);


	return E_OK;
}

#else

int init(void) {
	return E_OK;
}

#endif


int dnit(void) {
	return E_OK;
}
