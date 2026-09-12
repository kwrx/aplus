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
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <aplus/smp.h>
#include <aplus/utils/list.h>
#include <aplus/vfs.h>

#include <dev/interface.h>
#include <dev/network.h>
#include <dev/pci.h>

#include <arch/x86/cpu.h>



MODULE_NAME("network/e1000");
MODULE_DEPS("dev/interface,dev/network,dev/pci");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



#define E1000_VENDOR_ID         0x8086
#define E1000_DEVICE_ID         0x100E
#define E1000_DEVICE_ID_I217    0x153A
#define E1000_DEVICE_ID_82577LM 0x10EA


#define REG_CTRL       0x0000
#define REG_STATUS     0x0008
#define REG_EEPROM     0x0014
#define REG_CTRL_EXT   0x0018
#define REG_IMASK      0x00D0
#define REG_RCTRL      0x0100
#define REG_RXDESCLO   0x2800
#define REG_RXDESCHI   0x2804
#define REG_RXDESCLEN  0x2808
#define REG_RXDESCHEAD 0x2810
#define REG_RXDESCTAIL 0x2818

#define REG_TCTRL      0x0400
#define REG_TXDESCLO   0x3800
#define REG_TXDESCHI   0x3804
#define REG_TXDESCLEN  0x3808
#define REG_TXDESCHEAD 0x3810
#define REG_TXDESCTAIL 0x3818


#define REG_RDTR   0x2820 // RX Delay Timer Register
#define REG_RXDCTL 0x3828 // RX Descriptor Control
#define REG_RADV   0x282C // RX Int. Absolute Delay Timer
#define REG_RSRPD  0x2C00 // RX Small Packet Detect Interrupt



#define E1000_TX_TIMEOUT 1000000 // spins to wait for a transmit to be reported done

#define REG_TIPG  0x0410 // Transmit Inter Packet Gap
#define ECTRL_SLU 0x40   // set link up


#define RCTL_EN            (1 << 1)  // Receiver Enable
#define RCTL_SBP           (1 << 2)  // Store Bad Packets
#define RCTL_UPE           (1 << 3)  // Unicast Promiscuous Enabled
#define RCTL_MPE           (1 << 4)  // Multicast Promiscuous Enabled
#define RCTL_LPE           (1 << 5)  // Long Packet Reception Enable
#define RCTL_LBM_NONE      (0 << 6)  // No Loopback
#define RCTL_LBM_PHY       (3 << 6)  // PHY or external SerDesc loopback
#define RTCL_RDMTS_HALF    (0 << 8)  // Free Buffer Threshold is 1/2 of RDLEN
#define RTCL_RDMTS_QUARTER (1 << 8)  // Free Buffer Threshold is 1/4 of RDLEN
#define RTCL_RDMTS_EIGHTH  (2 << 8)  // Free Buffer Threshold is 1/8 of RDLEN
#define RCTL_MO_36         (0 << 12) // Multicast Offset - bits 47:36
#define RCTL_MO_35         (1 << 12) // Multicast Offset - bits 46:35
#define RCTL_MO_34         (2 << 12) // Multicast Offset - bits 45:34
#define RCTL_MO_32         (3 << 12) // Multicast Offset - bits 43:32
#define RCTL_BAM           (1 << 15) // Broadcast Accept Mode
#define RCTL_VFE           (1 << 18) // VLAN Filter Enable
#define RCTL_CFIEN         (1 << 19) // Canonical Form Indicator Enable
#define RCTL_CFI           (1 << 20) // Canonical Form Indicator Bit Value
#define RCTL_DPF           (1 << 22) // Discard Pause Frames
#define RCTL_PMCF          (1 << 23) // Pass MAC Control Frames
#define RCTL_SECRC         (1 << 26) // Strip Ethernet CRC

// Buffer Sizes
#define RCTL_BSIZE_256   (3 << 16)
#define RCTL_BSIZE_512   (2 << 16)
#define RCTL_BSIZE_1024  (1 << 16)
#define RCTL_BSIZE_2048  (0 << 16)
#define RCTL_BSIZE_4096  ((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192  ((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384 ((1 << 16) | (1 << 25))


// Transmit Command

#define CMD_EOP  (1 << 0) // End of Packet
#define CMD_IFCS (1 << 1) // Insert FCS
#define CMD_IC   (1 << 2) // Insert Checksum
#define CMD_RS   (1 << 3) // Report Status
#define CMD_RPS  (1 << 4) // Report Packet Sent
#define CMD_VLE  (1 << 6) // VLAN Packet Enable
#define CMD_IDE  (1 << 7) // Interrupt Delay Enable


// TCTL Register

#define TCTL_EN         (1 << 1)  // Transmit Enable
#define TCTL_PSP        (1 << 3)  // Pad Short Packets
#define TCTL_CT_SHIFT   4         // Collision Threshold
#define TCTL_COLD_SHIFT 12        // Collision Distance
#define TCTL_SWXOFF     (1 << 22) // Software XOFF Transmission
#define TCTL_RTLC       (1 << 24) // Re-transmit on Late Collision

#define TSTA_DD (1 << 0) // Descriptor Done
#define TSTA_EC (1 << 1) // Excess Collisions
#define TSTA_LC (1 << 2) // Late Collision
#define LSTA_TU (1 << 3) // Transmit Underrun


#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 8

#define E1000_MAX_DEVICES 32



struct e1000_rx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint16_t checksum;
    volatile uint8_t status;
    volatile uint8_t errors;
    volatile uint16_t special;
} __packed;

struct e1000_tx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint8_t cso;
    volatile uint8_t cmd;
    volatile uint8_t status;
    volatile uint8_t css;
    volatile uint16_t special;
} __packed;



struct e1000 {

    uint32_t pci;
    uint8_t irq;
    uint16_t io;
    uintptr_t mem;
    uintptr_t vmem;

    uintptr_t rx_desc[E1000_NUM_RX_DESC];
    uintptr_t tx_desc[E1000_NUM_TX_DESC];

    uint16_t rx_cur;
    uint16_t tx_cur;

    uint16_t rx_size;
    uint16_t rx_offset;

    uintptr_t cache;

    spinlock_t lock;
    device_t device;
};

static struct e1000* devices[E1000_MAX_DEVICES];

static uint32_t pci_devices[E1000_MAX_DEVICES];
static uint32_t pci_count = 0;



/*
 * Registers are reached through the memory-mapped window at BAR0. The card also offers an
 * indirect two-port path through BAR1, but that one is optional on several of the device IDs
 * matched here, and the address it needs was being taken from the wrong BAR anyway. The
 * offset is allowed to be zero (REG_CTRL), so it is not asserted.
 */
static inline void wrcmd(struct e1000* dev, uint16_t address, uint32_t value) {

    DEBUG_ASSERT(dev);
    DEBUG_ASSERT(dev->vmem);

    mmio_w32(dev->vmem + address, value);
}


static inline uint32_t rdcmd(struct e1000* dev, uint16_t address) {

    DEBUG_ASSERT(dev);
    DEBUG_ASSERT(dev->vmem);

    return mmio_r32(dev->vmem + address);
}



static int e1000_startoutput(void* internals) {

    DEBUG_ASSERT(internals);

    return 1;
}


static void e1000_output(void* internals, void* buf, uint16_t len) {

    DEBUG_ASSERT(internals);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(len);


    struct e1000* dev = (struct e1000*)internals;

    memcpy((void*)(arch_vmm_p2v(dev->cache, ARCH_VMM_AREA_HEAP)), (void*)(buf), (size_t)len);


    uint8_t j = dev->tx_cur;

    ((struct e1000_tx_desc*)arch_vmm_p2v(dev->tx_desc[j], ARCH_VMM_AREA_HEAP))->addr   = (uint64_t)dev->cache;
    ((struct e1000_tx_desc*)arch_vmm_p2v(dev->tx_desc[j], ARCH_VMM_AREA_HEAP))->length = len;
    ((struct e1000_tx_desc*)arch_vmm_p2v(dev->tx_desc[j], ARCH_VMM_AREA_HEAP))->cmd    = CMD_EOP | CMD_IFCS | CMD_RS;
    ((struct e1000_tx_desc*)arch_vmm_p2v(dev->tx_desc[j], ARCH_VMM_AREA_HEAP))->status = 0;

    dev->tx_cur = (dev->tx_cur + 1) % E1000_NUM_TX_DESC;


    wrcmd(dev, REG_TXDESCTAIL, dev->tx_cur);


    //? Wait for the card to report the descriptor done, but not forever: this runs with the
    //? caller's lock held and on the boot path, so a card that never answers used to hang the
    //? machine outright rather than drop a frame.
    for (size_t timeout = 0; timeout < E1000_TX_TIMEOUT; timeout++) {

        if (((struct e1000_tx_desc*)arch_vmm_p2v(dev->tx_desc[j], ARCH_VMM_AREA_HEAP))->status & 0xFF)
            return;

        __builtin_ia32_pause();
    }

#if DEBUG_LEVEL_ERROR
    kprintf("e1000: ERROR! transmit timed out on descriptor %d, frame dropped\n", j);
#endif
}


static void e1000_endoutput(void* internals, uint16_t len) {

    DEBUG_ASSERT(internals);
    DEBUG_ASSERT(len);
}


static int e1000_startinput(void* internals) {

    DEBUG_ASSERT(internals);

    struct e1000* dev = (struct e1000*)internals;


    if (!(((struct e1000_rx_desc*)arch_vmm_p2v(dev->rx_desc[dev->rx_cur], ARCH_VMM_AREA_HEAP))->status & 0x1))
        return 0;



    uint16_t size = ((struct e1000_rx_desc*)arch_vmm_p2v(dev->rx_desc[dev->rx_cur], ARCH_VMM_AREA_HEAP))->length;


#if DEBUG_LEVEL_TRACE
    kprintf("e1000: device received %d bytes from %d at %ldms\n", size, dev->rx_cur, arch_timer_generic_getms());
#endif


    //? The frame is handed up one pbuf at a time, so how far through it we are has to survive
    //? between calls; the descriptor is only released once, in endinput().
    dev->rx_size   = size;
    dev->rx_offset = 0;

    return size;
}



static void e1000_input(void* internals, void* buf, uint16_t len) {

    DEBUG_ASSERT(internals);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(len);


    struct e1000* dev = (struct e1000*)internals;

    DEBUG_ASSERT(dev->rx_offset < dev->rx_size);


    if (dev->rx_offset + len > dev->rx_size)
        len = dev->rx_size - dev->rx_offset;


    //? The address in the descriptor is the one the card writes to, a physical one. Reading it
    //? from the CPU has to go through the mapping, which this did not do.
    uintptr_t src = arch_vmm_p2v((uintptr_t)((struct e1000_rx_desc*)arch_vmm_p2v(dev->rx_desc[dev->rx_cur], ARCH_VMM_AREA_HEAP))->addr, ARCH_VMM_AREA_HEAP);

    memcpy(buf, (const void*)(src + dev->rx_offset), (size_t)len);

    dev->rx_offset += len;
}


static void e1000_endinput(void* internals) {

    DEBUG_ASSERT(internals);

    struct e1000* dev = (struct e1000*)internals;


    uint16_t j = dev->rx_cur;

    ((struct e1000_rx_desc*)arch_vmm_p2v(dev->rx_desc[j], ARCH_VMM_AREA_HEAP))->status = 0;

    dev->rx_cur = (dev->rx_cur + 1) % E1000_NUM_RX_DESC;

    //? The tail marks the last descriptor handed back to the card.
    wrcmd(dev, REG_RXDESCTAIL, j);

    dev->rx_size   = 0;
    dev->rx_offset = 0;
}


static void e1000_input_nomem(void* internals, uint16_t len) {
    kpanicf("e1000: PANIC! no memory left for %d bytes\n", len);
}



static void e1000_irq(pcidev_t device, uint8_t irq, struct e1000* dev) {

    DEBUG_ASSERT(dev);
    DEBUG_ASSERT(dev->irq == irq);


    //? Reading ICR is what acknowledges the interrupt. The causes are independent bits, so they
    //? are tested separately: as an if/else chain a link-status change on the same interrupt
    //? would discard the frame that came with it.
    uint32_t s = rdcmd(dev, 0xC0);

#if DEBUG_LEVEL_TRACE
    if (s & 0x04)
        kprintf("e1000: link status changed\n");
#endif

    if (s & (0x80 | 0x10))
        ethif_input(&dev->device.net.interface);
}


static void e1000_init(void* internals, uint8_t* address, void* mcast) {

    DEBUG_ASSERT(internals);
    DEBUG_ASSERT(address);
    // DEBUG_ASSERT(mcast);


    struct e1000* dev = (struct e1000*)internals;



    wrcmd(dev, REG_IMASK, 0x1F6DC);
    wrcmd(dev, REG_IMASK, 0xFF & ~4);
    rdcmd(dev, 0xC0);


    uintptr_t ptr = pmm_alloc_blocks(16);

    int j;
    for (j = 0; j < E1000_NUM_RX_DESC; j++) {

        dev->rx_desc[j] = ptr + (j * 16);

        ((struct e1000_rx_desc*)arch_vmm_p2v(dev->rx_desc[j], ARCH_VMM_AREA_HEAP))->addr   = pmm_alloc_blocks(4);
        ((struct e1000_rx_desc*)arch_vmm_p2v(dev->rx_desc[j], ARCH_VMM_AREA_HEAP))->status = 0;
    }

    //? Low half to the low register, high half to the high one. These were the other way
    //? round, which pointed the card at a ring address with its two halves exchanged.
    wrcmd(dev, REG_RXDESCLO, (uint32_t)((uint64_t)ptr & 0xFFFFFFFF));
    wrcmd(dev, REG_RXDESCHI, (uint32_t)((uint64_t)ptr >> 32));

    wrcmd(dev, REG_RXDESCLEN, E1000_NUM_RX_DESC * 16);
    wrcmd(dev, REG_RXDESCHEAD, 0);
    wrcmd(dev, REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);

    wrcmd(dev, REG_RCTRL, RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE | RCTL_LBM_NONE | RTCL_RDMTS_HALF | RCTL_BAM | RCTL_SECRC | RCTL_BSIZE_8192);



    ptr = pmm_alloc_blocks(16);

    for (j = 0; j < E1000_NUM_TX_DESC; j++) {

        dev->tx_desc[j] = ptr + (j * 16);

        ((struct e1000_tx_desc*)arch_vmm_p2v(dev->tx_desc[j], ARCH_VMM_AREA_HEAP))->addr   = 0;
        ((struct e1000_tx_desc*)arch_vmm_p2v(dev->tx_desc[j], ARCH_VMM_AREA_HEAP))->cmd    = 0;
        ((struct e1000_tx_desc*)arch_vmm_p2v(dev->tx_desc[j], ARCH_VMM_AREA_HEAP))->status = TSTA_DD;
    }

    wrcmd(dev, REG_TXDESCLO, (uint32_t)((uint64_t)ptr & 0xFFFFFFFF));
    wrcmd(dev, REG_TXDESCHI, (uint32_t)((uint64_t)ptr >> 32));

    wrcmd(dev, REG_TXDESCLEN, E1000_NUM_TX_DESC * 16);
    wrcmd(dev, REG_TXDESCHEAD, 0);
    wrcmd(dev, REG_TXDESCTAIL, 0);

    wrcmd(dev, REG_TCTRL, TCTL_EN | TCTL_PSP | (15 << TCTL_CT_SHIFT) | (64 << TCTL_COLD_SHIFT) | TCTL_RTLC);


    dev->tx_cur = dev->rx_cur = 0;

    dev->cache = pmm_alloc_blocks(16);


}



static void find_pci(uint32_t device, uint16_t venid, uint16_t devid, void* data) {

    if (pci_count >= E1000_MAX_DEVICES) {
        return;
    }

    if (venid != E1000_VENDOR_ID) {
        return;
    }

    if (devid != E1000_DEVICE_ID && devid != 0x1004 && devid != 0x100F && devid != 0x10D3 && devid != E1000_DEVICE_ID_82577LM && devid != E1000_DEVICE_ID_I217) {
        return;
    }


    pci_devices[pci_count++] = device;
}


void init(const char* args) {

    if (strstr(core->boot.cmdline, "network=off"))
        return;


    pci_scan(&find_pci, -1, NULL);

    if (!pci_count) {
#if DEBUG_LEVEL_ERROR
        kprintf("e1000: ERROR! pci device not found!\n");
#endif
        return;
    }



    for (size_t i = 0; i < pci_count; i++) {

        struct e1000* eth = (struct e1000*)kcalloc(1, sizeof(struct e1000), GFP_KERNEL);

        eth->pci = pci_devices[i];
        eth->irq = pci_read(eth->pci, PCI_INTERRUPT_LINE, 1);

        //? BAR0 is the memory window and BAR1 the I/O ports; they were read the other way
        //? round, so every register access was an outl() to a port number made out of a
        //? physical address. Both BARs also carry type bits in the low bits, which have to be
        //? masked off before the value is an address at all.
        eth->mem = pci_read(eth->pci, PCI_BAR0, 4) & PCI_BAR_MM_MASK;
        eth->io  = pci_read(eth->pci, PCI_BAR1, 4) & PCI_BAR_IO_MASK;

        pci_enable_pio(eth->pci);
        pci_enable_mmio(eth->pci);
        pci_enable_bus_mastering(eth->pci);


        //? Device memory is not in the direct map, so the window has to be mapped before it is
        //? touched. Uncached and write-through: a register write parked in a cache line never
        //? reaches the card.
        uintptr_t size = pci_bar_size(eth->pci, PCI_BAR0, 4);

        PANIC_ASSERT(ARCH_VMM_MAP_FAILED != arch_vmm_map(&core->bsp.address_space, eth->mem, eth->mem, size, ARCH_VMM_MAP_NOEXEC | ARCH_VMM_MAP_FIXED | ARCH_VMM_MAP_RDWR | ARCH_VMM_MAP_UNCACHED | ARCH_VMM_MAP_WRITE_THROUGH));

        eth->vmem = eth->mem;


        spinlock_init(&eth->lock);



        DEBUG_ASSERT(sizeof("e1000") < sizeof(eth->device.name));
        DEBUG_ASSERT(sizeof("Intel(R) PRO/1000 Network Connection") < sizeof(eth->device.description));

        strcpy(eth->device.name, "e1000");
        strcpy(eth->device.description, "Intel(R) PRO/1000 Network Connection");


        eth->device.major  = 144;
        eth->device.minor  = i;
        eth->device.type   = DEVICE_TYPE_NETWORK;
        eth->device.status = DEVICE_STATUS_UNKNOWN;


        for (size_t j = 0; j < 128; j++) {
            wrcmd(eth, 0x5200 + (j * 4), 0);
        }

        for (size_t j = 0; j < 6; j++) {
            eth->device.net.address[j] = mmio_r8(eth->vmem + 0x5400 + j);
        }



        eth->device.net.low_level_init        = e1000_init;
        eth->device.net.low_level_startoutput = e1000_startoutput;
        eth->device.net.low_level_output      = e1000_output;
        eth->device.net.low_level_endoutput   = e1000_endoutput;
        eth->device.net.low_level_startinput  = e1000_startinput;
        eth->device.net.low_level_input       = e1000_input;
        eth->device.net.low_level_endinput    = e1000_endinput;
        eth->device.net.low_level_input_nomem = e1000_input_nomem;



        IP4_ADDR(&eth->device.net.ip, 10, 0, 2, 15 + i);
        IP4_ADDR(&eth->device.net.nm, 255, 255, 255, 0);
        IP4_ADDR(&eth->device.net.gw, 10, 0, 2, 2);

        eth->device.net.interface.state = &eth->device;
        eth->device.net.internals       = eth;


        if (!netif_add(&eth->device.net.interface, &eth->device.net.ip, &eth->device.net.nm, &eth->device.net.gw, &eth->device, ethif_init, ethernet_input)) {

            kpanicf("e1000: PANIC! netif_add() failed\n");
        }


        //? Only now, with the interface registered, is it safe to take interrupts: the handler
        //? reaches ethif_input() through this netif. Unmasking earlier -- and bringing the
        //? interface up from inside low_level_init(), while netif_add() was still running --
        //? was what wedged this driver at boot.
        if (eth->irq != PCI_INTERRUPT_LINE_NONE) {

            pci_intx_map_irq(eth->pci, eth->irq, (pci_irq_handler_t)e1000_irq, (pci_irq_data_t)eth);
            pci_intx_unmask(eth->pci);
        }


        netif_set_default(&eth->device.net.interface);
        netif_set_up(&eth->device.net.interface);


        device_mkdev(&eth->device, 0666);

        devices[i]     = eth;
        devices[i + 1] = NULL;
    }
}

void dnit(void) {
}
