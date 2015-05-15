#include <aplus.h>
#include <aplus/spinlock.h>
#include <aplus/pci.h>
#include <aplus/mmio.h>

#include <arch/i386/i386.h>

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>


static uint16_t net_i82xx_eeprom_read(i825xx_device_t* dev, uint8_t addr) {
	uint16_t data = 0;
	uint32_t tmp = 0;

	mmio_w32(i825xx_REG_EERD, (1) | ((uint32_t) (addr) << 8));

	while(!((tmp = mmio_r32(i825xx_REG_EERD)) & (1 << 4)))
		timer_delay(1);

	data = (uint16_t) ((tmp >> 16) & 0xFFFF);
	return data;
}

static uint16_t net_i825xx_phy_read(i825xx_device_t* dev, int MDIC_REGADD) {
	uint16_t data = 0;
	
	mmio_w32(i825xx_REG_MDIC, (((MDIC_REGADD & 0x1F) << 16) | MDIC_PHYADD | MDIC_I | MDIC_OP_READ));

	while(!(mmio_r32(i825xx_REG_MDIC) & (MDIC_R | MDIC_E)))
		timer_delay(1);

	if(mmio_r32(i825xx_REG_MDIC) & MDIC_E) {
#ifdef I825XX_DEBUG
		kprintf("i825xx: MDI Read Error!\n");
		return -1;
#endif
	}


	data = (uint16_t) (mmio_r32(i825xx_REG_MDIC) & 0xFFFF);
	return data;
}

static void net_i825xx_phy_write(i825xx_device_t* dev, int MDIC_REGADD, uint16_t data) {
	mmio_w32(i825xx_REG_MDIC, ((data & 0xFFFF) | ((MDIC_REGADD & 0x1F) << 16) | MDIC_PHYADD | MDIC_I | MDIC_OP_WRITE));
	
	while(!(mmio_r32(i825xx_REG_MDIC) & (MDIC_R | MDIC_E)))
		timer_delay(1);

	if(mmio_r32(i825xx_REG_MDIC) & MDIC_E) {
#ifdef I825XX_DEBUG
		kprintf("i825xx: MDI Write Error!\n");
#endif
		return;
	}
}


static void net_i825xx_rx_enable(i825xx_device_t* dev) {
	mmio_w32(i825xx_REG_RTCL, mmio_r32(i825xx_REG_RCTL) | (RTCL_EN));
}

static int net_i825xx_rx_init(i825xx_device_t* dev) {
	int i;
	uintptr_t tmpbase = (uintptr_t) kmalloc((sizeof(i825xx_rx_desc_t) * NUM_RX_DESC) + 16);
	
	dev->rx_desc_base = (tmpbase % 16)
							? (uint8_t*) ((tmpbase) + 16 - (tmpbase % 16))
							: (uint8_t*) tmpbase;
	
	for(i = 0; i < NUM_RX_DESC; i++) {
		dev->rx_desc[i] = (i825xx_rx_desc_t*) (dev->rx_desc_base + (i * 16));
		dev->rx_desc[i]->address = (uintptr_t) kmalloc(8192 + 16);
		dev->rx_desc[i]->status = 0;
	}

	mmio_w32(i825xx_REG_RDBAH, (uint32_t) ((uint64_t) dev->rx_desc_base >> 32));
	mmio_w32(i825xx_REG_RDBAL, (uint32_t) ((uint64_t) dev->rx_desc_base & 0xFFFFFFFF));

#ifdef I825XX_DEBUG
	kprintf("i825xx: RDBAH/RDBAL = 0x%x:0x%x\n", mmio_r32(i825xx_REG_RDBAH), mmio_r32(i825xx_REG_RDBAL));
#endif

	mmio_w32(i825xx_REG_RDLEN, (uint32_t) (NUM_RX_DESC * 16));
	mmio_w32(i825xx_REG_RDH, 0);
	mmio_w32(i825xx_REG_RDT, NUM_RX_DESC);

	dev->rx_tail = 0;

	mmio_w32(i825xx_REG_RTCL,
					RCTL_SBP			|
					RCTL_UPE			|
					RCTL_MPE			|
					RDMTS_HALF			|
					RCTL_SECRC			|
					RTCL_LPE			|
					RTCL_BAM			|
					RTCL_BSIZE_8192
	);

	return 0;
}


static int net_i825xx_tx_init(i825xx_device_t* dev) {
	int i;	
	uintptr_t tmpbase = (uintptr_t) kmalloc((sizeof(i825xx_tx_desc_t) * NUM_TX_DESCRIPTORS) + 16);
	dev->tx_desc_base = (tmpbase % 16)
							? (uint8_t*) ((tmpbase) + 16 - (tmpbase % 16))
							: (uint8_t*) tmpbase;

	for(i = 0; i < NUM_TX_DESC; i++) {
		dev->tx_desc[i] = (i825xx_tx_desc_t*) (dev->tx_desc_base + (i * 16));
		dev->tx_desc[i]->address = 0;
		dev->tx_desc[i]->cmd = 0;
	}


	mmio_w32(i825xx_REG_TDBAH, (uint32_t) ((uint64_t) dev->tx_desc_base >> 32));
	mmio_w32(i825xx_REG_TDBAL, (uint32_t) ((uint64_t) dev->tx_desc_base & 0xFFFFFFFF));

#ifdef I825XX_DEBUG
	kprintf("i825xx: RDBAH/RDBAL = 0x%x:0x%x\n", mmio_r32(i825xx_REG_TDBAH), mmio_r32(i825xx_REG_TDBAL));
#endif

	mmio_w32(i825xx_REG_TDLEN, (uint32_t) (NUM_TX_DESC * 16));

	mmio_w32(i825xx_REG_TDH, 0);
	mmio_w32(i825xx_REG_TDT, NUM_TX_DESC);

	dev->tx_tail = 0;

	mmio_w32(i825xx_REG_TCTL, (TCTL_EN | TCTL_PSP));
	return 0;
}



static void net_i825xx_tx_poll(i825xx_device_t* dev, void* pkt, uint16_t length) {
	dev->tx_desc[dev->tx_tail]->address = (uint64_t) pkt;
	dev->tx_desc[dev->tx_tail]->length = length;
	dev->tx_desc[dev->tx_tail]->cmd = ((1 << 3) | (3));

	int oldtail = dev->tx_tail;
	dev->tx_tail = (dev->tx_tail + 1) % NUM_TX_DESC;

	mmio_w32(i825xx_REG_TDT, dev->tx_tail);
	
	while(!(dev->tx_desc[oldtail]->sta & 0x0F))
		timer_delay(1);
}

static void net_i825xx_rx_poll(i825xx_device_t* dev) {
	while((dev->rx_desc[dev->rx_tail]->status & (1 << 0))) {
		uint8_t* pkt = (void*) dev->rx_desc[dev->rx_tail]->address;
		uint16_t pktlen = dev->rx_desc[dev->rx_tail]->length;

		int dropflag = 0;

		if(pktlen < 60)
			dropflag = 1;

		if(!(dev->rx_desc[dev->rx_tail]->status & (1 << 1)))
			dropflag = 1;

		if(dev->rx_desc[dev->rx_tail]->errors)
			dropflag = 1;

		if(!dropflag)
			net_i825xx_recv(dev, pkt, pktlen);

		dev->rx_desc[dev->rx_tail]->status = (uint16_t) 0;
		dev->rx_tail = (dev->rx_tail + 1) % NUM_RX_DESC;

		mmio_w32(i825xx_REG_RDT, dev->rx_tail);
	}

}


static void i825xx_intr_handler(void* unused) {
	(void) unused;

	i825xx_device_t* dev = (i825xx_device_t*) irq_get_data(void);
	if(unlikely(dev == NULL)) {
#ifdef I825XX_DEBUG
		kprintf("i825xx: Unknown IRQ / Invalid Device!\n");
#endif

		return;
	}

	uint32_t icr = mmio_r32(dev->mmio_address + 0xC0);
	icr &= ~(3);

	if(icr & (1 << 2)) {
		icr &= ~(1 << 2);

		mmio_w32(i825xx_REG_CTRL, (mmio_r32(i825xx_REG_CTRL) | CTRL_SLU));

#ifdef I825XX_DEBUG
		kprintf("i825xx: Link Status Change, STATUS = 0x%x\n", mmio_r32(i825xx_REG_STATUS));
#endif
	}

	if(icr & (1 << 6) || icr & (1 << 4)) {
		icr &= ~((1 << 6) | (1 << 4));

		net_i825xx_rx_poll(dev);
	}

	if(icr & (1 << 7)) {
		icr &= ~(1 << 7);
		net_i825xx_rx_poll(dev);
	}

	mmio_r32(dev->mmio_address + 0xC0);
}


int init(void) {
	
	return 0;
}


int dnit(void) {
	return 0;
}
