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


MODULE_NAME("pc/network/pcnet");
MODULE_DEPS("arch/x86");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");

#if defined(__i386__)
#include <arch/i386/i386.h>
#include <arch/i386/pci.h>


#define PCNET_DE_SIZE           16
#define PCNET_BUFSIZE           1548
#define PCNET_RX_COUNT          32
#define PCNET_TX_COUNT          8


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

    uintptr_t bufp;
    uintptr_t rxdesp;
    uintptr_t txdesp;
    uintptr_t rxbufp;
    uintptr_t txbufp;

    int rxid;
    int txid;

    struct netif* netif;

    uint8_t cache[0x4000];
    uint16_t size;
    uint16_t offset;

    spinlock_t lock;
};

static int pcnet_irqno = 0;



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

static inline uintptr_t pcnet_v2p(struct pcnet* dev, uintptr_t vaddr) {
    return ((uintptr_t) vaddr - (uintptr_t) dev->buf) + (uintptr_t) dev->bufp;
}

static inline int d_owns(uint8_t* t, int idx) {
    return (t[PCNET_DE_SIZE * idx + 7] & 0x80) == 0;
}






int pcnet_startoutput(void* internals) {
    struct pcnet* dev = (struct pcnet*) internals;
    if(unlikely(!dev)) {
        kprintf(ERROR "pcnet: pcnet_startoutput() invalid args\n");
        return 0;
    }

    if(!d_owns((uint8_t*) dev->txdes, dev->txid)) {
        kprintf(ERROR "pcnet: no tx descriptor available!\n");
        return 0;
    }

    dev->offset = 0;
    dev->size = 0;

    return 1;
}

void pcnet_output(void* internals, void* buf, uint16_t len) {
    struct pcnet* dev = (struct pcnet*) internals;
    if(unlikely(!dev)) {
        kprintf(ERROR "pcnet: pcnet_output() invalid args\n");
        return;
    }

    memcpy((void*) ((uintptr_t) dev->cache + dev->offset), buf, len);
    dev->offset += len;
}

void pcnet_endoutput(void* internals, uint16_t len) {
    struct pcnet* dev = (struct pcnet*) internals;
    if(unlikely(!dev)) {
        kprintf(ERROR "pcnet: pcnet_endoutput() invalid args\n");
        return;
    }


    memcpy((void*) (dev->txbuf + dev->txid * PCNET_BUFSIZE), dev->cache, len);
    *((uint8_t*) dev->txdes + (dev->txid * PCNET_DE_SIZE + 7)) |= 0x03;


    uint16_t b = (uint16_t) (-len);
    b &= 0x0FFF;
    b |= 0xF000;

    *(uint16_t*) (dev->txdes + (dev->txid * PCNET_DE_SIZE + 4)) = b;
    *(uint8_t*) (dev->txdes + (dev->txid * PCNET_DE_SIZE + 7)) |= 0x80;

    uint32_t x = r_csr32(dev, 0) | (1 << 3);
    w_csr32(dev, 0, x);

    dev->txid++;
    if(dev->txid == PCNET_TX_COUNT)
        dev->txid = 0;

    //kprintf(INFO "pcnet: sending %d bytes\n", len);
}

int pcnet_startinput(void* internals) {
    struct pcnet* dev = (struct pcnet*) internals;
    if(unlikely(!dev)) {
        kprintf(ERROR "pcnet: pcnet_startinput() invalid args\n");
        return 0;
    }

    
    uint16_t size = *(uint16_t*) (dev->rxdes + (dev->rxid * PCNET_DE_SIZE + 8));
    //kprintf(INFO "pcnet: reiceved %d bytes from %d\n", size, dev->rxid);
    
    void* buf = (void*) (dev->rxbuf + dev->rxid * PCNET_BUFSIZE);
    memcpy(dev->cache, buf, size);

    dev->size = size;
    dev->offset = 0;
    return size;
}

void pcnet_input(void* internals, void* buf, uint16_t len) {
    struct pcnet* dev = (struct pcnet*) internals;
    if(unlikely(!dev)) {
        kprintf(ERROR "pcnet: pcnet_startinput() invalid args\n");
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

void pcnet_endinput(void* internals) {
    struct pcnet* dev = (struct pcnet*) internals;
    if(unlikely(!dev)) {
        kprintf(ERROR "pcnet: pcnet_endinput() invalid args\n");
        return;
    }


    ((uint8_t*) dev->rxdes) [dev->rxid * PCNET_DE_SIZE + 7] = 0x80;

    dev->rxid++;
    if(dev->rxid == PCNET_RX_COUNT)
        dev->rxid = 0;

    dev->size =
    dev->offset = 0;
}

void pcnet_input_nomem(void* internals, uint16_t len) {
    kprintf(ERROR "pcnet: no memory left for %d bytes\n", len);   
}

void pcnet_irq(void* context) {
    struct pcnet* dev = (struct pcnet*) irq_get_data(pcnet_irqno);
    if(unlikely(!dev)) {
        kprintf(ERROR "pcnet: invalid irq#%d\n", pcnet_irqno);
        return;
    }


    uint32_t x = r_csr32(dev, 0) | 0x0400;
    w_csr32(dev, 0, x);
    irq_ack(dev->irq);

    while(d_owns((uint8_t*) dev->rxdes, dev->rxid))
        ethif_input(dev->netif);
}

void pcnet_init(void* internals, uint8_t* address, void* mcast) {
    struct pcnet* dev = (struct pcnet*) internals;

    inl(dev->io + 0x18);
    inw(dev->io + 0x14);

    timer_delay(10);
    
    
    outl(dev->io + 0x10, 0);
    
    uint32_t x = r_csr32(dev, 58);
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

    dev->rxdesp = pcnet_v2p(dev, dev->rxdes);
    dev->txdesp = pcnet_v2p(dev, dev->txdes);
    dev->rxbufp = pcnet_v2p(dev, dev->rxbuf);
    dev->txbufp = pcnet_v2p(dev, dev->txbuf);


    int i;
    for(i = 0; i < PCNET_RX_COUNT; i++) {
        uint8_t* d = (uint8_t*) dev->rxdes;
        memset(&d[i * PCNET_DE_SIZE], 0, PCNET_DE_SIZE);

        *(uint32_t*) &d[i * PCNET_DE_SIZE] = (uint32_t) (dev->rxbufp + i * PCNET_BUFSIZE);

        uint16_t b = (uint16_t) (-PCNET_BUFSIZE);
        b &= 0x0FFF;
        b |= 0xF000;

        *(uint16_t*) &d[i * PCNET_DE_SIZE + 4] = (uint16_t) b;
        d[i * PCNET_DE_SIZE + 7] = 0x80;
    }

    for(i = 0; i < PCNET_TX_COUNT; i++) {
        uint8_t* d = (uint8_t*) dev->txdes;
        memset(&d[i * PCNET_DE_SIZE], 0, PCNET_DE_SIZE);

        *(uint32_t*) &d[i * PCNET_DE_SIZE] = (uint32_t) (dev->txbufp + i * PCNET_BUFSIZE);

        uint16_t b = (uint16_t) (-PCNET_BUFSIZE);
        b &= 0x0FFF;
        b |= 0xF000;

        *(uint16_t*) &d[i * PCNET_DE_SIZE + 4] = (uint16_t) b;
    }

    ((uint16_t*) dev->buf) [0] = 0;
    ((uint8_t*) dev->buf) [2] = 5 << 4;
    ((uint8_t*) dev->buf) [3] = 3 << 4;
    ((uint8_t*) dev->buf) [4] = address[0];
    ((uint8_t*) dev->buf) [5] = address[1];
    ((uint8_t*) dev->buf) [6] = address[2];
    ((uint8_t*) dev->buf) [7] = address[3];
    ((uint8_t*) dev->buf) [8] = address[4];
    ((uint8_t*) dev->buf) [9] = address[5];

    for(i = 10; i < 20; i++)
        ((uint8_t*) dev->buf) [i] = 0;

    ((uint32_t*) (dev->buf + 20)) [0] = (uint32_t) dev->rxdesp;
    ((uint32_t*) (dev->buf + 24)) [0] = (uint32_t) dev->txdesp;

    w_csr32(dev, 1, dev->bufp & 0xFFFF);
    w_csr32(dev, 2, (dev->bufp >> 16) & 0xFFFF);

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


int init(void) {
    
    void find_pci(uint32_t device, uint16_t venid, uint16_t devid, void* data) {
        if((venid == 0x1022) && (devid == 0x2000))
            *((uint32_t*) data) = device;
    }

    int pci = 0;
    pci_scan(&find_pci, -1, &pci);
    if(!pci) {
        kprintf(ERROR "pcnet: pci device not found!\n");
        return E_ERR;
    }

    struct pcnet* dev = (struct pcnet*) kmalloc(sizeof(struct pcnet), GFP_KERNEL);
    struct ethif* eth = (struct ethif*) kmalloc(sizeof(struct ethif), GFP_KERNEL);

    memset(dev, 0, sizeof(struct pcnet));
    memset(eth, 0, sizeof(struct ethif));


    eth->internals = (void*) dev;
    dev->pci = pci;



    spinlock_init(&dev->lock);

    dev->buf = (uintptr_t) kvalloc(0x10000, GFP_KERNEL);
    dev->bufp = (uintptr_t) V2P((void*) dev->buf);

    uint16_t cmd = pci_read_field(dev->pci, PCI_COMMAND, 4);
    if(!(cmd & (1 << 2))) 
        pci_write_field(dev->pci, PCI_COMMAND, 4, cmd | (1 << 2));


    dev->irq = pci_read_field(dev->pci, PCI_INTERRUPT_LINE, 1);
    dev->io = pci_read_field(dev->pci, PCI_BAR0, 4) & 0xFFFFFFF0;
    dev->mem = pci_read_field(dev->pci, PCI_BAR1, 4) & 0xFFFFFFF0;

    
    kprintf(LOG "pcnet: irq: %d, io: %p, mem: %p\n", dev->irq, dev->io, dev->mem);

    int i;
    for(i = 0; i < 6; i++)
        eth->address[i] = inb(dev->io + i);

    

    pcnet_irqno = dev->irq;   /* FIXME: fix current_irq */

    irq_enable(dev->irq, pcnet_irq);
    irq_set_data(dev->irq, dev);


    eth->low_level_init = pcnet_init;
    eth->low_level_startoutput = pcnet_startoutput;
    eth->low_level_output = pcnet_output;
    eth->low_level_endoutput = pcnet_endoutput;
    eth->low_level_startinput = pcnet_startinput;
    eth->low_level_input = pcnet_input;
    eth->low_level_endinput = pcnet_endinput;
    eth->low_level_input_nomem = pcnet_input_nomem;


    IP4_ADDR(&eth->ip, 10, 0, 2, 15);
    IP4_ADDR(&eth->nm, 255, 255, 255, 0);
    IP4_ADDR(&eth->gw, 10, 0, 2, 2);

    struct netif* netif = (struct netif*) kmalloc(sizeof(struct netif), GFP_KERNEL);
    dev->netif = netif;

    if(!netif_add(netif, &eth->ip, &eth->nm, &eth->gw, eth, ethif_init, ethernet_input)) {
        kprintf(ERROR "pcnet: netif_add() failed\n");

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