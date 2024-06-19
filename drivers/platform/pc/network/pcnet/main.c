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
#include <aplus/memory.h>
#include <aplus/vfs.h>
#include <aplus/smp.h>
#include <aplus/hal.h>
#include <aplus/errno.h>
#include <aplus/utils/list.h>

#include <dev/interface.h>
#include <dev/network.h>
#include <dev/pci.h>

#include <arch/x86/cpu.h>



MODULE_NAME("network/pcnet");
MODULE_DEPS("dev/interface,dev/network,dev/pci");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");





#define PCNET_DE_SIZE           16
#define PCNET_BUFSIZE           1548
#define PCNET_RX_COUNT          32
#define PCNET_TX_COUNT          8

#define PCNET_MAX_DEVICES       32

#define PCNET_PCI_VENDOR_ID     0x1022
#define PCNET_PCI_DEVICE_ID     0x2000



struct pcnet {

    uint32_t pci;
    uint16_t io;
    uintptr_t mem;
    uint8_t irq;

    uintptr_t buf;
    uintptr_t rxdes;
    uintptr_t txdes;
    uintptr_t rxbuf;
    uintptr_t txbuf;

    uintptr_t vbuf;
    uintptr_t vrxdes;
    uintptr_t vtxdes;
    uintptr_t vrxbuf;
    uintptr_t vtxbuf;

    int rxid;
    int txid;

    uint8_t cache[0x4000];
    uint16_t size;
    uint16_t offset;

    spinlock_t lock;
    device_t device;

};

static struct pcnet* devices[PCNET_MAX_DEVICES] = { 0 };

static uint32_t pci_devices[PCNET_MAX_DEVICES];
static uint32_t pci_count = 0;




#define w_rap32(dev, x)                     \
    outl(dev->io + 0x14, x)

#define w_rap16(dev, x)                     \
    outw(dev->io + 0x12, x)

#define w_csr32(dev, x, y)                  \
    w_rap32(dev, x);                        \
    outl(dev->io + 0x10, y)

#define w_csr16(dev, x, y)                  \
    w_rap16(dev, x);                        \
    outw(dev->io + 0x10, y)

#define w_bcr32(dev, x, y)                  \
    w_rap32(dev, x);                        \
    outl(dev->io + 0x1C, y)




static inline uint32_t r_csr32(struct pcnet* dev, uint32_t x) {
    w_rap32(dev, x);
    return inl(dev->io + 0x10);
}

static inline uint16_t r_csr16(struct pcnet* dev, uint16_t x) {
    w_rap32(dev, x);
    return inw(dev->io + 0x10);
}

static inline uint32_t r_bcr32(struct pcnet* dev, uint32_t x) {
    w_rap32(dev, x);
    return inl(dev->io + 0x1C);
}

static inline int d_owns(uintptr_t data, int idx) {
    return (mmio_r8(data + PCNET_DE_SIZE * idx + 7) & 0x80) == 0;
}








static int pcnet_startoutput(void* internals) {

    DEBUG_ASSERT(internals);


    struct pcnet* dev = (struct pcnet*) internals;

    while(!d_owns(dev->vtxdes, dev->txid)) {

#if DEBUG_LEVEL_ERROR
        kprintf("pcnet: ERROR! no tx %d descriptor available!\n", dev->txid);
#endif

        if(++dev->txid == PCNET_TX_COUNT) {
            dev->txid = 0;
        }

    }

    dev->offset = 0;
    dev->size = 0;

    return 1;

}


static void pcnet_output(void* internals, void* buf, uint16_t len) {

    DEBUG_ASSERT(internals);


    struct pcnet* dev = (struct pcnet*) internals;

    memcpy((void*) ((uintptr_t) dev->cache + dev->offset), buf, len);
    dev->offset += len;

}


static void pcnet_endoutput(void* internals, uint16_t len) {

    DEBUG_ASSERT(internals);


    struct pcnet* dev = (struct pcnet*) internals;


    memcpy (
        (void*) (dev->vtxbuf + dev->txid * PCNET_BUFSIZE),
        (void*) (dev->cache),
        len
    );


    mmio_w8(dev->vtxdes + (dev->txid * PCNET_DE_SIZE + 7), mmio_r8(dev->vtxdes + (dev->txid * PCNET_DE_SIZE + 7)) | 0x02);
    mmio_w8(dev->vtxdes + (dev->txid * PCNET_DE_SIZE + 7), mmio_r8(dev->vtxdes + (dev->txid * PCNET_DE_SIZE + 7)) | 0x01);

    mmio_w16(dev->vtxdes + (dev->txid * PCNET_DE_SIZE + 4), (((-len) & 0x0FFF) | 0xF000));
    mmio_w8 (dev->vtxdes + (dev->txid * PCNET_DE_SIZE + 7), mmio_r8(dev->vtxdes + (dev->txid * PCNET_DE_SIZE + 7)) | 0x80);


    w_csr32(dev, 0, r_csr32(dev, 0) | (1 << 3));


#if DEBUG_LEVEL_TRACE
    kprintf("pcnet: TRACE! [%d] sending %d bytes from %d\n", arch_timer_generic_getms(), len, dev->txid);
#endif


    if(++dev->txid == PCNET_TX_COUNT) {
        dev->txid = 0;
    }

}


static int pcnet_startinput(void* internals) {

    DEBUG_ASSERT(internals);

    struct pcnet* dev = (struct pcnet*) internals;


    uint16_t size = mmio_r16(dev->vrxdes + (dev->rxid * PCNET_DE_SIZE + 8));

#if DEBUG_LEVEL_TRACE
    kprintf("pcnet: TRACE! [%d] received %d bytes from %d\n", arch_timer_generic_getms(), size, dev->rxid);
#endif

    dev->size = size;
    dev->offset = 0;

    return size;

}


static void pcnet_input(void* internals, void* buf, uint16_t len) {


    struct pcnet* dev = (struct pcnet*) internals;

    DEBUG_ASSERT(dev);
    DEBUG_ASSERT(dev->offset < dev->size);

    if(dev->offset + len > dev->size) {
        len = dev->size - dev->offset;
    }

    memcpy(buf, (const void*) (dev->vrxbuf + dev->rxid * PCNET_BUFSIZE + dev->offset), len);

// // #if DEBUG_LEVEL_TRACE
// //     kprintf("Dump: %d bytes\n", len);
// //     for(size_t i = 0; i < len; i++) {
// //         kprintf("%c", ((char*) buf)[i] & 0xFF);
// //     }
// //     kprintf("\n");
// // #endif

    dev->offset += len;

}


static void pcnet_endinput(void* internals) {

    DEBUG_ASSERT(internals);

    struct pcnet* dev = (struct pcnet*) internals;


    mmio_w8(dev->vrxdes + dev->rxid * PCNET_DE_SIZE + 7, 0x80);

    if(++dev->rxid == PCNET_RX_COUNT) {
        dev->rxid = 0;
    }

    dev->size   = 0;
    dev->offset = 0;

}


static void pcnet_input_nomem(void* internals, uint16_t len) {
    kpanicf("pcnet: PANIC! no memory left for %d bytes\n", len);
}


static void pcnet_irq(pcidev_t device, uint8_t irq, struct pcnet* dev) {

    DEBUG_ASSERT(dev);
    DEBUG_ASSERT(dev->irq == irq);

    while(d_owns(dev->vrxdes, dev->rxid)) {
        ethif_input(&dev->device.net.interface);
    }

    w_csr32(dev, 0, r_csr32(dev, 0) | 0x0400);

}


static void pcnet_init(void* internals, uint8_t* address, void* mcast) {

    DEBUG_ASSERT(internals);
    DEBUG_ASSERT(address);

    struct pcnet* eth = (struct pcnet*) internals;

    inl(eth->io + 0x18);
    inw(eth->io + 0x14);

    arch_timer_delay(10000);

    outl(eth->io + 0x10, 0);


    uint32_t b;

    b  = r_csr32(eth, 58);
    b &= 0xFFF0;
    b |= 2;

    w_csr32(eth, 58, b);


    b  = r_bcr32(eth, 2);
    b |= 2;

    w_bcr32(eth, 2, b);



    eth->rxdes = eth->buf + 28;
    eth->txdes = eth->buf + 28 + PCNET_RX_COUNT * PCNET_DE_SIZE;
    eth->rxbuf = eth->buf + 28 + PCNET_RX_COUNT * PCNET_DE_SIZE + PCNET_TX_COUNT * PCNET_DE_SIZE;
    eth->txbuf = eth->buf + 28 + PCNET_RX_COUNT * PCNET_DE_SIZE + PCNET_TX_COUNT * PCNET_DE_SIZE + PCNET_RX_COUNT * PCNET_BUFSIZE;


    eth->vbuf   = arch_vmm_p2v(eth->buf  , ARCH_VMM_AREA_HEAP);
    eth->vrxdes = arch_vmm_p2v(eth->rxdes, ARCH_VMM_AREA_HEAP);
    eth->vtxdes = arch_vmm_p2v(eth->txdes, ARCH_VMM_AREA_HEAP);
    eth->vrxbuf = arch_vmm_p2v(eth->rxbuf, ARCH_VMM_AREA_HEAP);
    eth->vtxbuf = arch_vmm_p2v(eth->txbuf, ARCH_VMM_AREA_HEAP);


    for(size_t i = 0; i < PCNET_RX_COUNT; i++) {

        memset((void*) (eth->vrxdes + i * PCNET_DE_SIZE), 0, PCNET_DE_SIZE);

        mmio_w32(eth->vrxdes + i * PCNET_DE_SIZE + 0, eth->rxbuf + i * PCNET_BUFSIZE);
        mmio_w16(eth->vrxdes + i * PCNET_DE_SIZE + 4, ((-PCNET_BUFSIZE) & 0x0FFF) | 0xF000);
        mmio_w8 (eth->vrxdes + i * PCNET_DE_SIZE + 7, 0x80);

    }


    for(size_t i = 0; i < PCNET_TX_COUNT; i++) {

        memset((void*) (eth->vtxdes + i * PCNET_DE_SIZE), 0, PCNET_DE_SIZE);

        mmio_w32(eth->vtxdes + i * PCNET_DE_SIZE + 0, eth->txbuf + i * PCNET_BUFSIZE);
        mmio_w16(eth->vtxdes + i * PCNET_DE_SIZE + 4, ((-PCNET_BUFSIZE) & 0x0FFF) | 0xF000);

    }


    mmio_w8(eth->vbuf + 0, 0x00);
    mmio_w8(eth->vbuf + 1, 0x00);
    mmio_w8(eth->vbuf + 2, 5 << 4);
    mmio_w8(eth->vbuf + 3, 3 << 4);
    mmio_w8(eth->vbuf + 4, address[0]);
    mmio_w8(eth->vbuf + 5, address[1]);
    mmio_w8(eth->vbuf + 6, address[2]);
    mmio_w8(eth->vbuf + 7, address[3]);
    mmio_w8(eth->vbuf + 8, address[4]);
    mmio_w8(eth->vbuf + 9, address[5]);


    for(size_t i = 10; i < 20; i++) {
        mmio_w8(eth->vbuf + i, 0);
    }


    mmio_w32(eth->vbuf + 20, eth->rxdes);
    mmio_w32(eth->vbuf + 24, eth->txdes);


    w_csr32(eth, 1, (eth->buf) & 0xFFFF);
    w_csr32(eth, 2, (eth->buf >> 16) & 0xFFFF);

    r_csr32(eth, 1);
    r_csr32(eth, 2);


    uint16_t c = r_csr32(eth, 3);

    if(c & (1 << 10)) {
        c ^= (1 << 10);
    }

    if(c & (1 << 2)) {
        c ^= (1 << 2);
    }

    c |= (1 << 9);
    c |= (1 << 8);

    w_csr32(eth, 3, c);


    c = r_csr32(eth, 4) | (1 << 1) | (1 << 12) | (1 << 14);
    w_csr32(eth, 4, c);

    c = r_csr32(eth, 0) | (1 << 0) | (1 << 6);
    w_csr32(eth, 0, c);


    while((r_csr32(eth, 0) & (1 << 8)) == 0)
        ;


    c = r_csr32(eth, 0);

    if(c & (1 << 0)) {
        c ^= (1 << 0);
    }

    if(c & (1 << 2)) {
        c ^= (1 << 2);
    }

    c |= (1 << 1);

    w_csr32(eth, 0, c);

}





static void pcnet_find_pci(uint32_t device, uint16_t venid, uint16_t devid, void* data) {

    if(pci_count >= PCNET_MAX_DEVICES)
        return;

    if((venid == PCNET_PCI_VENDOR_ID) && (devid == PCNET_PCI_DEVICE_ID)) {
        pci_devices[pci_count++] = device;
    }

}



void init(const char* args) {

    if(strstr(core->boot.cmdline, "network=off"))
        return;



    pci_scan(&pcnet_find_pci, -1, NULL);


    if(pci_count == 0) {
#if DEBUG_LEVEL_ERROR
        kprintf("pcnet: ERROR! pci device not found!\n");
#endif
        return;
    }


    for(size_t i = 0; i < pci_count; i++) {

        struct pcnet* eth = (struct pcnet*) kcalloc(1, sizeof(struct pcnet), GFP_KERNEL);

        eth->pci = pci_devices[i];
        eth->buf = pmm_alloc_blocks(16);

        pci_enable_pio(eth->pci);
        pci_enable_mmio(eth->pci);
        pci_enable_bus_mastering(eth->pci);


        eth->irq = pci_read(eth->pci, PCI_INTERRUPT_LINE, 1);
        eth->io  = pci_read(eth->pci, PCI_BAR0, 4) & 0xFFFFFFF0;
        eth->mem = pci_read(eth->pci, PCI_BAR1, 4) & 0xFFFFFFF0;

        spinlock_init(&eth->lock);



        DEBUG_ASSERT(sizeof(eth->device.name) > strlen("pcnet"));
        DEBUG_ASSERT(sizeof(eth->device.description) > strlen("PCNET Network Adapter"));

        strcpy(eth->device.name, "pcnet");
        strcpy(eth->device.description, "PCNET Network Adapter");

        eth->device.major  = 144;
        eth->device.minor  = 1;
        eth->device.type   = DEVICE_TYPE_NETWORK;
        eth->device.status = DEVICE_STATUS_UNKNOWN;

        for(size_t j = 0; j < 6; j++) {
            eth->device.net.address[j] = inb(eth->io + j);
        }

        eth->device.net.low_level_init         = pcnet_init;
        eth->device.net.low_level_startoutput  = pcnet_startoutput;
        eth->device.net.low_level_output       = pcnet_output;
        eth->device.net.low_level_endoutput    = pcnet_endoutput;
        eth->device.net.low_level_startinput   = pcnet_startinput;
        eth->device.net.low_level_input        = pcnet_input;
        eth->device.net.low_level_endinput     = pcnet_endinput;
        eth->device.net.low_level_input_nomem  = pcnet_input_nomem;

        IP4_ADDR(&eth->device.net.ip, 10, 0, 2, 15 + i);
        IP4_ADDR(&eth->device.net.nm, 255, 255, 255, 0);
        IP4_ADDR(&eth->device.net.gw, 10, 0, 2, 2);

        eth->device.net.interface.state = &eth->device;
        eth->device.net.internals = eth;


        if(!netif_add( &eth->device.net.interface,
                       &eth->device.net.ip,
                       &eth->device.net.nm,
                       &eth->device.net.gw, &eth->device, ethif_init, ethernet_input)) {

            kpanicf("pcnet: PANIC! netif_add() failed\n");

        }


        if(eth->irq != PCI_INTERRUPT_LINE_NONE) {

            pci_intx_map_irq(eth->pci, eth->irq, (pci_irq_handler_t) pcnet_irq, (pci_irq_data_t) eth);
            pci_intx_unmask(eth->pci);

        }


        netif_set_default(&eth->device.net.interface);
        netif_set_up(&eth->device.net.interface);



        device_mkdev(&eth->device, 0666);

        devices[i] = eth;
        devices[i + 1] = NULL;


    }

}

void dnit(void) {

}