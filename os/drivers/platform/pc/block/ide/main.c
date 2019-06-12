/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/intr.h>
#include <aplus/mm.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <dev/interface.h>
#include <dev/block.h>
#include <dev/pci.h>

#include <arch/x86/cpu.h>


MODULE_NAME("block/ide");
MODULE_DEPS("dev/interface,dev/block,dev/pci");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



#define IDE_ATA_SECTOR_SIZE             512
#define IDE_ATAPI_SECTOR_SIZE           2048

#define IDE_IRQ_PRIMARY                 0x0E
#define IDE_IRQ_SECONDARY               0x0F


#define IDE_SR_BSY                      0x80
#define IDE_SR_DRDY                     0x40
#define IDE_SR_DF                       0x20
#define IDE_SR_DSC                      0x10
#define IDE_SR_DRQ                      0x08
#define IDE_SR_CORR                     0x04
#define IDE_SR_IDX                      0x02
#define IDE_SR_ERR                      0x01


#define IDE_ER_BBK                      0x80
#define IDE_ER_UNC                      0x40
#define IDE_ER_MC                       0x20
#define IDE_ER_IDNF                     0x10
#define IDE_ER_MCR                      0x08
#define IDE_ER_ABRT                     0x04
#define IDE_ER_TK0NF                    0x02
#define IDE_ER_AMNF                     0x01

#define IDE_CMD_READ_PIO                0x20
#define IDE_CMD_READ_PIO_EXT            0x24
#define IDE_CMD_READ_DMA                0xC8
#define IDE_CMD_READ_DMA_EXT            0x25
#define IDE_CMD_WRITE_PIO               0x30
#define IDE_CMD_WRITE_PIO_EXT           0x34
#define IDE_CMD_WRITE_DMA               0xCA
#define IDE_CMD_WRITE_DMA_EXT           0x35
#define IDE_CMD_CACHE_FLUSH             0xE7
#define IDE_CMD_CACHE_FLUSH_EXT         0xEA
#define IDE_CMD_PACKET                  0xA0
#define IDE_CMD_IDENTIFY_PACKET         0xA1
#define IDE_CMD_IDENTIFY                0xEC

#define IDE_ATAPI_CMD_READ              0xA8
#define IDE_ATAPI_CMD_EJECT             0x1B

#define IDE_IDENT_DEVICETYPE            0
#define IDE_IDENT_CYLINDERS             2
#define IDE_IDENT_HEADS                 6
#define IDE_IDENT_SECTORS               12
#define IDE_IDENT_SERIAL                20
#define IDE_IDENT_MODEL                 54
#define IDE_IDENT_CAPABILITIES          98
#define IDE_IDENT_FIELDVALID            106
#define IDE_IDENT_MAX_LBA               120
#define IDE_IDENT_COMMANDSETS           164
#define IDE_IDENT_MAX_LBA_EXT           200

#define IDE_ATA                         0x00
#define IDE_ATAPI                       0x01
 
#define IDE_MASTER                      0x00
#define IDE_SLAVE                       0x01

#define IDE_REG_DATA                    0x00
#define IDE_REG_ERROR                   0x01
#define IDE_REG_FEATURES                0x01
#define IDE_REG_SECCOUNT0               0x02
#define IDE_REG_LBA0                    0x03
#define IDE_REG_LBA1                    0x04
#define IDE_REG_LBA2                    0x05
#define IDE_REG_HDDEVSEL                0x06
#define IDE_REG_COMMAND                 0x07
#define IDE_REG_STATUS                  0x07
#define IDE_REG_SECCOUNT1               0x08
#define IDE_REG_LBA3                    0x09
#define IDE_REG_LBA4                    0x0A
#define IDE_REG_LBA5                    0x0B
#define IDE_REG_CONTROL                 0x0C
#define IDE_REG_ALTSTATUS               0x0C
#define IDE_REG_DEVADDRESS              0x0D

#define IDE_READ                        0x00
#define IDE_WRITE                       0x01


#define IDE_TYPE_MASTER                 0
#define IDE_TYPE_SLAVE                  1



struct disk {
    uint16_t io;
    uint16_t cp;
    uint8_t intno;
    uint8_t type;
};

struct ide_identify {
    uint16_t flags;
    uint16_t reserved0[9];
    char serial[20];
    uint16_t reserved1[3];
    char firmware[8];
    char model[40];

    uint16_t sectors_per_int;
    uint16_t reserved2;
    uint16_t caps[2];
    uint16_t reserved3;
    uint16_t reserved4;
    uint16_t valid_ext_data;
    uint16_t reserved5[5];
    uint16_t size_of_rw_mult;
    uint32_t sectors_28;
    uint16_t reserved6[38];
    uint64_t sectors_48;
    uint16_t reserved7[152];
};

struct {
    uint32_t vendorid;
    uint32_t deviceid;


    struct disk primary_master;    
    struct disk primary_slave;    
    struct disk secondary_master;    
    struct disk secondary_slave;    

    uintptr_t dma;
    spinlock_t lock;
} ide = { };



#define __iowait(d) {                       \
    inb(d->io + IDE_REG_ALTSTATUS);         \
    inb(d->io + IDE_REG_ALTSTATUS);         \
    inb(d->io + IDE_REG_ALTSTATUS);         \
    inb(d->io + IDE_REG_ALTSTATUS);         \
}

#define __reset(d) {                        \
    outb(d->cp, 0x04);                      \
    __iowait(d)                             \
    outb(d->cp, 0x00);                      \
}


static inline int __stwait(struct disk* d, int tm) {
    int e;

    if(unlikely(tm > 0))
        while((e = inb(d->io + IDE_REG_STATUS)) & IDE_SR_BSY && (tm > 0))
            tm--;
    else
        while((e = inb(d->io + IDE_REG_STATUS)) & IDE_SR_BSY)
            ;

    return e;
}


static inline void __wait(struct disk* d) {
    __iowait(d);
    __stwait(d, 10000);


#if defined(DEBUG)
    int e = inb(d->io + IDE_REG_STATUS);

    DEBUG_ASSERT(!(e & IDE_SR_ERR));
    DEBUG_ASSERT(!(e & IDE_SR_DF));
    DEBUG_ASSERT(!(e & IDE_SR_DRQ));
#endif
}





static void* irq_1(void* frame) {
    DEBUG_ASSERT(frame);

    inb(ide.primary_master.io + IDE_REG_STATUS);

    return frame;
}

static void* irq_2(void* frame) {
    DEBUG_ASSERT(frame);

    inb(ide.secondary_master.io + IDE_REG_STATUS);

    return frame;
}



static void ide_init(device_t* device) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct disk* d = (struct disk*) device->userdata;


    outb(d->io + 1, 1);
    outb(d->cp, 0);
    outb(d->io + IDE_REG_HDDEVSEL, 0xA0 | d->type << 4);

    __iowait(d);

    outb(d->io + IDE_REG_COMMAND, IDE_CMD_IDENTIFY);

    __iowait(d);

    inb(d->io + IDE_REG_COMMAND);

    __wait(d);


    struct ide_identify identify;
    uint16_t* buf = (uint16_t*) &identify;

    int i;
    for(i = 0; i < 256; i++)
        buf[i] = inw(d->io);

#if defined(DEBUG)

    for(i = 0; i < 39; i += 2) {
        identify.model[i] ^= identify.model[i + 1];
        identify.model[i + 1] ^= identify.model[i];
        identify.model[i] ^= identify.model[i + 1];
    }

    identify.model[39] = '\0';
    identify.serial[19] = '\0';
    identify.firmware[7] = '\0';


    kprintf("ide: initialize device:\n"
            "   Model:      %s\n"
            "   Serial:     %s\n"
            "   Firmware:   %s\n"
            "   Size:       %6.3f GB\n",
        
        identify.model,
        identify.serial,
        identify.firmware,
        0
    );

#endif
}


static void ide_dnit(device_t* device) {

}

static void ide_reset(device_t* device) {

}


static int ide_write(device_t* device, const void* buf, off_t offset, size_t count) {

}


static int ide_read(device_t* device, void* buf, off_t offset, size_t count) {

}



static void disk_device(struct disk* disk, int cdrom) {
    
    static const char* device_name[2][4] = {
        { "sda", "sdb", "sdc", "sdd" },
        { "cda", "cdb", "cdc", "cdd" },
    };

    static int device_id[] = {
        0,
        0
    };


    device_t* d = (device_t*) kcalloc(sizeof(device_t), 1, GFP_KERNEL);
    
    d->type = DEVICE_TYPE_BLOCK;
    d->name = device_name[cdrom][device_id[cdrom]];
    d->description = "IDE Storage Device";

    d->vendorid = S_IFBLK;
    d->deviceid = (device_id[cdrom]++ * 2) + cdrom;
    d->intno = disk->intno;

    d->status = DEVICE_STATUS_UNKNOWN;

    d->init = ide_init;
    d->dnit = ide_dnit;
    d->reset = ide_reset;

    d->blk.blksize = cdrom
                        ? IDE_ATAPI_SECTOR_SIZE
                        : IDE_ATA_SECTOR_SIZE
                        ;

    d->blk.blkcount = cdrom
                        ? 358400    /* FIXME */
                        : 0
                        ;


    d->blk.write = ide_write;    
    d->blk.read = ide_read;    

    d->userdata = (void*) disk;


   device_mkdev(d, 0660);
}



static void disk_detect(struct disk* disk) {
    __reset(disk);
    __iowait(disk);

    outb(disk->io + IDE_REG_HDDEVSEL, 0xA0 | disk->type << 4);

    __iowait(disk);
    __stwait(disk, 10000);

    uint16_t cx;
    cx = (inb(disk->io + IDE_REG_LBA1)) |
         (inb(disk->io + IDE_REG_LBA2) << 8);


    switch(cx) {
    
        case 0xFFFF:
            return;

        case 0x0000:
        case 0xC33C:    /* ATA, SATA */
            disk_device(disk, 0);
            break;

        case 0xEB14:
        case 0x9669:    /* ATAPI, SATAPI */
            disk_device(disk, 1);
            break;

        default:
            kpanic("ide: detected unknown disk type: %p\n", cx);
    
    }

}




static void pci_find(pcidev_t device, uint16_t vid, uint16_t did, void* arg) {

    if((vid != 0x8086) || (did != 0x7010))
        return;


    ide.vendorid = vid;
    ide.deviceid = did;



    ide.primary_master.type = IDE_TYPE_MASTER;
    ide.primary_master.intno = IDE_IRQ_PRIMARY;
    ide.primary_master.io = pci_read(device, PCI_BAR0, 4) & PCI_BAR_IO_MASK;
    ide.primary_master.cp = pci_read(device, PCI_BAR1, 4) & PCI_BAR_IO_MASK;
    
    if(ide.primary_master.io < 2)
        ide.primary_master.io = 0x1F0;

    if(ide.primary_master.cp < 2)
        ide.primary_master.cp = 0x3F6;


    ide.primary_slave.type = IDE_TYPE_SLAVE;
    ide.primary_slave.intno = IDE_IRQ_PRIMARY;
    ide.primary_slave.io = ide.primary_master.io;
    ide.primary_slave.cp = ide.primary_master.cp;



    ide.secondary_master.type = IDE_TYPE_MASTER;
    ide.secondary_master.intno = IDE_IRQ_SECONDARY;
    ide.secondary_master.io = pci_read(device, PCI_BAR2, 4) & PCI_BAR_IO_MASK;
    ide.secondary_master.cp = pci_read(device, PCI_BAR3, 4) & PCI_BAR_IO_MASK;

    if(ide.secondary_master.io < 2)
        ide.secondary_master.io = 0x170;

    if(ide.secondary_master.cp < 2)
        ide.secondary_master.cp = 0x376;

    ide.secondary_slave.type = IDE_TYPE_SLAVE;
    ide.secondary_slave.intno = IDE_IRQ_SECONDARY;
    ide.secondary_slave.io = ide.primary_master.io;
    ide.secondary_slave.cp = ide.primary_master.cp;


    ide.dma = pci_read(device, PCI_BAR4, 4) & PCI_BAR_IO_MASK;


    /* Bus Mastering */
    uint32_t c = pci_read(device, PCI_COMMAND, 4);
    if(!(c & (1 << 2)))
        pci_write(device, PCI_COMMAND, 4, c | (1 << 2));

}



void init(const char* args) {

    pci_scan(&pci_find, 0x0101, NULL);

    if(!ide.vendorid)
        return;


    spinlock_init(&ide.lock);


    arch_intr_map_irq(IDE_IRQ_PRIMARY, irq_1);
    arch_intr_map_irq(IDE_IRQ_SECONDARY, irq_2);


    disk_detect(&ide.primary_master);
    //disk_detect(&ide.primary_slave);
    //disk_detect(&ide.secondary_master);
    //disk_detect(&ide.secondary_slave);
}


void dnit(void) {
    arch_intr_unmap_irq(IDE_IRQ_PRIMARY);
    arch_intr_unmap_irq(IDE_IRQ_SECONDARY);
}