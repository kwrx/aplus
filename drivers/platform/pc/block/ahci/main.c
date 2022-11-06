/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aplus.
 * 
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/memory.h>
#include <aplus/ipc.h>
#include <aplus/smp.h>
#include <aplus/hal.h>
#include <aplus/errno.h>

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


#define AHCI_HBA_SIZE                   (0x1100)

#define AHCI_HBA_GHC_AE                 (1U << 31)
#define AHCI_HBA_GHC_MRSM               (1U << 2)
#define AHCI_HBA_GHC_IE                 (1U << 1)
#define AHCI_HBA_GHC_HR                 (1U << 0)

#define AHCI_HBA_CAPS_EXT_BOH           (1 << 0)

#define AHCI_HBA_BOHC_BOS               (1 << 0)
#define AHCI_HBA_BOHC_OOS               (1 << 1)


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


#define AHCI_SIG_NULL                   (0xFFFFFFFF)
#define AHCI_SIG_ATA                    (0x00000101)
#define AHCI_SIG_ATAPI                  (0xEB140101)
#define AHCI_SIG_SEMB                   (0xC33C0101)
#define AHCI_SIG_PM                     (0x96690101)


#define AHCI_MEMORY_SIZE                (1024 * 1024)
#define AHCI_MEMORY_IOCACHE             ( 512 * 1024)   /* 16 KiB for each device */

#define AHCI_MEMORY_AREA                (ahci->contiguous_memory_area)
#define AHCI_MAX_DEVICES                (32)

#define AHCI_DEVICE_INDEX(d)            (((long) d->minor >> 3))




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


struct ahci {

    uintptr_t contiguous_memory_area;

    pcidev_t deviceid;
    uint8_t irq;

    semaphore_t io;

    hba_t volatile* hba;

};


static struct ahci devices[AHCI_MAX_DEVICES];
static uint8_t devices_count = 0;






static void irq(pcidev_t device, uint8_t irq, struct ahci* ahci) {
    
    DEBUG_ASSERT(ahci);
    DEBUG_ASSERT(ahci->irq == irq);


    int p = ahci->hba->is - 1;
    int s = ahci->hba->ports[p].is;

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
        sem_post(&ahci->io);

    
    if(s & AHCI_PORT_IS_TFES)
        s &= ~AHCI_PORT_IS_TFES;
    

    ahci->hba->ports[p].is = s;
    ahci->hba->is = p + 1;

    

}




static void satapi_init(device_t* device) {
    
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);


    struct ahci* ahci = (struct ahci*) device->userdata;


    long i = AHCI_DEVICE_INDEX(device);

    DEBUG_ASSERT(i >= 0 && i < 32);


    hba_cmd_t volatile* cmd = (hba_cmd_t volatile*) (arch_vmm_p2v(ahci->hba->ports[i].clb, ARCH_VMM_AREA_HEAP));

    cmd->cfl = sizeof(fis_h2d_t) / sizeof(uint32_t);
    cmd->w = 0;
    cmd->a = 0;
    cmd->prdtl = 1;


    hba_cmd_table_t volatile* tbl = (hba_cmd_table_t volatile*) (arch_vmm_p2v(cmd->ctba, ARCH_VMM_AREA_HEAP));

    tbl->prdt[0].dba = AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (i << 4);
    tbl->prdt[0].dbc = 512 - 1;
    tbl->prdt[0].i = 1;

    
    memset((void*) &tbl->cfis, 0, sizeof(tbl->cfis));
    memset((void*) &tbl->acmd, 0, sizeof(tbl->acmd));

    memset ( (void*)
        (arch_vmm_p2v(AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (i << 4), ARCH_VMM_AREA_HEAP)),
        (0),
        512
    );


    
    fis_h2d_t volatile* h2d = (fis_h2d_t volatile*) &tbl->cfis;

    h2d->type = FIS_TYPE_H2D;
    h2d->c = 1;
    h2d->command = ATA_CMD_IDENTIFY_PACKET;
    h2d->device = 0;


    while(ahci->hba->ports[i].tfd & (ATA_SR_BSY | ATA_SR_DRQ))
        __builtin_ia32_pause();



    ahci->hba->ports[i].ci |= (1 << 0);

    sem_wait(&ahci->io);


    if(ahci->hba->ports[i].is & AHCI_PORT_IS_TFES)
        kpanicf("ahci: PANIC! device %d I/O Error on ATA_CMD_IDENTIFY_PACKET");


    ata_identify_t identify;

    memcpy (
        &identify, 
        (void*) (arch_vmm_p2v(AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (i << 4), ARCH_VMM_AREA_HEAP)),
        sizeof(identify)
    );




    /* Read Capacity */

    cmd->cfl = sizeof(fis_h2d_t) / sizeof(uint32_t);
    cmd->w = 0;
    cmd->a = 1;
    cmd->prdtl = 1;

    tbl->prdt[0].dba = AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (i << 4);
    tbl->prdt[0].dbc = 2048 - 1;
    tbl->prdt[0].i = 1;

    
    memset((void*) &tbl->cfis, 0, sizeof(tbl->cfis));
    memset((void*) &tbl->acmd, 0, sizeof(tbl->acmd));

    memset ( (void*)
        (arch_vmm_p2v(AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (i << 4), ARCH_VMM_AREA_HEAP)),
        (0),
        2048
    );



    h2d->type = FIS_TYPE_H2D;
    h2d->c = 1;
    h2d->command = ATA_CMD_PACKET;
    h2d->device = 0;

    h2d->lba0 = (2048) & 0xFF;
    h2d->lba1 = (2048 >> 8) & 0xFF;


    uint8_t* command = (uint8_t*) (&tbl->acmd);

    command[0] = 0x25; /* 25h: SCSI_READ_CAPACITY */
    command[1] = 0x00;
    command[2] = 0x00;


    while(ahci->hba->ports[i].tfd & (ATA_SR_BSY | ATA_SR_DRQ))
        __builtin_ia32_pause();



    ahci->hba->ports[i].ci |= (1 << 0);

    sem_wait(&ahci->io);


    if(ahci->hba->ports[i].is & AHCI_PORT_IS_TFES)
        kpanicf("ahci: PANIC! device %d I/O Error on ATA_CMD_PACKET (SCSI::ReadCapacity)", i);



    /* Update info */
    device->blk.blkcount = __builtin_bswap32(mmio_r32(arch_vmm_p2v(AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (i << 4) + 0, ARCH_VMM_AREA_HEAP)));
    device->blk.blksize  = __builtin_bswap32(mmio_r32(arch_vmm_p2v(AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (i << 4) + 4, ARCH_VMM_AREA_HEAP)));

    DEBUG_ASSERT(device->blk.blkcount);
    DEBUG_ASSERT(device->blk.blksize == 2048);


    device->blk.blkmax = (16 * 1024) / device->blk.blksize;



#if DEBUG_LEVEL_TRACE

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
            "   Type:       ATAPI\n"           
            "   Model:      %s\n"
            "   Serial:     %s\n"
            "   Firmware:   %s\n"
            "   Sectors:    %d\n"
            "   Block:      %d\n",
        
        i,
        identify.model,
        identify.serial,
        identify.firmware,
        __builtin_bswap32(mmio_r32(arch_vmm_p2v(AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (i << 4) + 0, ARCH_VMM_AREA_HEAP))),
        __builtin_bswap32(mmio_r32(arch_vmm_p2v(AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (i << 4) + 4, ARCH_VMM_AREA_HEAP)))
    );
#endif

}



static ssize_t satapi_read(device_t* device, void* buf, off_t offset, size_t count) {
   
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(count);


    struct ahci* ahci = (struct ahci*) device->userdata;


    long d = AHCI_DEVICE_INDEX(device);

    DEBUG_ASSERT(d >= 0 && d < 32);


    uint32_t s = ahci->hba->ports[d].sact |
                 ahci->hba->ports[d].ci   ;

    int b = __builtin_ffs(~s) - 1;

    if(b == -1)
        kpanicf("ahci: FAULT! no free command slot %p available for /dev/%s", s, device->name);



    hba_cmd_t volatile* cmd = (hba_cmd_t volatile*) (arch_vmm_p2v(ahci->hba->ports[d].clb, ARCH_VMM_AREA_HEAP));
    cmd += b;

    cmd->cfl = sizeof(fis_h2d_t) / sizeof(uint32_t);
    cmd->w = 0;
    cmd->a = 1;
    cmd->prdtl = 1;


    hba_cmd_table_t volatile* tbl = (hba_cmd_table_t volatile*) (arch_vmm_p2v(cmd->ctba, ARCH_VMM_AREA_HEAP));

    tbl->prdt[0].dba = AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (d << 4);
    tbl->prdt[0].dbc = (count << 11) - 1;
    tbl->prdt[0].i = 1;


    memset((void*) &tbl->cfis, 0, sizeof(tbl->cfis));
    memset((void*) &tbl->acmd, 0, sizeof(tbl->acmd));

    memset ( (void*)
        (arch_vmm_p2v(AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (d << 4), ARCH_VMM_AREA_HEAP)),
        (0),
        (count << 11)
    );
    


    fis_h2d_t volatile* h2d = (fis_h2d_t volatile*) &tbl->cfis;

    h2d->type = FIS_TYPE_H2D;
    h2d->c = 1;
    h2d->command = ATA_CMD_PACKET;
    h2d->device = 0;

    h2d->lba0 = ((count << 11) >> 0) & 0xFF;
    h2d->lba1 = ((count << 11) >> 8) & 0xFF;



    uint8_t* command = (uint8_t*) (&tbl->acmd);

    command[0] = 0x28; /* 28h: SCSI_READ (10) */
    command[1] = 0x00;
    command[2] = (offset >> 24) & 0xFF;
    command[3] = (offset >> 16) & 0XFF;
    command[4] = (offset >>  8) & 0XFF;
    command[5] = (offset >>  0) & 0XFF;
    command[6] = 0x0000;
    command[7] = (count >> 8) & 0xFF;
    command[8] = (count >> 0) & 0xFF;
    command[9] = 0x00;




    while(ahci->hba->ports[d].tfd & (ATA_SR_BSY | ATA_SR_DRQ))
        __builtin_ia32_pause();


    ahci->hba->ports[d].ci |= (1 << b);



    // if(current_cpu->flags & SMP_CPU_FLAGS_INTERRUPT) {

        while(ahci->hba->ports[d].ci & (1 << b))     /* Polling */
            __builtin_ia32_pause();
    
    // } else
        // sem_wait(&ahci->io);



    if(ahci->hba->ports[d].is & AHCI_PORT_IS_TFES) {

        kprintf("ahci: FAULT! Task File Error: %s::read -> cmd(%d) tfd(%p) buf(%p) offset(%d) count(%d)\n", b, device->name, ahci->hba->ports[d].tfd,  buf, offset, count);

        ahci->hba->ports[d].is |= AHCI_PORT_IS_TFES;
        return errno = EIO, 0;
    }


    memcpy (
        buf, 
        (void*) (arch_vmm_p2v(AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (d << 4), ARCH_VMM_AREA_HEAP)),
        count << 11
    );


    DEBUG_ASSERT(cmd->prdbc == (count << 11));
    return count;
}



static void sata_init(device_t* device) {
   
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct ahci* ahci = (struct ahci*) device->userdata;


    long i = AHCI_DEVICE_INDEX(device);

    DEBUG_ASSERT(i >= 0 && i < 32);


    hba_cmd_t volatile* cmd = (hba_cmd_t volatile*) (arch_vmm_p2v(ahci->hba->ports[i].clb, ARCH_VMM_AREA_HEAP));

    cmd->cfl = sizeof(fis_h2d_t) / sizeof(uint32_t);
    cmd->w = 0;
    cmd->a = 0;
    cmd->prdtl = 1;


    hba_cmd_table_t volatile* tbl = (hba_cmd_table_t volatile*) (arch_vmm_p2v(cmd->ctba, ARCH_VMM_AREA_HEAP));

    tbl->prdt[0].dba = AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (i << 4);
    tbl->prdt[0].dbc = 512 - 1;
    tbl->prdt[0].i = 1;


    memset((void*) &tbl->cfis, 0, sizeof(tbl->cfis));
    memset((void*) &tbl->acmd, 0, sizeof(tbl->acmd));

    memset ( (void*)
        (arch_vmm_p2v(AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (i << 4), ARCH_VMM_AREA_HEAP)),
        (0),
        512
    );

    

    fis_h2d_t volatile* h2d = (fis_h2d_t volatile*) &tbl->cfis;

    h2d->type = FIS_TYPE_H2D;
    h2d->c = 1;
    h2d->command = ATA_CMD_IDENTIFY;
    h2d->device = 0;


    while(ahci->hba->ports[i].tfd & (ATA_SR_BSY | ATA_SR_DRQ))
        __builtin_ia32_pause();



    ahci->hba->ports[i].ci |= (1 << 0);

    sem_wait(&ahci->io);


    if(ahci->hba->ports[i].is & AHCI_PORT_IS_TFES)
        kpanicf("ahci: PANIC! device %d I/O Error on ATA_CMD_IDENTIFY", i);



    ata_identify_t identify;

    memcpy (
        &identify, 
        (void*) (arch_vmm_p2v(AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (i << 4), ARCH_VMM_AREA_HEAP)),
        sizeof(identify)
    );


    device->blk.blkcount = identify.sectors_28
                            ? identify.sectors_28
                            : identify.sectors_48
                            ;


#if DEBUG_LEVEL_TRACE

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
            "   Type:       ATA\n"
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

}


static void sata_dnit(device_t* device) {
    
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct ahci* ahci = (struct ahci*) device->userdata;


    long i = AHCI_DEVICE_INDEX(device);

    DEBUG_ASSERT(i >= 0 && i < 32);


    ahci->hba->ports[i].ie = 0;
    ahci->hba->ports[i].serr = 0;

    ahci->hba->ports[i].cmd &= ~AHCI_PORT_CMD_ST;
    ahci->hba->ports[i].cmd &= ~AHCI_PORT_CMD_FRE;

    /* TODO: implements sata device de-initialitiation */

}


static void sata_reset(device_t* device) {
    
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);

    struct ahci* ahci = (struct ahci*) device->userdata;


    long i = AHCI_DEVICE_INDEX(device);

    DEBUG_ASSERT(i >= 0 && i < 32);



    int c = ahci->hba->ports[i].cmd;

    if (
        (c & AHCI_PORT_CMD_ST)  ||
        (c & AHCI_PORT_CMD_CR)  ||
        (c & AHCI_PORT_CMD_FRE) ||
        (c & AHCI_PORT_CMD_FR)
    ) {

        ahci->hba->ports[i].cmd &= ~AHCI_PORT_CMD_ST;

        while(ahci->hba->ports[i].cmd & AHCI_PORT_CMD_CR)
            __builtin_ia32_pause();


        if(c & AHCI_PORT_CMD_FRE) {

            ahci->hba->ports[i].cmd &= ~AHCI_PORT_CMD_FRE;

            while(ahci->hba->ports[i].cmd & AHCI_PORT_CMD_FR)
                __builtin_ia32_pause();

        }
    }

        
    ahci->hba->ports[i].is = 0;
    ahci->hba->ports[i].ie = AHCI_PORT_IS_MASK;

    ahci->hba->ports[i].serr = 0;
    ahci->hba->ports[i].serr |= AHCI_PORT_SERR_DIAG;
    ahci->hba->ports[i].serr |= AHCI_PORT_SERR_ERR;


    while(ahci->hba->ports[i].cmd & AHCI_PORT_CMD_CR)
        __builtin_ia32_pause();


    ahci->hba->ports[i].cmd |= AHCI_PORT_CMD_FRE;
    ahci->hba->ports[i].cmd |= AHCI_PORT_CMD_ST;


    /* TODO: see AHCI 1.3.1 - pg 114, 10.4.1 */

}



static ssize_t sata_read(device_t* device, void* buf, off_t offset, size_t count) {
    
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(count);
    DEBUG_ASSERT(count <= 32);

    struct ahci* ahci = (struct ahci*) device->userdata;


    long d = AHCI_DEVICE_INDEX(device);

    DEBUG_ASSERT(d >= 0 && d < 32);


    uint32_t s = ahci->hba->ports[d].sact |
                 ahci->hba->ports[d].ci   ;

    int b = __builtin_ffs(~s) - 1;

    if(b == -1)
        kpanicf("ahci: FAULT! no free command slot %p available for /dev/%s", s, device->name);



    hba_cmd_t volatile* cmd = (hba_cmd_t volatile*) (arch_vmm_p2v(ahci->hba->ports[d].clb, ARCH_VMM_AREA_HEAP));
    cmd += b;

    cmd->cfl = sizeof(fis_h2d_t) / sizeof(uint32_t);
    cmd->w = 0;
    cmd->a = 0;
    cmd->prdtl = 1;


    hba_cmd_table_t volatile* tbl = (hba_cmd_table_t volatile*) (arch_vmm_p2v(cmd->ctba, ARCH_VMM_AREA_HEAP));

    tbl->prdt[0].dba = AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (d << 4);
    tbl->prdt[0].dbc = (count << 9) - 1;
    tbl->prdt[0].i = 1;


    memset((void*) &tbl->cfis, 0, sizeof(tbl->cfis));
    memset((void*) &tbl->acmd, 0, sizeof(tbl->acmd));

    memset ( (void*)
        (arch_vmm_p2v(AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (d << 4), ARCH_VMM_AREA_HEAP)),
        (0),
        (count << 9)
    );
    


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


    while(ahci->hba->ports[d].tfd & (ATA_SR_BSY | ATA_SR_DRQ))
        __builtin_ia32_pause();


    ahci->hba->ports[d].ci |= (1 << b);



    // if(current_cpu->flags & SMP_CPU_FLAGS_INTERRUPT) {

        while(ahci->hba->ports[d].ci & (1 << b))     /* Polling */
            __builtin_ia32_pause();
    
    // } else
    //     sem_wait(&ahci->io);



    if(ahci->hba->ports[d].is & AHCI_PORT_IS_TFES) {

        kprintf("ahci: FAULT! Task File Error: %s::read -> cmd(%d) tfd(%p) buf(%p) offset(%d) count(%d)\n", b, device->name, ahci->hba->ports[d].tfd,  buf, offset, count);

        ahci->hba->ports[d].is |= AHCI_PORT_IS_TFES;
        return errno = EIO, 0;
    }


    memcpy (
        buf, 
        (void*) (arch_vmm_p2v(AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (d << 4), ARCH_VMM_AREA_HEAP)),
        count << 9
    );


    DEBUG_ASSERT(cmd->prdbc == (count << 9));
    return count;
}



static ssize_t sata_write(device_t* device, const void* buf, off_t offset, size_t count) {
    
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->userdata);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(count);
    DEBUG_ASSERT(count <= 32);

    struct ahci* ahci = (struct ahci*) device->userdata;


    long d = AHCI_DEVICE_INDEX(device);

    DEBUG_ASSERT(d >= 0 && d < 32);


    uint32_t s = ahci->hba->ports[d].sact |
                 ahci->hba->ports[d].ci   ;

    int b = __builtin_ffs(~s) - 1;

    if(b == -1)
        kpanicf("ahci: FAULT! no free command slot %p available for /dev/%s", s, device->name);



    hba_cmd_t volatile* cmd = (hba_cmd_t volatile*) (arch_vmm_p2v(ahci->hba->ports[d].clb, ARCH_VMM_AREA_HEAP));
    cmd += b;

    cmd->cfl = sizeof(fis_h2d_t) / sizeof(uint32_t);
    cmd->w = 1;
    cmd->a = 0;
    cmd->prdtl = 1;


    hba_cmd_table_t volatile* tbl = (hba_cmd_table_t volatile*) (arch_vmm_p2v(cmd->ctba, ARCH_VMM_AREA_HEAP));    

    tbl->prdt[0].dba = AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (d << 4);
    tbl->prdt[0].dbc = (count << 9) - 1;
    tbl->prdt[0].i = 1;


    memset((void*) &tbl->cfis, 0, sizeof(tbl->cfis));
    memset((void*) &tbl->acmd, 0, sizeof(tbl->acmd));

    memset ( (void*)
        (arch_vmm_p2v(AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (d << 4), ARCH_VMM_AREA_HEAP)),
        (0),
        (count << 9)
    );

    

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


    memcpy ( (void*)
        (arch_vmm_p2v(AHCI_MEMORY_AREA + AHCI_MEMORY_IOCACHE + (d << 4), ARCH_VMM_AREA_HEAP)),
        (buf),
        (count << 9)
    );


    while(ahci->hba->ports[d].tfd & (ATA_SR_BSY | ATA_SR_DRQ))
        __builtin_ia32_pause();


    ahci->hba->ports[d].ci |= (1 << b);



    // if(current_cpu->flags & SMP_CPU_FLAGS_INTERRUPT) {

        while(ahci->hba->ports[d].ci & (1 << b))     /* Polling */
            __builtin_ia32_pause();
    
    // } else
    //     sem_wait(&ahci->io);



    if(ahci->hba->ports[d].is & AHCI_PORT_IS_TFES) {

        kprintf("ahci: FAULT! Task File Error: %s::write -> cmd(%d) tfd(%p) buf(%p) offset(%d) count(%d)\n", b, device->name, ahci->hba->ports[d].tfd,  buf, offset, count);

        ahci->hba->ports[d].is |= AHCI_PORT_IS_TFES;
        return errno = EIO, 0;
    }


    DEBUG_ASSERT(cmd->prdbc == (count << 9));
    return count;

}



static void pci_find(pcidev_t device, uint16_t vid, uint16_t did, void* arg) {

    static struct {
        uint16_t vid;
        uint16_t did;
    } supported_devices[] = {
        { 0x8086, 0x2652 },
        { 0x8086, 0x2653 },
        { 0x8086, 0x2681 },
        { 0x8086, 0x2682 },
        { 0x8086, 0x2683 },
        { 0x8086, 0x27c1 },
        { 0x8086, 0x27c3 },
        { 0x8086, 0x27c5 },
        { 0x8086, 0x27c6 },
        { 0x8086, 0x2821 },
        { 0x8086, 0x2822 },
        { 0x8086, 0x2824 },
        { 0x8086, 0x2829 },
        { 0x8086, 0x282a },
        { 0x8086, 0x2922 },
        { 0x8086, 0x2923 },
        { 0x8086, 0x2924 },
        { 0x8086, 0x2925 },
        { 0x8086, 0x2927 },
        { 0x8086, 0x2929 },
        { 0x8086, 0x292a },
        { 0x8086, 0x292b },
        { 0x8086, 0x292c },
        { 0x8086, 0x292f },
        { 0x8086, 0x294d },
        { 0x8086, 0x294e },
        { 0x8086, 0x3a05 },
        { 0x8086, 0x3a22 },
        { 0x8086, 0x3a25 },
        { 0, 0 }
    };


    if(({

        bool found = false;

        for(size_t i = 0; supported_devices[i].vid; i++) {

            if(vid != supported_devices[i].vid || did != supported_devices[i].did)
                continue;

            found = true;
            break;

        }

        found;

    }) == false) {
        return;
    }



    struct ahci* ahci = &devices[devices_count++];

    pci_enable_pio(device);
    pci_enable_mmio(device);
    pci_enable_bus_mastering(device);

    ahci->deviceid = device;
    ahci->irq = pci_read(device, PCI_INTERRUPT_LINE, 1);

    ahci->hba = (hba_t volatile*) (pci_read(device, PCI_BAR5, 4) & PCI_BAR_MM_MASK);


    uint32_t size = pci_bar_size(device, PCI_BAR5, 4);


    arch_vmm_map (
        &core->bsp.address_space,
        (uintptr_t) ahci->hba,
        (uintptr_t) ahci->hba, size,
        ARCH_VMM_MAP_NOEXEC |
        ARCH_VMM_MAP_FIXED  |
        ARCH_VMM_MAP_RDWR   |
        ARCH_VMM_MAP_UNCACHED
    );

#if DEBUG_LEVEL_TRACE
    kprintf("ahci: pci device found: index(%d) vendor(%x) device(%x) irq(%d) hba(%p) size(%p)\n", devices_count - 1, vid, did, ahci->irq, ahci->hba, size);
#endif

}


void init(const char* args) {

    pci_scan(&pci_find, PCI_TYPE_SATA, NULL);


    struct ahci* ahci;
    for(int index = 0; (index < devices_count) && (ahci = &devices[index]); index++) {

    
        if(!ahci->deviceid)
            continue;


        ahci->contiguous_memory_area = pmm_alloc_blocks(AHCI_MEMORY_SIZE >> 12);

#if DEBUG_LEVEL_TRACE
    kprintf("ahci: contiguous memory area: address(%p) size(%p)\n", ahci->contiguous_memory_area, AHCI_MEMORY_SIZE);
#endif

        
        if(ahci->irq != PCI_INTERRUPT_LINE_NONE) {

            pci_intx_map_irq(ahci->deviceid, ahci->irq, (pci_irq_handler_t) &irq, (pci_irq_data_t) ahci);
            pci_intx_unmask(ahci->deviceid);

        }



        // BIOS/OS Handoff Control
        if(ahci->hba->caps_ext & AHCI_HBA_CAPS_EXT_BOH) {
            
            ahci->hba->bohc |= AHCI_HBA_BOHC_OOS;

            while(
                !(ahci->hba->bohc & AHCI_HBA_BOHC_OOS) ||
                (ahci->hba->bohc & AHCI_HBA_BOHC_BOS)
            )
                __builtin_ia32_pause();

        }


        sem_init(&ahci->io, 0);


        ahci->hba->ghc |= AHCI_HBA_GHC_AE;
        ahci->hba->ghc &= ~AHCI_HBA_GHC_IE;

        int p = ahci->hba->pi;
        int q = 0;
        int w = 0;



        long i;
        for(i = 0; i < 32; i++) {

            if(!(p & (1UL << i)))
                continue;


            int s = ahci->hba->ports[i].ssts;
            int c = ahci->hba->ports[i].cmd;
            int m = ahci->hba->ports[i].sig;

            if((s & AHCI_PORT_STSS_DET) != (3 << 0))    /* Device detected and connected */
                continue;

            if((s & AHCI_PORT_STSS_IPM) != (1 << 8))    /* Interface active */
                continue;

#if DEBUG_LEVEL_WARN
            if((s & AHCI_PORT_STSS_SPD) != (3 << 4))    /* 6 Gbps */
                kprintf("ahci: WARN! device %d has a slow interface speed: %s Gbps\n", i,
                            &("0\0  "
                            "1.5\0"
                            "3\0  "
                            "6\0  ")[((s & AHCI_PORT_STSS_SPD) >> 4) * 4]);
#endif


            switch(m) {
                case AHCI_SIG_NULL:
                case AHCI_SIG_SEMB:
                case AHCI_SIG_PM:
                    continue;

                case AHCI_SIG_ATA:
                case AHCI_SIG_ATAPI:
                    break;

                default:
                    kpanicf("ahci: PANIC! invalid device %d signature: %p\n", i, m);
            }

            
            if (
                (c & AHCI_PORT_CMD_ST)  ||
                (c & AHCI_PORT_CMD_CR)  ||
                (c & AHCI_PORT_CMD_FRE) ||
                (c & AHCI_PORT_CMD_FR)
            ) {

                ahci->hba->ports[i].cmd &= ~AHCI_PORT_CMD_ST;

                while(ahci->hba->ports[i].cmd & AHCI_PORT_CMD_CR)
                    __builtin_ia32_pause();


                if(c & AHCI_PORT_CMD_FRE) {

                    ahci->hba->ports[i].cmd &= ~AHCI_PORT_CMD_FRE;

                    while(ahci->hba->ports[i].cmd & AHCI_PORT_CMD_FR)
                        __builtin_ia32_pause();

                }
            }

        
            ahci->hba->ports[i].clb = AHCI_MEMORY_AREA + (i << 10);
            ahci->hba->ports[i].clbu = 0;

            ahci->hba->ports[i].fb = AHCI_MEMORY_AREA + (32 << 10) + (i << 8);
            ahci->hba->ports[i].fbu = 0;



            hba_cmd_t volatile* clbp = (hba_cmd_t volatile*) (arch_vmm_p2v(ahci->hba->ports[i].clb, ARCH_VMM_AREA_HEAP));

            int j;
            for(j = 0; j < 32; j++) {
                
                clbp[j].prdtl = 8;
                clbp[j].ctba = AHCI_MEMORY_AREA + (40 << 10) + (i << 13) + (j << 8);
                clbp[j].ctbau = 0;

            }

            ahci->hba->ports[i].is = 0;
            ahci->hba->ports[i].ie = AHCI_PORT_IS_MASK;
            
            ahci->hba->ports[i].serr = 0;
            ahci->hba->ports[i].serr |= AHCI_PORT_SERR_DIAG;
            ahci->hba->ports[i].serr |= AHCI_PORT_SERR_ERR;


            while(ahci->hba->ports[i].cmd & AHCI_PORT_CMD_CR)
                __builtin_ia32_pause();


            ahci->hba->ports[i].cmd |= AHCI_PORT_CMD_FRE;
            ahci->hba->ports[i].cmd |= AHCI_PORT_CMD_ST;

    
            if(m == AHCI_SIG_ATA)
                q |= (1 << i);
            else
                w |= (1 << i);
        }


        ahci->hba->is = 0;
        ahci->hba->ghc |= AHCI_HBA_GHC_IE;



        /* ATA Devices */
        for(i = 0; i < 32; i++) {

            if(!(q & (1UL << i)))
                continue;


            device_t* d = (device_t*) kcalloc(sizeof(device_t), 1, GFP_KERNEL);
            
            strncpy(d->name, "sda", DEVICE_MAXNAMELEN);
            strncpy(d->description, "SATA disk device", DEVICE_MAXDESCLEN);

            d->name[2] += (index << 4) + i;


            d->type = DEVICE_TYPE_BLOCK;
            d->major = index ? 65 + index : 8;
            d->minor = i << 3;

            d->init = sata_init;
            d->dnit = sata_dnit;
            d->reset = sata_reset;

            d->blk.blksize = 512;
            d->blk.blkcount = 0;
            d->blk.blkmax = (16 * 1024) / d->blk.blksize;
            d->blk.blkoff = 0;

            d->blk.read = sata_read;
            d->blk.write = sata_write;

            d->userdata = (void*) ahci;

            device_mkdev(d, 0660);

        }



        /* ATAPI Devices */
        for(i = 0; i < 32; i++) {

            if(!(w & (1UL << i)))
                continue;


            device_t* d = (device_t*) kcalloc(sizeof(device_t), 1, GFP_KERNEL);
            
            strncpy(d->name, "scd0", DEVICE_MAXNAMELEN);
            strncpy(d->description, "SATA CD-ROM device", DEVICE_MAXDESCLEN);

            d->name[3] += (index << 4) + i;


            d->type = DEVICE_TYPE_BLOCK;
            d->major = 11;
            d->minor = i << 3;

            d->init = satapi_init;
            d->dnit = sata_dnit;
            d->reset = sata_reset;

            d->blk.blksize = 2048;
            d->blk.blkcount = 0;
            d->blk.blkmax = (16 * 1024) / d->blk.blksize;
            d->blk.blkoff = 0;

            d->blk.read = satapi_read;
            d->blk.write = NULL;

            d->userdata = (void*) ahci;

            device_mkdev(d, 0660);

        }

    }

}



void dnit(void) {
    
    struct ahci* ahci;
    for(int index = 0; (index < devices_count) && (ahci = &devices[index]); index++) {

    
        if(!ahci->deviceid)
            continue;


        ahci->hba->ghc &= ~AHCI_HBA_GHC_IE;
        ahci->hba->ghc &= ~AHCI_HBA_GHC_AE;
        ahci->hba->is = ~0;


        arch_intr_unmap_irq(ahci->irq);
        arch_vmm_unmap (&core->bsp.address_space, (uintptr_t) ahci->hba, AHCI_HBA_SIZE);

        /* TODO: implements ahci de-initialitation */

    }

}