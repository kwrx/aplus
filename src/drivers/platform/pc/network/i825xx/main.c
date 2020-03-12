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
#include <aplus/memory.h>
#include <aplus/vfs.h>
#include <aplus/errno.h>
#include <stdint.h>
#include <string.h>

#include <dev/interface.h>
#include <dev/network.h>
#include <dev/pci.h>

#include <hal/timer.h>
#include <hal/vmm.h>
#include <hal/interrupt.h>

#include <arch/x86/cpu.h>

#include <aplus/utils/list.h>



MODULE_NAME("network/i825xx");
MODULE_DEPS("dev/interface,dev/network,dev/pci");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



#define i825xx_REG_CTRL             (dev->mmio_address + 0x0000)
#define i825xx_REG_STATUS           (dev->mmio_address + 0x0008)
#define i825xx_REG_EECD             (dev->mmio_address + 0x0010)
#define i825xx_REG_EERD             (dev->mmio_address + 0x0014)

#define i825xx_REG_MDIC             (dev->mmio_address + 0x0020)

#define i825xx_REG_IMS              (dev->mmio_address + 0x00D0)
#define i825xx_REG_RCTL             (dev->mmio_address + 0x0100)
#define i825xx_REG_TCTL             (dev->mmio_address + 0x0400)

#define i825xx_REG_RDBAL            (dev->mmio_address + 0x2800)
#define i825xx_REG_RDBAH            (dev->mmio_address + 0x2804)
#define i825xx_REG_RDLEN            (dev->mmio_address + 0x2808)
#define i825xx_REG_RDH              (dev->mmio_address + 0x2810)
#define i825xx_REG_RDT              (dev->mmio_address + 0x2818)

#define i825xx_REG_TDBAL            (dev->mmio_address + 0x3800)
#define i825xx_REG_TDBAH            (dev->mmio_address + 0x3804)
#define i825xx_REG_TDLEN            (dev->mmio_address + 0x3808)
#define i825xx_REG_TDH              (dev->mmio_address + 0x3810)
#define i825xx_REG_TDT              (dev->mmio_address + 0x3818)

#define i825xx_REG_MTA              (dev->mmio_address + 0x5200)

#define i825xx_REG_RAL              (dev->mmio_address + 0x5400)
#define i825xx_REG_RAH              (dev->mmio_address + 0x5404)

#define i825xx_MAX_DEVICES          32


#define i825xx_PHYREG_PCTRL         (0)
#define i825xx_PHYREG_PSTATUS       (1)
#define i825xx_PHYREG_PSSTAT        (17)


#define NUM_RX_DESCRIPTORS          768
#define NUM_TX_DESCRIPTORS          768

#define CTRL_FD                     (1 << 0)
#define CTRL_ASDE                   (1 << 5)
#define CTRL_SLU                    (1 << 6)

#define MDIC_PHYADD                 (1 << 21)
#define MDIC_OP_WRITE               (1 << 26)
#define MDIC_OP_READ                (2 << 26)
#define MDIC_R                      (1 << 28)
#define MDIC_I                      (1 << 29)
#define MDIC_E                      (1 << 30)



#define RCTL_EN                     (1 << 1)
#define RCTL_SBP                    (1 << 2)
#define RCTL_UPE                    (1 << 3)
#define RCTL_MPE                    (1 << 4)
#define RCTL_LPE                    (1 << 5)
#define RDMTS_HALF                  (0 << 8)
#define RDMTS_QUARTER               (1 << 8)
#define RDMTS_EIGHTH                (2 << 8)
#define RCTL_BAM                    (1 << 15)
#define RCTL_BSIZE_256              (3 << 16)
#define RCTL_BSIZE_512              (2 << 16)
#define RCTL_BSIZE_1024             (1 << 16)
#define RCTL_BSIZE_2048             (0 << 16)
#define RCTL_BSIZE_4096             ((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192             ((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384            ((1 << 16) | (1 << 25))
#define RCTL_SECRC                  (1 << 26)

#define TCTL_EN                     (1 << 1)
#define TCTL_PSP                    (1 << 3)



// RX and TX descriptor structures
typedef struct __attribute__((packed)) i825xx_rx_desc_s {

    volatile uint64_t address;
    volatile uint16_t length;
    volatile uint16_t checksum;
    volatile uint8_t  status;
    volatile uint8_t  errors;
    volatile uint16_t special;

} i825xx_rx_desc_t;


typedef struct __attribute__((packed)) i825xx_tx_desc_s {

    volatile uint64_t address;
    volatile uint16_t length;
    volatile uint8_t  cso;
    volatile uint8_t  cmd;
    volatile uint8_t  sta;
    volatile uint8_t  css;
    volatile uint16_t special;

} i825xx_tx_desc_t;



struct i825xx {

    uintptr_t mmio_address;
    uint32_t pci;
    uint16_t irq;
    
    volatile uint8_t* rx_desc_base;
    volatile i825xx_rx_desc_t* rx_desc[NUM_RX_DESCRIPTORS];
    volatile uint16_t rx_tail;
    
    volatile uint8_t* tx_desc_base;
    volatile i825xx_tx_desc_t* tx_desc[NUM_TX_DESCRIPTORS];
    volatile uint16_t tx_tail;

    spinlock_t lock;
    device_t device;

};

static struct i825xx* devices[i825xx_MAX_DEVICES];


static uint16_t net_i825xx_eeprom_read(struct i825xx* dev, uint8_t addr) {

    DEBUG_ASSERT(dev);
    

    mmio_w32(i825xx_REG_EERD, (1) | ((uint32_t)(addr) << 8));
    
    uint32_t i;
    //while(!((i = mmio_r32(i825xx_REG_EERD)) & (1 << 4)))
    //    __builtin_ia32_pause();
            
    return (uint16_t)((i >> 16) & 0xFFFF);

}


static uint16_t net_i825xx_phy_read(struct i825xx *dev, int MDIC_REGADD) {

    DEBUG_ASSERT(dev);

    
    mmio_w32(i825xx_REG_MDIC, (((MDIC_REGADD & 0x1F) << 16) | 
                                 MDIC_PHYADD | MDIC_I | MDIC_OP_READ));


    while(!(mmio_r32(i825xx_REG_MDIC) & (MDIC_R | MDIC_E)))
        __builtin_ia32_pause();

    
    if(mmio_r32(i825xx_REG_MDIC) & MDIC_E)
        kpanicf("i825xx: PANIC! MDI READ ERROR\n");
    
    
    return (uint16_t)(mmio_r32(i825xx_REG_MDIC) & 0xFFFF);

}


static void net_i825xx_phy_write(struct i825xx *dev, int MDIC_REGADD, uint16_t MDIC_DATA) {

    DEBUG_ASSERT(dev);


    mmio_w32(i825xx_REG_MDIC, ((MDIC_DATA & 0xFFFF) | ((MDIC_REGADD & 0x1F) << 16) | 
                                MDIC_PHYADD | MDIC_I | MDIC_OP_WRITE));
                                    
    while(!(mmio_r32(i825xx_REG_MDIC) & (MDIC_R | MDIC_E)))
        __builtin_ia32_pause();

    
    if(mmio_r32(i825xx_REG_MDIC) & MDIC_E )
        kpanicf("i825xx: PANIC! MDI WRITE ERROR\n");
    
}



static void net_i825xx_rx_enable(struct i825xx* dev) {
    mmio_w32(i825xx_REG_RCTL, mmio_r32(i825xx_REG_RCTL) | (RCTL_EN));
}


static void net_i825xx_rx_init(struct i825xx* dev) {

    DEBUG_ASSERT(dev);


    dev->rx_desc_base = (void*) arch_vmm_p2v(pmm_alloc_blocks(16), ARCH_VMM_AREA_HEAP);
    
    int i;
    for(i = 0; i < NUM_RX_DESCRIPTORS; i++) {

        dev->rx_desc[i] = (i825xx_rx_desc_t *) (dev->rx_desc_base + (i * 16));
        dev->rx_desc[i]->address = pmm_alloc_blocks(4);
        dev->rx_desc[i]->status = 0;

    }
    

    mmio_w32(i825xx_REG_RDBAH, (uint32_t)((uint64_t) arch_vmm_v2p((uintptr_t) dev->rx_desc_base, ARCH_VMM_AREA_HEAP) >> 32));
    mmio_w32(i825xx_REG_RDBAL, (uint32_t)((uint64_t) arch_vmm_v2p((uintptr_t) dev->rx_desc_base, ARCH_VMM_AREA_HEAP) & 0xFFFFFFFF));

    kprintf("i825xx: RDBAH/RDBAL = %p:%p\n", mmio_r32(i825xx_REG_RDBAH), mmio_r32(i825xx_REG_RDBAL));
    

    mmio_w32(i825xx_REG_RDLEN, (NUM_RX_DESCRIPTORS * 16));
    mmio_w32(i825xx_REG_RDH, 0);
    mmio_w32(i825xx_REG_RDT, NUM_RX_DESCRIPTORS);
    
    dev->rx_tail = 0;
    

    mmio_w32(i825xx_REG_RCTL, (RCTL_SBP | RCTL_UPE | RCTL_MPE | RDMTS_HALF | RCTL_SECRC | 
                               RCTL_LPE | RCTL_BAM | RCTL_BSIZE_8192));

}



static void net_i825xx_tx_init(struct i825xx* dev) {
    
    DEBUG_ASSERT(dev);


    dev->tx_desc_base = (void*) arch_vmm_p2v(pmm_alloc_blocks(16), ARCH_VMM_AREA_HEAP);
    
    int i;
    for(i = 0; i < NUM_TX_DESCRIPTORS; i++) {

        dev->tx_desc[i] = (i825xx_tx_desc_t *)(dev->tx_desc_base + (i * 16));
        dev->tx_desc[i]->address = 0;
        dev->tx_desc[i]->cmd = 0;
    
    }
    

    mmio_w32(i825xx_REG_TDBAH, (uint32_t)((uint64_t) arch_vmm_v2p((uintptr_t) dev->tx_desc_base, ARCH_VMM_AREA_HEAP) >> 32));
    mmio_w32(i825xx_REG_TDBAL, (uint32_t)((uint64_t) arch_vmm_v2p((uintptr_t) dev->rx_desc_base, ARCH_VMM_AREA_HEAP) & 0xFFFFFFFF));

    kprintf("i825xx: TDBAH/TDBAL = %p:%p\n", mmio_r32(i825xx_REG_TDBAH), mmio_r32(i825xx_REG_TDBAL));
    

    mmio_w32(i825xx_REG_TDLEN, (uint32_t)(NUM_TX_DESCRIPTORS * 16));
    mmio_w32(i825xx_REG_TDH, 0);
    mmio_w32(i825xx_REG_TDT, NUM_TX_DESCRIPTORS);

    dev->tx_tail = 0;
    
    mmio_w32(i825xx_REG_TCTL, (TCTL_EN | TCTL_PSP));

}


static void net_i825xx_rx_poll(struct i825xx* dev) {

    DEBUG_ASSERT(dev);


    while((dev->rx_desc[dev->rx_tail]->status & (1 << 0))) {

        uint8_t* pkt = (void *) arch_vmm_p2v(dev->rx_desc[dev->rx_tail]->address, ARCH_VMM_AREA_HEAP);
        uint16_t pktlen = dev->rx_desc[dev->rx_tail]->length;


        do {

            if(pktlen < 60) {

                kprintf("i825xx: short packet (%u bytes)\n", pktlen);
                break;

            }

            // TODO:
            if(!(dev->rx_desc[dev->rx_tail]->status & (1 << 1))) {
                
                kprintf("i825xx: no EOP set! (len=%u, 0x%x 0x%x 0x%x)\n", 
                    pktlen, pkt[0], pkt[1], pkt[2]);
                
                break;
            }
            
            if(dev->rx_desc[dev->rx_tail]->errors) {

                kprintf("i825xx: rx errors (0x%x)\n", dev->rx_desc[dev->rx_tail]->errors);
                break;

            }


            ethif_input(&dev->device.net.interface);

        } while(0);
        

        
        dev->rx_desc[dev->rx_tail]->status = (uint16_t)(0);
        dev->rx_tail = (dev->rx_tail + 1) % NUM_RX_DESCRIPTORS;
        
        mmio_w32(i825xx_REG_RDT, dev->rx_tail);

    }

}



static int i825xx_startoutput(void* internals) {
    return 1;
}


static void i825xx_output(void* internals, void* buf, uint16_t len) {

    DEBUG_ASSERT(internals);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(len);


    struct i825xx* dev = (struct i825xx*) internals;

    kprintf("i825xx: transmitting packet (%u bytes) [h=%u, t=%u]\n", len, mmio_r32(i825xx_REG_TDH), dev->tx_tail);

    dev->tx_desc[dev->tx_tail]->address = (uint64_t) buf;
    dev->tx_desc[dev->tx_tail]->length = len;
    dev->tx_desc[dev->tx_tail]->cmd = ((1 << 3) | (3));
    

    int oldtail = dev->tx_tail;
    dev->tx_tail = (dev->tx_tail + 1) % NUM_TX_DESCRIPTORS;


    mmio_w32(i825xx_REG_TDT, dev->tx_tail);
    
    while(!(dev->tx_desc[oldtail]->sta & 0xF))
        __builtin_ia32_pause();


    kprintf("i825xx: transmit status = %p\n", (dev->tx_desc[oldtail]->sta & 0xF));

}


static void i825xx_endoutput(void* internals, uint16_t len) {

#if defined(DEBUG) && DEBUG_LEVEL >= 4
    kprintf("i825xx: INFO! [%d] sending %d bytes\n", arch_timer_getms(), len);
#endif

}


static int i825xx_startinput(void* internals) {

    DEBUG_ASSERT(internals);

    struct i825xx* dev = (struct i825xx*) internals;

    return dev->rx_desc[dev->rx_tail]->length;

}


static void i825xx_input(void* internals, void* buf, uint16_t len) {

    DEBUG_ASSERT(internals);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(len);

    struct i825xx* dev = (struct i825xx*) internals;

    
    uint8_t* pkt = (void *) arch_vmm_p2v(dev->rx_desc[dev->rx_tail]->address, ARCH_VMM_AREA_HEAP);
    uint16_t pktlen = dev->rx_desc[dev->rx_tail]->length;


    if(len > pktlen)
        len = pktlen;

    memcpy(buf, pkt, len);


    dev->rx_desc[dev->rx_tail]->address += len;
    dev->rx_desc[dev->rx_tail]->length  -= len;


    int i;
    for(i = 0; i < len; i++)
        kprintf("%c ", ((char*)buf)[i] & 0xFF);


}


static void i825xx_endinput(void* internals) {
    (void) internals;
}


static void i825xx_input_nomem(void* internals, uint16_t len) {
    kpanicf("i825xx: PANIC! no memory left for %d bytes\n", len);   
}




static void i825xx_irq(void* frame, int irq) {
    
    struct i825xx* dev;
    for(int i = 0; (dev = devices[i]); i++) {
    
        if(dev->irq != irq)
            continue;


        uint32_t icr = mmio_r32(dev->mmio_address + 0xC0) & ~(3);

        if(icr & (1 << 2)) {

            icr &= ~(1 << 2);
            mmio_w32(i825xx_REG_CTRL, (mmio_r32(i825xx_REG_CTRL) | CTRL_SLU));

            kprintf("i825xx: Link Status Change, STATUS = %p\n", mmio_r32(i825xx_REG_STATUS));
            kprintf("i825xx: PHY CONTROL = %p\n", net_i825xx_phy_read(dev, i825xx_PHYREG_PCTRL));
            kprintf("i825xx: PHY STATUS = %p\n", net_i825xx_phy_read(dev, i825xx_PHYREG_PSTATUS));
            kprintf("i825xx: PHY PSSTAT = %p\n", net_i825xx_phy_read(dev, i825xx_PHYREG_PSSTAT));
            kprintf("i825xx: PHY ANA = %p\n", net_i825xx_phy_read(dev, 4));
            kprintf("i825xx: PHY ANE = %p\n", net_i825xx_phy_read(dev, 6));
            kprintf("i825xx: PHY GCON = %p\n", net_i825xx_phy_read(dev, 9));
            kprintf("i825xx: PHY GSTATUS = %p\n", net_i825xx_phy_read(dev, 10));
            kprintf("i825xx: PHY EPSTATUS = %p\n", net_i825xx_phy_read(dev, 15));

        }


        if( icr & (1 << 6) || icr & (1 << 4) ) {
            
            icr &= ~((1 << 6) | (1 << 4));
            kprintf("i825xx: underrun (rx_head = %u, rx_tail = %u)\n", mmio_r32(i825xx_REG_RDH), dev->rx_tail);
            
            volatile int i;
            for(i = 0; i < NUM_RX_DESCRIPTORS; i++) {

                if(dev->rx_desc[i]->status)
                    kprintf("i825xx: pending descriptor (i=%u, status=0x%p4x)\n", i, dev->rx_desc[i]->status);
            
            }
            

            net_i825xx_rx_poll(dev);

        }


        if(icr & (1 << 7)) {

            icr &= ~(1 << 7);
            net_i825xx_rx_poll(dev);

        }


        if(icr)
            kprintf("i825xx: unhandled interrupt #%u received! (%p)\n", irq, icr);


        mmio_r32(dev->mmio_address + 0xC0);
        
    }

}


static void i825xx_init(void* internals, uint8_t* address, void* mcast) {
    
    DEBUG_ASSERT(internals);
    DEBUG_ASSERT(address);
    //DEBUG_ASSERT(mcast);
    
    struct i825xx* dev = (struct i825xx*) internals;


    
    mmio_w32(i825xx_REG_CTRL, (mmio_r32(i825xx_REG_CTRL) | CTRL_SLU));

    int i;
	for( i = 0; i < 128; i++ )
		mmio_w32(i825xx_REG_MTA + (i * 4), 0);

    mmio_w32(i825xx_REG_IMS, 0x1F6DC);
    mmio_r32(dev->mmio_address + 0xC0);

    net_i825xx_rx_init(dev);
    net_i825xx_tx_init(dev);
    net_i825xx_rx_enable(dev);

    kprintf("i825xx: configuratione complete\n");

}






void init(const char* args) {

    
    int pci_devices[i825xx_MAX_DEVICES];
    int pci_count = 0;


    void find_pci(uint32_t device, uint16_t venid, uint16_t devid, void* data) {
        
        if((venid == 0x8086) && (devid == 0x1209))
            pci_devices[pci_count++] = device;

        DEBUG_ASSERT(pci_count < i825xx_MAX_DEVICES);

    }



    pci_scan(&find_pci, -1, NULL);


    if(!pci_count) {
#if defined(DEBUG) && DEBUG_LEVEL >= 2
        kprintf("i825xx: ERROR! pci device not found!\n");
#endif
        return;
    }


    int i;
    for(i = 0; i < pci_count; i++) {

    
        struct i825xx* eth = (struct i825xx*) kcalloc(1, sizeof(struct i825xx), GFP_KERNEL);

        eth->pci = pci_devices[i];


        uint32_t cmd;
        
        if(!((cmd = pci_read(eth->pci, PCI_COMMAND, 4)) & (1 << 2)))
            pci_write(eth->pci, PCI_COMMAND, 4, cmd | (1 << 2));


        eth->irq          = pci_read(eth->pci, PCI_INTERRUPT_LINE, 1);
        eth->mmio_address = pci_read(eth->pci, PCI_BAR0, 4) & 0xFFFFFFF0;

        spinlock_init(&eth->lock);


        arch_vmm_map (
            &core->bsp.address_space,
            eth->mmio_address,
            eth->mmio_address,
            PML1_PAGESIZE << 4,
            
            ARCH_VMM_MAP_RDWR       |
            ARCH_VMM_MAP_UNCACHED   |
            ARCH_VMM_MAP_NOEXEC     |
            ARCH_VMM_MAP_FIXED
        );



        strcpy(eth->device.name, "i825xx");
        strcpy(eth->device.description, "i825xx Network Adapter");

        eth->device.major = 144;
        eth->device.minor = i;

        eth->device.type   = DEVICE_TYPE_NETWORK;
        eth->device.status = DEVICE_STATUS_UNKNOWN;



        int j;
        for(j = 0; j < 3; j++) {

            eth->device.net.address[j * 2]     = net_i825xx_eeprom_read(eth, j) & 0xFF;
            eth->device.net.address[j * 2 + 1] = net_i825xx_eeprom_read(eth, j) >> 8;

        }

#if defined(DEBUG) && DEBUG_LEVEL >= 2
        kprintf("i825xx: index(%d) irq(%d) mem(%p) mac(%x:%x:%x:%x:%x:%x)\n", i, eth->irq, eth->mmio_address,
            eth->device.net.address[0] & 0xFF,
            eth->device.net.address[1] & 0xFF,
            eth->device.net.address[2] & 0xFF,
            eth->device.net.address[3] & 0xFF,
            eth->device.net.address[4] & 0xFF,
            eth->device.net.address[5] & 0xFF
        );
#endif


        eth->device.net.low_level_init         = i825xx_init;
        eth->device.net.low_level_startoutput  = i825xx_startoutput;
        eth->device.net.low_level_output       = i825xx_output;
        eth->device.net.low_level_endoutput    = i825xx_endoutput;
        eth->device.net.low_level_startinput   = i825xx_startinput;
        eth->device.net.low_level_input        = i825xx_input;
        eth->device.net.low_level_endinput     = i825xx_endinput;
        eth->device.net.low_level_input_nomem  = i825xx_input_nomem;


        IP4_ADDR(&eth->device.net.ip, 10, 0, 2, 15 + i);
        IP4_ADDR(&eth->device.net.nm, 255, 255, 255, 0);
        IP4_ADDR(&eth->device.net.gw, 10, 0, 2, 2);

        eth->device.net.interface.state = &eth->device;
        eth->device.net.internals = eth;


        if(!netif_add( &eth->device.net.interface, 
                       &eth->device.net.ip,
                       &eth->device.net.nm,
                       &eth->device.net.gw, &eth->device, ethif_init, ethernet_input)) {

            kpanicf("i825xx: PANIC! netif_add() failed\n");

        }


        arch_intr_map_irq(eth->irq, i825xx_irq);

        netif_set_default(&eth->device.net.interface);
        netif_set_up(&eth->device.net.interface);



        device_mkdev(&eth->device, 0666);

        devices[i] = eth;
        devices[i + 1] = NULL;

    }

    
}


void dnit(void) {
    
}