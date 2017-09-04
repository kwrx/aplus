#include <aplus.h>
#include <aplus/base.h>
#include <aplus/module.h>
#include <aplus/mm.h>
#include <aplus/mmio.h>
#include <aplus/vfs.h>
#include <aplus/debug.h>
#include <aplus/network.h>
#include <aplus/intr.h>
#include <libc.h>


MODULE_NAME("pc/network/rtl8139");
MODULE_DEPS("arch/x86");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");

#if defined(__i386__)
#include <arch/i386/i386.h>
#include <arch/i386/pci.h>


#define RTL_PORT_TXSTAT             0x10
#define RTL_PORT_TXBUF              0x20
#define RTL_PORT_RBSTART            0x30
#define RTL_PORT_CMD                0x37
#define RTL_PORT_RXPTR              0x38
#define RTL_PORT_IMR                0x3C
#define RTL_PORT_ISR                0x3E
#define RTL_PORT_TCR                0x40
#define RTL_PORT_RCR                0x44
#define RTL_PORT_RXMISS             0x4C
#define RTL_PORT_CONFIG             0x52



struct rtl8139 {
    uint32_t pci;
    uint16_t io;
    uintptr_t mem;
    uint8_t irq;

    uint8_t* rxbuf;
    uintptr_t rxbufp;

    uint8_t* txbuf[5];
    uintptr_t txbufp[5];

    int rxcur;
    int txcur;
    int txcurd;

    struct netif* netif;

    uint8_t cache[0x4000];
    uint16_t size;
    uint16_t offset;

    spinlock_t lock;
};


static int rtl8139_irqno = 0;


int rtl8139_startoutput(void* internals) {
    struct rtl8139* dev = (struct rtl8139*) internals;
    if(unlikely(!dev)) {
        kprintf(ERROR "rtl8139: rtl8139_startoutput() invalid args\n");
        return 0;
    }

    dev->offset = 0;
    dev->size = 0;

    return 1;
}

void rtl8139_output(void* internals, void* buf, uint16_t len) {
    struct rtl8139* dev = (struct rtl8139*) internals;
    if(unlikely(!dev)) {
        kprintf(ERROR "rtl8139: rtl8139_output() invalid args\n");
        return;
    }

    memcpy((void*) ((uintptr_t) dev->cache + dev->offset), buf, len);
    dev->offset += len;
}

void rtl8139_endoutput(void* internals, uint16_t len) {
    struct rtl8139* dev = (struct rtl8139*) internals;
    if(unlikely(!dev)) {
        kprintf(ERROR "rtl8139: rtl8139_endoutput() invalid args\n");
        return;
    }


    memcpy(dev->txbuf[dev->txcur], dev->cache, len);
    outl(dev->io + RTL_PORT_TXBUF + 4 * dev->txcur, dev->txbufp[dev->txcur]);
    outl(dev->io + RTL_PORT_TXSTAT + 4 * dev->txcur, len);

    while(!(inl(dev->io + RTL_PORT_TXSTAT + 4 * dev->txcur) & 0x8000))
        ;

    dev->txcur++;
    if(dev->txcur == 4)
        dev->txcur = 0;

    kprintf(INFO "rtl8139: sending %d bytes\n", len);
}

void rtl8139_endinput(void* internals);
int rtl8139_startinput(void* internals) {
    struct rtl8139* dev = (struct rtl8139*) internals;
    if(unlikely(!dev)) {
        kprintf(ERROR "rtl8139: rtl8139_startinput() invalid args\n");
        return 0;
    }

    uint32_t s = ((volatile uint32_t*) ((uintptr_t) dev->rxbuf + (dev->rxcur % 0x2000))) [0];
    if((s & (0x0020 | 0x0010 | 0x0004 | 0x0002)) || !(s & 1)) {
        kprintf(WARN "rtl8139: RX error! lost %d bytes (status: %08x, RX: %p)\n", s >> 16, s, dev->rxcur);
        return 0;
    }
      
    size_t size = (s >> 16) & 0xFFFF;
    
    void* pkt = &((uint32_t*) ((uintptr_t) dev->rxbuf + (dev->rxcur % 0x2000))) [1];
    uintptr_t pktend = (uintptr_t) dev->rxbuf + size;

    if(pktend > (uintptr_t) dev->rxbuf + 0x2000) {
        size_t s = ((uintptr_t) dev->rxbuf + 0x2000) - (uintptr_t) pkt;

        memcpy(dev->cache, pkt, s);
        memcpy((void*) ((uintptr_t) dev->cache + s), dev->rxbuf, size - s);
    } else
        memcpy(dev->cache, pkt, size);

    dev->size = size;
    dev->offset = 0;


    kprintf(WARN "rtl8139: reiceved %d bytes (status: %08x)\n", s >> 16, s);
    return size;
}

void rtl8139_input(void* internals, void* buf, uint16_t len) {
    struct rtl8139* dev = (struct rtl8139*) internals;
    if(unlikely(!dev)) {
        kprintf(ERROR "rtl8139: rtl8139_startinput() invalid args\n");
        return;
    }

    if(!(dev->offset < dev->size))
        return;

    if(dev->size - dev->offset < len)
        len = dev->size - dev->offset;

    if(dev->offset < dev->size)
        memcpy(buf, &dev->cache[dev->offset], len);

    dev->offset += len;
}

void rtl8139_endinput(void* internals) {
    struct rtl8139* dev = (struct rtl8139*) internals;
    if(unlikely(!dev)) {
        kprintf(ERROR "rtl8139: rtl8139_endinput() invalid args\n");
        return;
    }

    dev->rxcur = (dev->rxcur + dev->size + 4 + 3) & ~3;
    dev->rxcur %= 0x2000;
    outw(dev->io + RTL_PORT_RXPTR, dev->rxcur - 16);

    dev->size =
    dev->offset = 0;
}

void rtl8139_input_nomem(void* internals, uint16_t len) {
    kprintf(ERROR "rtl8139: no memory left for %d bytes\n", len);   
}

void rtl8139_irq(void* context) {
    struct rtl8139* dev = (struct rtl8139*) irq_get_data(rtl8139_irqno);
    if(unlikely(!dev)) {
        kprintf(ERROR "rtl8139: invalid irq#%d\n", rtl8139_irqno);
        return;
    }


    uint16_t s;
    if(!(s = inw(dev->io + RTL_PORT_ISR)))
        return;

    outw(dev->io + RTL_PORT_ISR, s);
    irq_ack(dev->irq);


    if(s & 1 || s & 2)
        while((inb(dev->io + RTL_PORT_CMD) & 1) == 0)
            ethif_input(dev->netif);
        

    if(s & 8 || s & 4) {
        inl(dev->io + RTL_PORT_TXSTAT + 4 * dev->txcurd);

        dev->txcurd++;
        if(dev->txcurd == 5)
            dev->txcurd = 0;
    }

    outw(dev->io + RTL_PORT_ISR, s);
}

void rtl8139_init(void* internals, uint8_t* address, void* mcast) {
    struct rtl8139* dev = (struct rtl8139*) internals;

    outb(dev->io + RTL_PORT_CONFIG, 0);
    outb(dev->io + RTL_PORT_CMD, 0x10);
    
    while((inb(dev->io + 0x37) & 0x10) != 0)
        ;

    int i;
    for(i = 0; i < 5; i++) {
        dev->txbuf[i] = (void*) kvalloc(0x1000, GFP_KERNEL);
        dev->txbufp[i] = V2P(dev->txbuf[i]);

        int j;
        for(j = 0; j < 60; j++)
            dev->txbuf[i][j] = 0xF0;
    }

    dev->rxbuf = (uint8_t*) kvalloc(0x4000, GFP_KERNEL);
    dev->rxbufp = V2P(dev->rxbuf);
    memset(dev->rxbuf, 0, 0x4000);

    outl(dev->io + RTL_PORT_RBSTART, dev->rxbufp);
    
    outw(dev->io + RTL_PORT_IMR, 
        0x8000      |
        0x4000      |
        0x40        |
        0x20        |
        0x10        |
        0x08        |
        0x04        |
        0x02        |
        0x01
    );

    outl(dev->io + RTL_PORT_TCR, 0x00);
    outl(dev->io + RTL_PORT_RCR, 0x0f);
    outb(dev->io + RTL_PORT_CMD, 0x08 | 0x04);
    outl(dev->io + RTL_PORT_RXMISS, 0);

    rtl8139_irqno = dev->irq;   /* FIXME: fix current_irq */

    irq_enable(dev->irq, rtl8139_irq);
    irq_set_data(dev->irq, dev);
}


int init(void) {
    
    void find_pci(uint32_t device, uint16_t venid, uint16_t devid, void* data) {
        if((venid == 0x10EC) && (devid == 0x8139))
            *((uint32_t*) data) = device;
    }

    int pci = 0;
    pci_scan(&find_pci, -1, &pci);
    if(!pci) {
        kprintf(ERROR "rtl8139: pci device not found!\n");
        return E_ERR;
    }

    struct rtl8139* dev = (struct rtl8139*) kmalloc(sizeof(struct rtl8139), GFP_KERNEL);
    struct ethif* eth = (struct ethif*) kmalloc(sizeof(struct ethif), GFP_KERNEL);

    memset(dev, 0, sizeof(struct rtl8139));
    memset(eth, 0, sizeof(struct ethif));


    eth->internals = (void*) dev;
    dev->pci = pci;


    uint16_t cmd = pci_read_field(dev->pci, PCI_COMMAND, 4);
    if(!(cmd & (1 << 2))) 
        pci_write_field(dev->pci, PCI_COMMAND, 4, cmd | (1 << 2));


    dev->irq = pci_read_field(dev->pci, PCI_INTERRUPT_LINE, 1);
    dev->io = pci_read_field(dev->pci, PCI_BAR0, 4);
    dev->mem = pci_read_field(dev->pci, PCI_BAR1, 4);

    if(dev->io & 1)
        dev->io &= 0xFFFFFFFC;
    else
        kprintf(WARN "rtl8139: invalid registers %d\n", dev->io);


    spinlock_init(&dev->lock);

    kprintf(LOG "rtl8139: irq: %d, io: %p, mem: %p\n", dev->irq, dev->io, dev->mem);

    int i;
    for(i = 0; i < 6; i++)
        eth->address[i] = inb(dev->io + i);


    eth->low_level_init = rtl8139_init;
    eth->low_level_startoutput = rtl8139_startoutput;
    eth->low_level_output = rtl8139_output;
    eth->low_level_endoutput = rtl8139_endoutput;
    eth->low_level_startinput = rtl8139_startinput;
    eth->low_level_input = rtl8139_input;
    eth->low_level_endinput = rtl8139_endinput;
    eth->low_level_input_nomem = rtl8139_input_nomem;


    IP4_ADDR(&eth->ip, 10, 0, 2, 15);
    IP4_ADDR(&eth->nm, 255, 255, 255, 0);
    IP4_ADDR(&eth->gw, 10, 0, 2, 2);

    struct netif* netif = (struct netif*) kmalloc(sizeof(struct netif), GFP_KERNEL);
    dev->netif = netif;

    if(!netif_add(netif, &eth->ip, &eth->nm, &eth->gw, eth, ethif_init, ethernet_input)) {
        kprintf(ERROR "rtl8139: netif_add() failed\n");

        kfree(dev);
        kfree(eth);
        kfree(netif);
        return E_ERR;
    }

    netif_set_default(netif);
    netif_set_up(netif);

    return E_OK;
}



#else
int init(void) {
    return E_ERR;
}
#endif


int dnit(void) {
    return E_OK;
}