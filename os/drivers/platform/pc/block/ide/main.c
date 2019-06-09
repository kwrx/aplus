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
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <dev/interface.h>
#include <dev/block.h>
#include <dev/pci.h>


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


#define IDE_PRIMARY                     0x00
#define IDE_SECONDARY                   0x01

#define IDE_READ                        0x00
#define IDE_WRITE                       0x01




struct {
    uint32_t vendorid;
    uint32_t deviceid;

    struct {
        uint16_t io;
        uint16_t cp;
    } master;

    struct {
        uint16_t io;
        uint16_t cp;
    } slave;

    uintptr_t dma;
    spinlock_t lock;
} ide = { };


static void* irq_1(void* frame) {
    DEBUG_ASSERT(frame);

    inb(ide.master.io + IDE_REG_STATUS);

    return frame;
}

static void* irq_2(void* frame) {
    DEBUG_ASSERT(frame);

    inb(ide.slave.io + IDE_REG_STATUS);

    return frame;
}


static void pci_find(pcidev_t device, uint16_t vid, uint16_t did, void* arg) {

    if((vid != 0x8086) || (did != 0x7010))
        return;


    ide.vendorid = vid;
    ide.deviceid = did;

    ide.master.io = pci_read(device, PCI_BAR0, 4) & PCI_BAR_IO_MASK;
    ide.master.cp = pci_read(device, PCI_BAR1, 4) & PCI_BAR_IO_MASK;
    
    if(ide.master.io < 2)
        ide.master.io = 0x1F0;

    if(ide.master.cp < 2)
        ide.master.cp = 0x3F6;


    ide.slave.io = pci_read(device, PCI_BAR2, 4) & PCI_BAR_IO_MASK;
    ide.slave.io = pci_read(device, PCI_BAR3, 4) & PCI_BAR_IO_MASK;

    if(ide.slave.io < 2)
        ide.slave.io = 0x170;

    if(ide.master.cp < 2)
        ide.slave.cp = 0x376;


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


}


void dnit(void) {
    arch_intr_unmap_irq(IDE_IRQ_PRIMARY);
    arch_intr_unmap_irq(IDE_IRQ_SECONDARY);
}