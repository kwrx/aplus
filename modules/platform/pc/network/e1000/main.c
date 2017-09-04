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


MODULE_NAME("pc/network/e1000");
MODULE_DEPS("arch/x86");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");

#if defined(__i386__)
#include <arch/i386/i386.h>
#include <arch/i386/pci.h>


#define E1000_NUM_RX_DESC               32
#define E1000_NUM_TX_DESC               8

#define E1000_REG_CTRL                  0x0000
#define E1000_REG_STATUS                0x0008
#define E1000_REG_EEPROM                0x0014
#define E1000_REG_CTRL_EXT              0x0018

#define E1000_REG_RCTRL                 0x0100
#define E1000_REG_RXDESCLO              0x2800
#define E1000_REG_RXDESCHI              0x2804
#define E1000_REG_RXDESCLEN             0x2808
#define E1000_REG_RXDESCHEAD            0x2810
#define E1000_REG_RXDESCTAIL            0x2818

#define E1000_REG_TCTRL                 0x0400
#define E1000_REG_TXDESCLO              0x3800
#define E1000_REG_TXDESCHI              0x3804
#define E1000_REG_TXDESCLEN             0x3808
#define E1000_REG_TXDESCHEAD            0x3810
#define E1000_REG_TXDESCTAIL            0x3818

#define RCTL_EN                         (1 << 1)    /* Receiver Enable */
#define RCTL_SBP                        (1 << 2)    /* Store Bad Packets */
#define RCTL_UPE                        (1 << 3)    /* Unicast Promiscuous Enabled */
#define RCTL_MPE                        (1 << 4)    /* Multicast Promiscuous Enabled */
#define RCTL_LPE                        (1 << 5)    /* Long Packet Reception Enable */
#define RCTL_LBM_NONE                   (0 << 6)    /* No Loopback */
#define RCTL_LBM_PHY                    (3 << 6)    /* PHY or external SerDesc loopback */
#define RTCL_RDMTS_HALF                 (0 << 8)    /* Free Buffer Threshold is 1/2 of RDLEN */
#define RTCL_RDMTS_QUARTER              (1 << 8)    /* Free Buffer Threshold is 1/4 of RDLEN */
#define RTCL_RDMTS_EIGHTH               (2 << 8)    /* Free Buffer Threshold is 1/8 of RDLEN */
#define RCTL_MO_36                      (0 << 12)   /* Multicast Offset - bits 47:36 */
#define RCTL_MO_35                      (1 << 12)   /* Multicast Offset - bits 46:35 */
#define RCTL_MO_34                      (2 << 12)   /* Multicast Offset - bits 45:34 */
#define RCTL_MO_32                      (3 << 12)   /* Multicast Offset - bits 43:32 */
#define RCTL_BAM                        (1 << 15)   /* Broadcast Accept Mode */
#define RCTL_VFE                        (1 << 18)   /* VLAN Filter Enable */
#define RCTL_CFIEN                      (1 << 19)   /* Canonical Form Indicator Enable */
#define RCTL_CFI                        (1 << 20)   /* Canonical Form Indicator Bit Value */
#define RCTL_DPF                        (1 << 22)   /* Discard Pause Frames */
#define RCTL_PMCF                       (1 << 23)   /* Pass MAC Control Frames */
#define RCTL_SECRC                      (1 << 26)   /* Strip Ethernet CRC */

#define RCTL_BSIZE_256                  (3 << 16)
#define RCTL_BSIZE_512                  (2 << 16)
#define RCTL_BSIZE_1024                 (1 << 16)
#define RCTL_BSIZE_2048                 (0 << 16)
#define RCTL_BSIZE_4096                 ((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192                 ((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384                ((1 << 16) | (1 << 25))

#define TCTL_EN                         (1 << 1)    /* Transmit Enable */
#define TCTL_PSP                        (1 << 3)    /* Pad Short Packets */
#define TCTL_CT_SHIFT                   4           /* Collision Threshold */
#define TCTL_COLD_SHIFT                 12          /* Collision Distance */
#define TCTL_SWXOFF                     (1 << 22)   /* Software XOFF Transmission */
#define TCTL_RTLC                       (1 << 24)   /* Re-transmit on Late Collision */

#define CMD_EOP                         (1 << 0)    /* End of Packet */
#define CMD_IFCS                        (1 << 1)    /* Insert FCS */
#define CMD_IC                          (1 << 2)    /* Insert Checksum */
#define CMD_RS                          (1 << 3)    /* Report Status */
#define CMD_RPS                         (1 << 4)    /* Report Packet Sent */
#define CMD_VLE                         (1 << 6)    /* VLAN Packet Enable */
#define CMD_IDE                         (1 << 7)    /* Interrupt Delay Enable */


typedef struct {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint16_t checksum;
    volatile uint8_t status;
    volatile uint8_t errors;
    volatile uint16_t special;
} __packed rx_t;

typedef struct {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint8_t cso;
    volatile uint8_t cmd;
    volatile uint8_t status;
    volatile uint8_t css;
    volatile uint16_t special;
} __packed tx_t;


struct e1000 {
    uint32_t pci;
    uintptr_t membase;
    int irq;
    int eeprom;
    int rx_index;
    int tx_index;

    uint8_t* rxbuf[E1000_NUM_RX_DESC];
    uint8_t* txbuf[E1000_NUM_TX_DESC];

    uintptr_t rxaddr;
    uintptr_t txaddr;

    rx_t* rx;
    tx_t* tx;

    uint8_t mac[6];
} *e1000 = NULL;



static inline void wrcmd(struct e1000* e1000, uintptr_t addr, uint32_t value) {
    mmio_w32((e1000->membase + addr), value);
}

static inline uint32_t rdcmd(struct e1000* e1000, uintptr_t addr) {
    return mmio_r32((e1000->membase + addr));
}

static void eeprom_detect(void) {
    wrcmd(e1000, E1000_REG_EEPROM, 1);

    int i;
    for(i = 0; i < 100000 && !e1000->eeprom; i++)
        if(rdcmd(e1000, E1000_REG_EEPROM) & 0x10)
            e1000->eeprom = 1;

}


static uint16_t eeprom_read(uint8_t addr) {
    wrcmd(e1000, E1000_REG_EEPROM, 1 | ((uint32_t) addr << 8));

    int e;
    while(!((e = rdcmd(e1000, E1000_REG_EEPROM)) & (1 << 4)))
        ;

    return (uint16_t) ((e >> 16) & 0xFFFF);
}


static void e1000_irq_handler(void* context) {
    uint32_t s = rdcmd(e1000, 0xC0);
    irq_ack(e1000->irq);

    if(!s)
        return;

    if(s & 0x04)
        kprintf(INFO "e1000: start linking\n");
    else if(s & 0x10)
        kprintf(WARN "e1000: unknown status %d\n", s);
    else if(s & ((1 << 6) | (1 << 7))) {
        kprintf(WARN "e1000: received packet!\n");

    }
}


static void init_rx(void) {

    wrcmd(e1000, E1000_REG_RXDESCLO, e1000->rxaddr);
    wrcmd(e1000, E1000_REG_RXDESCHI, 0);

    wrcmd(e1000, E1000_REG_RXDESCLEN, E1000_NUM_RX_DESC * sizeof(rx_t));

    wrcmd(e1000, E1000_REG_RXDESCHEAD, 0);
    wrcmd(e1000, E1000_REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);

    e1000->rx_index = 0;

    wrcmd(e1000, E1000_REG_RCTRL,
        RCTL_EN  |
        (rdcmd(e1000, E1000_REG_RCTRL) & (~((1 << 17) | (1 << 16)))));

}

static void init_tx(void) {
    wrcmd(e1000, E1000_REG_TXDESCLO, e1000->txaddr);
    wrcmd(e1000, E1000_REG_TXDESCHI, 0);

    wrcmd(e1000, E1000_REG_TXDESCLEN, E1000_NUM_TX_DESC * sizeof(tx_t));

    wrcmd(e1000, E1000_REG_TXDESCHEAD, 0);
    wrcmd(e1000, E1000_REG_TXDESCTAIL, 0);

    e1000->tx_index = 0;

    wrcmd(e1000, E1000_REG_TCTRL,
        TCTL_EN |
        TCTL_PSP |
        rdcmd(e1000, E1000_REG_TCTRL));
}


static void e1000_init() {
    uint16_t r = pci_read_field(e1000->pci, PCI_COMMAND, 2);
    r |= (1 << 2);
    r |= (1 << 0);
    pci_write_field(e1000->pci, PCI_COMMAND, 2, r);

    eeprom_detect();

    if(e1000->eeprom) {
        int i, j, t;
        for(i = j = 0; i < 3; i++, j += 2) {
            t = eeprom_read(i);
            e1000->mac[j] = t & 0xFF;
            e1000->mac[j + 1] = t >> 8;
        }
    } else {
        volatile uint8_t* t = (volatile uint8_t*) (e1000->membase + 0x5400);
        int i;
        for(i = 0; i < 6; i++)
            e1000->mac[i] = t[i];
    }

    kprintf(INFO "e1000: MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
        e1000->mac[0], e1000->mac[1], e1000->mac[2],
        e1000->mac[3], e1000->mac[4], e1000->mac[5]
    );

    wrcmd(e1000, E1000_REG_CTRL, (1 << 26));
    timer_delay(10);

    uint32_t s = rdcmd(e1000, E1000_REG_CTRL);
    s |= (1 << 5);
    s |= (1 << 6);
    s &= ~(1 << 3);
    s &= ~(1 << 31);
    s &= ~(1 << 7);

    wrcmd(e1000, E1000_REG_CTRL, s);
    wrcmd(e1000, 0x0028, 0);
    wrcmd(e1000, 0x002c, 0);
    wrcmd(e1000, 0x0030, 0);
    wrcmd(e1000, 0x0170, 0);

    s = rdcmd(e1000, E1000_REG_CTRL);
    s &= ~(1 << 30);
    
    wrcmd(e1000, E1000_REG_CTRL, s);
    timer_delay(10);

    e1000->irq = pci_read_field(e1000->pci, PCI_INTERRUPT_LINE, 1);
    irq_enable(e1000->irq, e1000_irq_handler);

    int i;
    for(i = 0; i < 128; i++)
        wrcmd(e1000, 0x5200 + i * 4, 0);

    for(i = 0; i < 64; i++)
        wrcmd(e1000, 0x4000 + i * 4, 0);

    wrcmd(e1000, E1000_REG_RCTRL, (1 << 4));

    init_rx();
    init_tx();

    wrcmd(e1000, 0x00D0, 0xFF);
    wrcmd(e1000, 0x00D8, 0xFF);
    wrcmd(e1000,
        0x00D0,
        (1 << 2)    |
        (1 << 6)    |
        (1 << 7)    |
        (1 << 1)    |
        (1 << 0)
    );

    timer_delay(10);
    s = rdcmd(e1000, E1000_REG_STATUS) & (1 << 1);

    kprintf(INFO "e1000: status: %d, eeprom: %d, membase: %p, irq: %d\n", s, e1000->eeprom, e1000->membase, e1000->irq);
}



int init(void) {
    e1000 = (struct e1000*) kcalloc(sizeof(struct e1000), 1, GFP_KERNEL);
    if(!e1000) {
        kprintf(ERROR "e1000: no memory left!\n");
        return E_ERR;
    }


    void find_pci(uint32_t device, uint16_t venid, uint16_t devid, void* data) {
        if((venid == 0x8086) && (devid == 0x100e || devid == 0x01004 || devid == 0x100f))
            *((uint32_t*) data) = device;
    }

    e1000->pci = 0;
    pci_scan(&find_pci, -1, &e1000->pci);
    if(!e1000->pci) {
        kprintf(ERROR "e1000: pci device not found!\n");
        return E_ERR;
    }

#if 0
    kprintf(INFO "BAR0: %p\nBAR1: %p\nBAR2: %p\nBAR3: %p\nBAR4: %p\n",
        pci_read_field(e1000->pci, PCI_BAR0, 4) & ~0xF,
        pci_read_field(e1000->pci, PCI_BAR1, 4) & ~0xF,
        pci_read_field(e1000->pci, PCI_BAR2, 4) & ~0xF,
        pci_read_field(e1000->pci, PCI_BAR3, 4) & ~0xF,
        pci_read_field(e1000->pci, PCI_BAR4, 4) & ~0xF
    
    );
#endif

    e1000->membase = pci_read_field(e1000->pci, PCI_BAR0, 4) & 0xFFFFFFF0;
    if(!sys_mmap((void*) (e1000->membase & ~0xFFF), 0x10000, PROT_READ | PROT_WRITE, MAP_FIXED, -1, 0)) {
        kprintf(ERROR "e1000: mapping %p failed\n", e1000->membase & ~0xFFF);
        return E_ERR;
    }


    e1000->rx = (rx_t*) kmalloc(sizeof(rx_t) * E1000_NUM_RX_DESC + 16, GFP_KERNEL);
    e1000->rxaddr = V2P(e1000->rx);

    e1000->tx = (tx_t*) kmalloc(sizeof(tx_t) * E1000_NUM_TX_DESC + 16, GFP_KERNEL);
    e1000->txaddr = V2P(e1000->tx);


    int i;
    for(i = 0; i < E1000_NUM_RX_DESC; i++) {
        e1000->rxbuf[i] = (void*) kvalloc(8192 + 16, GFP_KERNEL);
        e1000->rx[i].addr = V2P(e1000->rxbuf[i]);
        e1000->rx[i].status = 0;
    }

    for(i = 0; i < E1000_NUM_TX_DESC; i++) {
        e1000->txbuf[i] = (void*) kvalloc(8192 + 16, GFP_KERNEL);
        e1000->tx[i].addr = V2P(e1000->txbuf[i]);
        e1000->tx[i].status = 0;
        e1000->tx[i].cmd = (1 << 0);
    }

    timer_delay(10);
    e1000_init();
    return E_OK;
}


int dnit(void) {
    return E_OK;
}


#else

int init(void) {
    return E_ERR;
}

int dnit(void) {
    return E_ERR;
}

#endif