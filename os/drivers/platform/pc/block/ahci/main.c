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
#include <aplus/ipc.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <dev/interface.h>
#include <dev/block.h>
#include <dev/pci.h>

#include <arch/x86/cpu.h>


MODULE_NAME("block/ahci");
MODULE_DEPS("dev/interface,dev/block,dev/pci");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



#define ATA_SR_BSY                      0x80
#define ATA_SR_DRDY                     0x40
#define ATA_SR_DF                       0x20
#define ATA_SR_DSC                      0x10
#define ATA_SR_DRQ                      0x08
#define ATA_SR_CORR                     0x04
#define ATA_SR_IDX                      0x02
#define ATA_SR_ERR                      0x01

#define ATA_ER_BBK                      0x80
#define ATA_ER_UNC                      0x40
#define ATA_ER_MC                       0x20
#define ATA_ER_IDNF                     0x10
#define ATA_ER_MCR                      0x08
#define ATA_ER_ABRT                     0x04
#define ATA_ER_TK0NF                    0x02
#define ATA_ER_AMNF                     0x01

#define ATA_CMD_READ_PIO                0x20
#define ATA_CMD_READ_PIO_EXT            0x24
#define ATA_CMD_READ_DMA                0xC8
#define ATA_CMD_READ_DMA_EXT            0x25
#define ATA_CMD_WRITE_PIO               0x30
#define ATA_CMD_WRITE_PIO_EXT           0x34
#define ATA_CMD_WRITE_DMA               0xCA
#define ATA_CMD_WRITE_DMA_EXT           0x35
#define ATA_CMD_CACHE_FLUSH             0xE7
#define ATA_CMD_CACHE_FLUSH_EXT         0xEA
#define ATA_CMD_PACKET                  0xA0
#define ATA_CMD_IDENTIFY_PACKET         0xA1
#define ATA_CMD_IDENTIFY                0xEC

#define ATAPI_CMD_READ                  0xA8
#define ATAPI_CMD_EJECT                 0x1B


#define FIS_TYPE_H2D                    0x27
#define FIS_TYPE_D2H                    0x34
#define FIS_TYPE_DMA_ACT                0x39
#define FIS_TYPE_DMA_SETUP              0x41
#define FIS_TYPE_DATA                   0x46
#define FIS_TYPE_BIST                   0x58
#define FIS_TYPE_PIO_SETUP              0x5F
#define FIS_TYPE_DEV_BITS               0xA1


#define ACHI_HBA_SIZE                   (0x1100)

#define AHCI_HBA_GHC_AE                 (1 << 31)
#define AHCI_HBA_GHC_MRSM               (1 << 2)
#define AHCI_HBA_GHC_IE                 (1 << 1)
#define AHCI_HBA_GHC_HR                 (1 << 0)

#define AHCI_PORT_IS_MASK               0x7DC000FF
#define AHCI_PORT_IS_TFES               (1 << 30)
#define AHCI_PORT_IS_HBFE               (1 << 29)
#define AHCI_PORT_IS_HBDE               (1 << 28)
#define AHCI_PORT_IS_IFE                (1 << 27)
#define AHCI_PORT_IS_INFE               (1 << 26)
#define AHCI_PORT_IS_OFE                (1 << 24)
#define AHCI_PORT_IS_IPME               (1 << 23)
#define AHCI_PORT_IS_PRCE               (1 << 22)
#define AHCI_PORT_IS_DPME               (1 << 7)
#define AHCI_PORT_IS_PCE                (1 << 6)
#define AHCI_PORT_IS_DPIE               (1 << 5)
#define AHCI_PORT_IS_UFE                (1 << 4)
#define AHCI_PORT_IS_SDBE               (1 << 3)
#define AHCI_PORT_IS_DSE                (1 << 2)
#define AHCI_PORT_IS_PSE                (1 << 1)
#define AHCI_PORT_IS_DHRE               (1 << 0)

#define AHCI_PORT_CMD_CR                (1 << 15)
#define AHCI_PORT_CMD_FR                (1 << 14)
#define AHCI_PORT_CMD_FRE               (1 << 4)
#define AHCI_PORT_CMD_ST                (1 << 0)

#define AHCI_PORT_STSS_DET              (15 << 0)
#define AHCI_PORT_STSS_SPD              (15 << 4)
#define AHCI_PORT_STSS_IPM              (15 << 8)

#define AHCI_PORT_SERR_DIAG             (0x07FF << 16)
#define AHCI_PORT_SERR_ERR              (0x0F03)


#define AHCI_MEMORY_SIZE                (1024 * 1024)
#define AHCI_MEMORY_IOCACHE             ( 512 * 1024)   /* 16 KiB for each device */

#define AHCI_MEMORY_AREA                \
    ((uintptr_t) &contiguous_memory_area - CONFIG_KERNEL_BASE)

/* See arch/x86/cma.asm */
extern int contiguous_memory_area;


typedef struct {

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

} __packed ata_identify_t;


typedef volatile struct {
    uint8_t type;

    uint8_t pmport:4;
    uint8_t rsv0:3;
    uint8_t c:1;

    uint8_t command;
    uint8_t featurel;

    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t featureh;

    uint8_t countl;
    uint8_t counth;
    uint8_t icc;
    uint8_t control;

    uint8_t rsv1[4];

} __packed fis_h2d_t;


typedef volatile struct {
    uint8_t type;

    uint8_t pmport:4;
    uint8_t rsv0:2;
    uint8_t i:1;
    uint8_t rsv1:1;

    uint8_t status;
    uint8_t error;

    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t rsv2;

    uint8_t countl;
    uint8_t counth;

    uint8_t rsv3[6];

} __packed fis_d2h_t;


typedef volatile struct {
    uint8_t type;

    uint8_t pmport:4;
    uint8_t rsv0:4;

    uint8_t rsv1[2];
    uint32_t payload[1];

} __packed fis_data_t;


typedef volatile struct {
    uint8_t type;

    uint8_t pmport:4;
    uint8_t rsv0:1;
    uint8_t d:1;
    uint8_t i:1;
    uint8_t rsv1:1;

    uint8_t status;
    uint8_t error;

    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t rsv2;

    uint8_t countl;
    uint8_t counth;
    uint8_t rsv3;
    uint8_t status2;

    uint16_t tc;

    uint8_t rsv4[4];

} __packed fis_pio_setup_t;


typedef volatile struct {
    uint8_t type;

    uint8_t pmport:4;
    uint8_t rsv0:1;
    uint8_t d:1;
    uint8_t i:1;
    uint8_t a:1;

    uint8_t rsv1[2];

    uint64_t id;
    uint32_t rsv2;
    uint32_t offset;
    uint32_t tc;
    uint32_t rsv3;

} __packed fis_dma_setup_t;


typedef volatile struct {
    union {
        uint32_t dw[8];

        struct {
            uint8_t cfl:5;
            uint8_t a:1;
            uint8_t w:1;
            uint8_t p:1;
            uint8_t r:1;
            uint8_t b:1;
            uint8_t c:1;
            uint8_t rsv:1;
            uint8_t pmp:4;
            uint16_t prdtl;

            uint32_t prdbc;
            
            uint32_t ctba;
            uint32_t ctbau;
        };
    };
} __packed hba_cmd_t;


typedef volatile struct {

    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t rsv0[48];

    struct {
        
        uint32_t dba;
        uint32_t dbau;
        uint32_t rsv0;

        uint32_t dbc:22;
        uint32_t rsv1:9;
        uint32_t i:1;

    } __packed prdt[1];

} __packed hba_cmd_table_t;


typedef volatile struct {

    fis_dma_setup_t dma;
    uint8_t __padding0[4];

    fis_pio_setup_t pio;
    uint8_t __padding1[12];

    fis_d2h_t d2h;
    uint8_t __padding2[4];

    //fis_dev_bits_t bits;
    uint8_t __padding3[8];

    uint8_t ufis[64];
    uint8_t __padding[96];

} __packed hba_fb_t;


typedef volatile struct hba {
    uint32_t caps;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;    
    uint32_t vs;    
    uint32_t ccc_ctl;    
    uint32_t ccc_pts;    
    uint32_t em_loc;    
    uint32_t em_ctl;    
    uint32_t caps_ext;    
    uint32_t bohc;

    uint8_t __padding[212];  

    struct {

        uint32_t clb;
        uint32_t clbu;
        uint32_t fb;
        uint32_t fbu;
        uint32_t is;
        uint32_t ie;        
        uint32_t cmd;        
        uint32_t rsv;        
        uint32_t tfd;        
        uint32_t sig;        
        uint32_t ssts;        
        uint32_t sctl;        
        uint32_t serr;        
        uint32_t sact;        
        uint32_t ci;        
        uint32_t sntf;        
        uint32_t fbs;
        uint32_t __padding[15];

    } __packed ports[32];
    
} __packed hba_t;


struct {
    uint32_t vendorid;
    uint32_t deviceid;
    uint8_t irq;

    semaphore_t io;

    hba_t volatile* hba;
} sata = { };




static void* irq(void* frame) {

    int p = sata.hba->is - 1;
    int s = sata.hba->ports[p].is;

    //DEBUG_ASSERT(!(s & AHCI_PORT_IS_TFES));
    DEBUG_ASSERT(!(s & AHCI_PORT_IS_HBFE));
    DEBUG_ASSERT(!(s & AHCI_PORT_IS_HBDE));
    DEBUG_ASSERT(!(s & AHCI_PORT_IS_IFE));
    DEBUG_ASSERT(!(s & AHCI_PORT_IS_INFE));
    DEBUG_ASSERT(!(s & AHCI_PORT_IS_OFE));
    DEBUG_ASSERT(!(s & AHCI_PORT_IS_IPME));
    DEBUG_ASSERT(!(s & AHCI_PORT_IS_PRCE));
    DEBUG_ASSERT(!(s & AHCI_PORT_IS_DPME));
    DEBUG_ASSERT(!(s & AHCI_PORT_IS_PCE));
    DEBUG_ASSERT(!(s & AHCI_PORT_IS_DPIE));
    DEBUG_ASSERT(!(s & AHCI_PORT_IS_UFE));
    DEBUG_ASSERT(!(s & AHCI_PORT_IS_SDBE));
    DEBUG_ASSERT(!(s & AHCI_PORT_IS_DSE));
    //DEBUG_ASSERT(!(s & AHCI_PORT_IS_PSE));
    //DEBUG_ASSERT(!(s & AHCI_PORT_IS_DHRE));



    if(s & AHCI_PORT_IS_DHRE)
        sem_post(&sata.io);

    
    if(s & AHCI_PORT_IS_TFES)
        s &= ~AHCI_PORT_IS_TFES;
    
   
    sata.hba->ports[p].is = s;
    sata.hba->is = p + 1;

    return frame;
}



static void sata_init(device_t* device) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    long i = (long) device->userdata - 1;


    hba_cmd_t volatile* cmd = (hba_cmd_t volatile*) (CONFIG_KERNEL_BASE + sata.hba->ports[i].clb);

    cmd->cfl = sizeof(fis_h2d_t*) / sizeof(uint32_t);
    cmd->w = 0;
    cmd->prdtl = 1;


    hba_cmd_table_t volatile* tbl = (hba_cmd_table_t volatile*) (CONFIG_KERNEL_BASE + cmd->ctba);

    tbl->prdt[0].dba = AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (i << 4);
    tbl->prdt[0].dbc = 512 - 1;
    tbl->prdt[0].i = 1;

    
    fis_h2d_t volatile* h2d = (fis_h2d_t volatile*) &tbl->cfis;

    h2d->type = FIS_TYPE_H2D;
    h2d->c = 1;
    h2d->command = ATA_CMD_IDENTIFY;
    h2d->device = 0;

    while(sata.hba->ports[i].tfd & (ATA_SR_BSY | ATA_SR_DRQ))
        __builtin_ia32_pause();


    sata.hba->ports[i].ci |= (1 << 0);

    sem_wait(&sata.io);


    if(sata.hba->ports[i].is & AHCI_PORT_IS_TFES)
        kpanic("ahci: device %d I/O Error on ATA_CMD_IDENTIFY");


    ata_identify_t identify;

    memcpy (
        &identify, 
        (void*) (CONFIG_KERNEL_BASE + AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (i << 4)),
        sizeof(identify)
    );


#if defined(DEBUG)

    int j;
    for(j = 0; j < 39; j += 2) {
        identify.model[j] ^= identify.model[j + 1];
        identify.model[j + 1] ^= identify.model[j];
        identify.model[j] ^= identify.model[j + 1];
    }

    identify.model[39] = '\0';
    identify.serial[19] = '\0';
    identify.firmware[7] = '\0';


    kprintf("ahci: initialize device %d:\n"
            "   Model:      %s\n"
            "   Serial:     %s\n"
            "   Firmware:   %s\n"
            "   Sector28:   %d\n"
            "   Sector48:   %d\n",
        
        i,
        identify.model,
        identify.serial,
        identify.firmware,
        identify.sectors_28,
        identify.sectors_48
    );

#endif


    device->blk.blkcount = identify.sectors_28
                            ? identify.sectors_28
                            : identify.sectors_48
                            ;

}


static void sata_dnit(device_t* device) {
    
    sata.hba->ghc &= ~AHCI_HBA_GHC_AE;

    /* TODO */
}


static void sata_reset(device_t* device) {

    sata.hba->ghc &= ~AHCI_HBA_GHC_IE;
    sata.hba->is = ~0;
    sata.hba->ghc |= AHCI_HBA_GHC_IE;



    /* TODO: See AHCI 1.3.1 - pg 114, 10.4.1 */
}


__thread_safe
static int sata_read(device_t* device, void* buf, off_t offset, size_t count) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(count);

    long d = (long) device->userdata - 1;

    DEBUG_ASSERT(d >= 0 && d < 32);


    uint32_t s = sata.hba->ports[d].sact |
                 sata.hba->ports[d].ci   ;

    int b = __builtin_ffs(~s) - 1;

    if(b == -1)
        kpanic("ahci: FAULT! no free command slot %p available for /dev/%s", s, device->name);



    hba_cmd_t volatile* cmd = (hba_cmd_t volatile*) (CONFIG_KERNEL_BASE + sata.hba->ports[d].clb);
    cmd += b;

    cmd->cfl = sizeof(fis_h2d_t) / sizeof(uint32_t);
    cmd->w = 0;
    cmd->prdtl = 1;


    hba_cmd_table_t volatile* tbl = (hba_cmd_table_t volatile*) (CONFIG_KERNEL_BASE + cmd->ctba);

    tbl->prdt[0].dba = AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (d << 4);
    tbl->prdt[0].dbc = (count << 9) - 1;
    tbl->prdt[0].i = 1;

    
    fis_h2d_t volatile* h2d = (fis_h2d_t volatile*) &tbl->cfis;

    h2d->type = FIS_TYPE_H2D;
    h2d->c = 1;
    h2d->command = ATA_CMD_READ_DMA_EXT;
    h2d->device = (1 << 6);

    h2d->lba0 = (offset >>  0) & 0xFF;
    h2d->lba1 = (offset >>  8) & 0xFF;
    h2d->lba2 = (offset >> 16) & 0xFF;
    h2d->lba3 = (offset >> 24) & 0xFF;

    if(sizeof(offset) > 4) {
        h2d->lba4 = ((uint64_t) offset >> 32) & 0xFF;
        h2d->lba5 = ((uint64_t) offset >> 40) & 0xFF;
    }

    h2d->countl = (count) & 0xFF;
    h2d->counth = (count >> 8) & 0xFF;


    while(sata.hba->ports[d].tfd & (ATA_SR_BSY | ATA_SR_DRQ))
        __builtin_ia32_pause();


    sata.hba->ports[d].ci |= (1 << b);


    // Polling
    //while(sata.hba->ports[d].ci & (1 << b))
    //    __builtin_ia32_pause();

    sem_wait(&sata.io);


    if(sata.hba->ports[d].is & AHCI_PORT_IS_TFES) {

        kprintf("ahci: FAULT! Task File Error: %s::read -> cmd(%d) tfd(%p) buf(%p) offset(%d) count(%d)\n", b, device->name, sata.hba->ports[d].tfd,  buf, offset, count);

        sata.hba->ports[d].is |= AHCI_PORT_IS_TFES;
        return errno = EIO, 0;
    }


    memcpy (
        buf, 
        (void*) (CONFIG_KERNEL_BASE + AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (d << 4)),
        count << 9
    );

    return count;
}


__thread_safe
static int sata_write(device_t* device, const void* buf, off_t offset, size_t count) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(count);

    long d = (long) device->userdata - 1;

    DEBUG_ASSERT(d >= 0 && d < 32);


    uint32_t s = sata.hba->ports[d].sact |
                 sata.hba->ports[d].ci   ;

    int b = __builtin_ffs(~s) - 1;

    if(b == -1)
        kpanic("ahci: FAULT! no free command slot %p available for /dev/%s", s, device->name);



    hba_cmd_t volatile* cmd = (hba_cmd_t volatile*) (CONFIG_KERNEL_BASE + sata.hba->ports[d].clb);
    cmd += b;

    cmd->cfl = sizeof(fis_h2d_t) / sizeof(uint32_t);
    cmd->w = 1;
    cmd->prdtl = 1;


    hba_cmd_table_t volatile* tbl = (hba_cmd_table_t volatile*) (CONFIG_KERNEL_BASE + cmd->ctba);

    tbl->prdt[0].dba = AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (d << 4);
    tbl->prdt[0].dbc = (count << 9) - 1;
    tbl->prdt[0].i = 1;

    
    fis_h2d_t volatile* h2d = (fis_h2d_t volatile*) &tbl->cfis;

    h2d->type = FIS_TYPE_H2D;
    h2d->c = 1;
    h2d->command = ATA_CMD_WRITE_DMA_EXT;
    h2d->device = (1 << 6);

    h2d->lba0 = (offset >>  0) & 0xFF;
    h2d->lba1 = (offset >>  8) & 0xFF;
    h2d->lba2 = (offset >> 16) & 0xFF;
    h2d->lba3 = (offset >> 24) & 0xFF;

    if(sizeof(offset) > 4) {
        h2d->lba4 = ((uint64_t) offset >> 32) & 0xFF;
        h2d->lba5 = ((uint64_t) offset >> 40) & 0xFF;
    }

    h2d->countl = (count) & 0xFF;
    h2d->counth = (count >> 8) & 0xFF;


    memcpy ( 
        (void*) (CONFIG_KERNEL_BASE + AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (d << 4)),
        buf,
        count << 9
    );


    while(sata.hba->ports[d].tfd & (ATA_SR_BSY | ATA_SR_DRQ))
        __builtin_ia32_pause();


    sata.hba->ports[d].ci |= (1 << b);


    // Polling
    //while(sata.hba->ports[d].ci & (1 << b))
    //    __builtin_ia32_pause();

    sem_wait(&sata.io);


    if(sata.hba->ports[d].is & AHCI_PORT_IS_TFES) {

        kprintf("ahci: FAULT! Task File Error: %s::write -> cmd(%d) tfd(%p) buf(%p) offset(%d) count(%d)\n", b, device->name, sata.hba->ports[d].tfd,  buf, offset, count);

        sata.hba->ports[d].is |= AHCI_PORT_IS_TFES;
        return errno = EIO, 0;
    }


    return count;
}



static void pci_find(pcidev_t device, uint16_t vid, uint16_t did, void* arg) {

    if((vid != 0x8086) || (did != 0x2922))
        return;

    sata.deviceid = did;
    sata.vendorid = vid;
    sata.irq = pci_read(device, PCI_INTERRUPT_LINE, 1);

    sata.hba = (hba_t volatile*) (pci_read(device, PCI_BAR5, 4) & PCI_BAR_MM_MASK);
}


void init(const char* args) {
    
    pci_scan(&pci_find, 0x0106, NULL);

    if(!sata.deviceid)
        return;


    sem_init(&sata.io, 1);
    

    arch_mmap (
        (void*) sata.hba, ACHI_HBA_SIZE,
        ARCH_MAP_NOEXEC |
        ARCH_MAP_FIXED  |
        ARCH_MAP_RDWR   |
        ARCH_MAP_UNCACHED
    );

    arch_intr_map_irq(sata.irq, &irq);


    sata.hba->ghc |= AHCI_HBA_GHC_AE;
    sata.hba->ghc &= ~AHCI_HBA_GHC_IE;


    int p = sata.hba->pi;
    int q = 0;

    long i;
    for(i = 0; i < 32; i++) {
        if(!(p & (1 << i)))
            continue;


        int s = sata.hba->ports[i].ssts;
        int c = sata.hba->ports[i].cmd;

        if((s & AHCI_PORT_STSS_DET) != 3)
            continue;

        if((s & AHCI_PORT_STSS_IPM) != 256)
            continue;

        
        if (
            (c & AHCI_PORT_CMD_ST)  ||
            (c & AHCI_PORT_CMD_CR)  ||
            (c & AHCI_PORT_CMD_FRE) ||
            (c & AHCI_PORT_CMD_FR)
        ) {

            sata.hba->ports[i].cmd &= ~AHCI_PORT_CMD_ST;

            while(sata.hba->ports[i].cmd & AHCI_PORT_CMD_CR)
                __builtin_ia32_pause();


            if(c & AHCI_PORT_CMD_FRE) {

                sata.hba->ports[i].cmd &= ~AHCI_PORT_CMD_FRE;

                while(sata.hba->ports[i].cmd & AHCI_PORT_CMD_FR)
                    __builtin_ia32_pause();

            }
        }

    
        sata.hba->ports[i].clb = AHCI_MEMORY_AREA + (i << 10);
        sata.hba->ports[i].clbu = 0;

        sata.hba->ports[i].fb = AHCI_MEMORY_AREA + (32 << 10) + (i << 8);
        sata.hba->ports[i].fbu = 0;



        hba_cmd_t volatile* clbp = (hba_cmd_t volatile*) (CONFIG_KERNEL_BASE + sata.hba->ports[i].clb);

        int j;
        for(j = 0; j < 32; j++) {
            
            clbp[j].prdtl = 8;
            clbp[j].ctba = AHCI_MEMORY_AREA + (40 << 10) + (i << 13) + (j << 8);
            clbp[j].ctbau = 0;

        }

        sata.hba->ports[i].is = 0;
        sata.hba->ports[i].ie = AHCI_PORT_IS_MASK;
        sata.hba->ports[i].serr |= AHCI_PORT_SERR_DIAG;
        sata.hba->ports[i].serr |= AHCI_PORT_SERR_ERR;


        while(sata.hba->ports[i].cmd & AHCI_PORT_CMD_CR)
            __builtin_ia32_pause();

        sata.hba->ports[i].cmd |= AHCI_PORT_CMD_FRE;
        sata.hba->ports[i].cmd |= AHCI_PORT_CMD_ST;


        q |= (1 << i);
    }


    sata.hba->is = 0;
    sata.hba->ghc |= AHCI_HBA_GHC_IE;


    for(i = 0; i < 32; i++) {
        if(!(q & (1 << i)))
            continue;


        device_t* d = (device_t*) kcalloc(sizeof(device_t), 1, GFP_KERNEL);

        d->type = DEVICE_TYPE_BLOCK;
        
        strncpy(d->name, "sda", DEVICE_MAXNAMELEN);
        strncpy(d->description, "SATA disk device", DEVICE_MAXDESCLEN);

        d->name[2] += i;

        d->major = 8;
        d->minor = i << 3;

        d->init = sata_init;
        d->dnit = sata_dnit;
        d->reset = sata_reset;

        d->blk.blksize = 512; /* FIXME */
        d->blk.blkcount = 0;
        d->blk.blkmax = (16 * 1024) / d->blk.blksize;
        d->blk.blkoff = 0;

        d->blk.read = sata_read;
        d->blk.write = sata_write;

        d->userdata = (void*) (i + 1);

        device_mkdev(d, 0660);
    }

}

void dnit(void) {
    /* TODO */
}