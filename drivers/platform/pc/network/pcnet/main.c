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

    int rxid;
    int txid;

    uint8_t cache[0x4000];
    uint16_t size;
    uint16_t offset;

    spinlock_t lock;
    device_t device;

};

static struct pcnet* devices[PCNET_MAX_DEVICES];





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
    return (((uint8_t*) arch_vmm_p2v(data, ARCH_VMM_AREA_HEAP)) [PCNET_DE_SIZE * idx + 7] & 0x80) == 0;
}








static int pcnet_startoutput(void* internals) {

    DEBUG_ASSERT(internals);


    struct pcnet* dev = (struct pcnet*) internals;

    while(!d_owns(dev->txdes, dev->txid)) {

#if defined(DEBUG) && DEBUG_LEVEL >= 2
        kprintf("pcnet: ERROR! no tx %d descriptor available!\n", dev->txid);
#endif

        if(++dev->txid == PCNET_TX_COUNT)
            dev->txid = 0;

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
        (void*) (arch_vmm_p2v(dev->txbuf, ARCH_VMM_AREA_HEAP) + dev->txid * PCNET_BUFSIZE), 
        (void*) (dev->cache), 
        len
    );


    *((uint8_t*) arch_vmm_p2v(dev->txdes, ARCH_VMM_AREA_HEAP) + (dev->txid * PCNET_DE_SIZE + 7)) |= 0x02;
    *((uint8_t*) arch_vmm_p2v(dev->txdes, ARCH_VMM_AREA_HEAP) + (dev->txid * PCNET_DE_SIZE + 7)) |= 0x01;


    uint16_t b = (uint16_t) (-len);
    b &= 0x0FFF;
    b |= 0xF000;

    *(uint16_t*) (arch_vmm_p2v(dev->txdes, ARCH_VMM_AREA_HEAP) + (dev->txid * PCNET_DE_SIZE + 4)) = b;
    *(uint8_t*)  (arch_vmm_p2v(dev->txdes, ARCH_VMM_AREA_HEAP) + (dev->txid * PCNET_DE_SIZE + 7)) |= 0x80;

    
    w_csr32(dev, 0, r_csr32(dev, 0) | (1 << 3));


#if defined(DEBUG) && DEBUG_LEVEL >= 4
    kprintf("pcnet: INFO! [%d] sending %d bytes from %d\n", arch_timer_generic_getms(), len, dev->txid);
#endif


    if(++dev->txid == PCNET_TX_COUNT)
        dev->txid = 0;

}


static int pcnet_startinput(void* internals) {

    DEBUG_ASSERT(internals);

    struct pcnet* dev = (struct pcnet*) internals;
    

    uint16_t size = *(uint16_t*) (arch_vmm_p2v(dev->rxdes, ARCH_VMM_AREA_HEAP) + (dev->rxid * PCNET_DE_SIZE + 8));

#if defined(DEBUG) && DEBUG_LEVEL >= 4
    kprintf("pcnet: INFO! [%d] received %d bytes from %d\n", arch_timer_generic_getms(), size, dev->rxid);
#endif


    memcpy(dev->cache, (void*) (arch_vmm_p2v(dev->rxbuf, ARCH_VMM_AREA_HEAP) + dev->rxid * PCNET_BUFSIZE), size);

    dev->size = size;
    dev->offset = 0;

    return size;

}


static void pcnet_input(void* internals, void* buf, uint16_t len) {


    struct pcnet* dev = (struct pcnet*) internals;

    DEBUG_ASSERT(dev);
    DEBUG_ASSERT(dev->offset < dev->size);
        


    if(dev->offset + len > dev->size)
        len = dev->size - dev->offset;

    memcpy(buf, &dev->cache[dev->offset], len);


    int i;
    for(i = 0; i < len; i++)
        kprintf("%c ", ((char*)buf)[i] & 0xFF);


    dev->offset += len;

}


static void pcnet_endinput(void* internals) {
    
    DEBUG_ASSERT(internals);
    
    struct pcnet* dev = (struct pcnet*) internals;
    

    ((uint8_t*) arch_vmm_p2v(dev->rxdes, ARCH_VMM_AREA_HEAP)) [dev->rxid * PCNET_DE_SIZE + 7] = 0x80;


    if(++dev->rxid == PCNET_RX_COUNT)
        dev->rxid = 0;

    dev->size =
    dev->offset = 0;

}


static void pcnet_input_nomem(void* internals, uint16_t len) {
    kpanicf("pcnet: PANIC! no memory left for %d bytes\n", len);   
}




static void pcnet_irq(void* frame, uint8_t irq) {
    
    struct pcnet* dev;
    for(int i = 0; (dev = devices[i]); i++) {
    
        if(dev->irq != irq)
            continue;

        
        // FIXME: Use semaphore for pcnet rx ownership
        while(d_owns(dev->rxdes, dev->rxid))
            ethif_input(&dev->device.net.interface);

        w_csr32(dev, 0, r_csr32(dev, 0) | 0x0400);

    }

}


static void pcnet_init(void* internals, uint8_t* address, void* mcast) {
    
    DEBUG_ASSERT(internals);
    DEBUG_ASSERT(address);
    //DEBUG_ASSERT(mcast);
    
    
    struct pcnet* dev = (struct pcnet*) internals;

    inl(dev->io + 0x18);
    inw(dev->io + 0x14);

    arch_timer_delay(10000);
    
    
    outl(dev->io + 0x10, 0);
    

    uint32_t x;

    x = r_csr32(dev, 58);
    x &= 0xFFF0;
    x |= 2;

    w_csr32(dev, 58, x);


    x = r_bcr32(dev, 2);
    x |= 2;

    w_bcr32(dev, 2, x);



    dev->rxdes = dev->buf + 28;
    dev->txdes = dev->rxdes + PCNET_RX_COUNT * PCNET_DE_SIZE;
    dev->rxbuf = dev->txdes + PCNET_TX_COUNT * PCNET_DE_SIZE;
    dev->txbuf = dev->rxbuf + PCNET_RX_COUNT * PCNET_BUFSIZE;


    int i;
    for(i = 0; i < PCNET_RX_COUNT; i++) {

        uint8_t* d = (uint8_t*) arch_vmm_p2v(dev->rxdes, ARCH_VMM_AREA_HEAP);

        memset(&d[i * PCNET_DE_SIZE], 0, PCNET_DE_SIZE);


        *(uint32_t*) &d[i * PCNET_DE_SIZE] = (uint32_t) (dev->rxbuf + i * PCNET_BUFSIZE);

        uint16_t b = (uint16_t) (-PCNET_BUFSIZE);
        b &= 0x0FFF;
        b |= 0xF000;
        *(uint16_t*) &d[i * PCNET_DE_SIZE + 4] = (uint16_t) b;

        d[i * PCNET_DE_SIZE + 7] = 0x80;

    }



    for(i = 0; i < PCNET_TX_COUNT; i++) {

        uint8_t* d = (uint8_t*) arch_vmm_p2v(dev->txdes, ARCH_VMM_AREA_HEAP);

        memset(&d[i * PCNET_DE_SIZE], 0, PCNET_DE_SIZE);


        *(uint32_t*) &d[i * PCNET_DE_SIZE] = (uint32_t) (dev->txbuf + i * PCNET_BUFSIZE);

        uint16_t b = (uint16_t) (-PCNET_BUFSIZE);
        b &= 0x0FFF;
        b |= 0xF000;
        *(uint16_t*) &d[i * PCNET_DE_SIZE + 4] = (uint16_t) b;

    }



    ((uint8_t*) arch_vmm_p2v(dev->buf, ARCH_VMM_AREA_HEAP)) [0] = 0;
    ((uint8_t*) arch_vmm_p2v(dev->buf, ARCH_VMM_AREA_HEAP)) [1] = 0;
    ((uint8_t*) arch_vmm_p2v(dev->buf, ARCH_VMM_AREA_HEAP)) [2] = 5 << 4;
    ((uint8_t*) arch_vmm_p2v(dev->buf, ARCH_VMM_AREA_HEAP)) [3] = 3 << 4;
    ((uint8_t*) arch_vmm_p2v(dev->buf, ARCH_VMM_AREA_HEAP)) [4] = address[0];
    ((uint8_t*) arch_vmm_p2v(dev->buf, ARCH_VMM_AREA_HEAP)) [5] = address[1];
    ((uint8_t*) arch_vmm_p2v(dev->buf, ARCH_VMM_AREA_HEAP)) [6] = address[2];
    ((uint8_t*) arch_vmm_p2v(dev->buf, ARCH_VMM_AREA_HEAP)) [7] = address[3];
    ((uint8_t*) arch_vmm_p2v(dev->buf, ARCH_VMM_AREA_HEAP)) [8] = address[4];
    ((uint8_t*) arch_vmm_p2v(dev->buf, ARCH_VMM_AREA_HEAP)) [9] = address[5];



    for(i = 10; i < 20; i++)
        ((uint8_t*) arch_vmm_p2v(dev->buf, ARCH_VMM_AREA_HEAP)) [i] = 0;



    ((uint32_t*) (arch_vmm_p2v(dev->buf, ARCH_VMM_AREA_HEAP) + 20)) [0] = (uint32_t) dev->rxdes;
    ((uint32_t*) (arch_vmm_p2v(dev->buf, ARCH_VMM_AREA_HEAP) + 24)) [0] = (uint32_t) dev->txdes;


    w_csr32(dev, 1, (dev->buf) & 0xFFFF);
    w_csr32(dev, 2, (dev->buf >> 16) & 0xFFFF);

    r_csr32(dev, 1);
    r_csr32(dev, 2);


    uint16_t c = r_csr32(dev, 3);

    if(c & (1 << 10))
        c ^= (1 << 10);
    if(c & (1 << 2))
        c ^= (1 << 2);

    c |= (1 << 9);
    c |= (1 << 8);

    w_csr32(dev, 3, c);


    c = r_csr32(dev, 4) | (1 << 1) | (1 << 12) | (1 << 14);
    w_csr32(dev, 4, c);


    c = r_csr32(dev, 0) | (1 << 0) | (1 << 6);
    w_csr32(dev, 0, c);


    uint32_t s;
    while(((s = r_csr32(dev, 0)) & (1 << 8)) == 0)
        ;


    c = r_csr32(dev, 0);
    if(c & (1 << 0))
        c ^= (1 << 0);
    if(c & (1 << 2))
        c ^= (1 << 2);
    
    c |= (1 << 1);

    w_csr32(dev, 0, c);


}






void init(const char* args) {

    
    int pci_devices[PCNET_MAX_DEVICES];
    int pci_count = 0;


    void find_pci(uint32_t device, uint16_t venid, uint16_t devid, void* data) {
        
        if((venid == 0x1022) && (devid == 0x2000))
            pci_devices[pci_count++] = device;

        DEBUG_ASSERT(pci_count < PCNET_MAX_DEVICES);

    }



    pci_scan(&find_pci, -1, NULL);


    if(!pci_count) {
#if defined(DEBUG) && DEBUG_LEVEL >= 2
        kprintf("pcnet: ERROR! pci device not found!\n");
#endif
        return;
    }


    int i;
    for(i = 0; i < pci_count; i++) {

    
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


#if defined(DEBUG) && DEBUG_LEVEL >= 2
        kprintf("pcnet: index(%d) irq(%d) io(%p) mem(%p)\n", i, eth->irq, eth->io, eth->mem);
#endif




        strcpy(eth->device.name, "pcnet");
        strcpy(eth->device.description, "PCNET Network Adapter");

        eth->device.major = 144;
        eth->device.minor = i;

        eth->device.type   = DEVICE_TYPE_NETWORK;
        eth->device.status = DEVICE_STATUS_UNKNOWN;



        int j;
        for(j = 0; j < 6; j++)
            eth->device.net.address[j] = inb(eth->io + j);


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


        arch_intr_map_irq(eth->irq, pcnet_irq);

        netif_set_default(&eth->device.net.interface);
        netif_set_up(&eth->device.net.interface);



        device_mkdev(&eth->device, 0666);

        devices[i] = eth;
        devices[i + 1] = NULL;

    }

    
}


void dnit(void) {
    
}