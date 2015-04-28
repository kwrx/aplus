#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <aplus.h>
#include <aplus/fs.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>


#include "e1000.h"
#include <aplus/mmio.h>
#include <aplus/netif.h>
#include <aplus/mm.h>

#include <arch/i386/i386.h>

static e1000_t e1000;

static void cmdwr(uint16_t a, uint32_t v) {
	if(e1000.bar_type == E1000_BAR_MMIO)
		return mmio_w32(e1000.mem_base + a, v);
	
	outl(e1000.io_base, a);
	outl(e1000.io_base + 4, v);
}


static uint32_t cmdrd(uint16_t a) {
	if(e1000.bar_type == E1000_BAR_MMIO)
		return mmio_r32(e1000.mem_base + a);

	outl(e1000.io_base, a);
	return inl(e1000.io_base + 4);
}


static uint8_t eeprom_detect(void) {
	cmdwr(REG_EEPROM, 1);
	for(int i = 0; i < 1000; i++)
		if(cmdrd(REG_EEPROM) & 0x10)
			return 1;
	
	return 0;
}

static uint16_t eeprom_read(uint8_t a) {
	int t = 0;
	if(e1000.eeprom_exists) {
		cmdwr(REG_EEPROM, 1 | ((uint32_t) (a) << 8));

		while(!((t = cmdrd(REG_EEPROM)) & (1 << 4)))
			cpu_wait();
	} else {
		cmdwr(REG_EEPROM, 1 | ((uint32_t) (a) << 2));
		
		while(!((t = cmdrd(REG_EEPROM)) & (1 << 1)))
			cpu_wait();
	}

	return (t >> 16) & 0xFFFF;
}


static int read_macaddr(macaddr_t* macaddr) {
	uint8_t mt[6];

	if(e1000.eeprom_exists) {
		register uint16_t t = eeprom_read(0);
		mt[0] = t & 0xFF;
		mt[1] = (t >> 8) & 0xFF;
	
		t = eeprom_read(1);
		mt[2] = t & 0xFF;
		mt[3] = (t >> 8) & 0xFF;
	
		t = eeprom_read(2);
		mt[4] = t & 0xFF;
		mt[5] = (t >> 8) & 0xFF;
	} else {
		if(mmio_r32(e1000.mem_base + 0x5400) == 0)
			return -1;

		for(int i = 0; i < 6; i++)
			mt[i] = mmio_r8(e1000.mem_base + 0x5400 + i);
	}

#ifdef E1000_DEBUG
	kprintf("e1000: mac address -> ");
	for(int i = 0; i < 5; i++)
		kprintf("%x:", mt[i]);
	kprintf("%x\n", mt[6]);
#endif

	memcpy(macaddr, mt, 6);
	return 0;
}

static void e1000_rx_init(void) {
	e1000_rx_desc_t* descs = (e1000_rx_desc_t*) kmalloc(sizeof(e1000_rx_desc_t) * E1000_NUM_RX_DESC + 16);
	for(int i = 0; i < E1000_NUM_RX_DESC; i++) {
		e1000.rx_descs[i] = (e1000_rx_desc_t*) ((uint32_t) descs + i * 16);
		e1000.rx_descs[i]->addr = (uintptr_t) mm_paddr((void*) kmalloc(8192 + 16));
		e1000.rx_descs[i]->status = 0;
	}

	/* FIXME */
	cmdwr(REG_RXDESCLO, (uint32_t) mm_paddr((void*) descs));
	cmdwr(REG_RXDESCHI, 0);

	cmdwr(REG_RXDESCLEN, E1000_NUM_RX_DESC * 16);
	cmdwr(REG_RXDESCHEAD, 0);
	cmdwr(REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);

	e1000.rx_cur = 0;

	cmdwr(REG_RCTRL, 	RCTL_EN 		|
						RCTL_SBP		|
						RCTL_UPE		|
						RCTL_MPE		|
						RCTL_LBM_NONE	|
						RTCL_RDMTS_HALF	|
						RCTL_BAM		|
						RCTL_SECRC		|
						RCTL_BSIZE_2048
	);
}


static void e1000_tx_init(void) {
	e1000_tx_desc_t* descs = (e1000_tx_desc_t*) kmalloc(sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESC + 16);
	for(int i = 0; i < E1000_NUM_TX_DESC; i++) {
		e1000.tx_descs[i] = (e1000_tx_desc_t*) ((uint32_t) descs + i * 16);
		e1000.tx_descs[i]->addr = (uintptr_t) 0;
		e1000.tx_descs[i]->cmd = 0;
		e1000.tx_descs[i]->status = TSTA_DD;
	}

	/* FIXME */
	cmdwr(REG_TXDESCLO, (uint32_t) mm_paddr((void*) descs));
	cmdwr(REG_TXDESCHI, (uint32_t) 0);

	cmdwr(REG_TXDESCLEN, E1000_NUM_TX_DESC * 16);
	cmdwr(REG_TXDESCHEAD, 0);
	cmdwr(REG_TXDESCTAIL, E1000_NUM_TX_DESC - 1);
	
	e1000.tx_cur = 0;

	cmdwr(REG_TCTRL,	TCTL_EN					|
						TCTL_PSP				|
						(15 << TCTL_CT_SHIFT)	|
						(64 << TCTL_COLD_SHIFT)	|
						TCTL_RTLC
	);

	cmdwr(REG_TIPG, 0x0060200A);
}

static void e1000_intr() {
	cmdwr(REG_IMASK, 0x1F6DC);
	cmdwr(REG_IMASK, 0xFF & ~4);
	cmdrd(0xC0);
}


static void e1000_recv(void) {
	while((e1000.rx_descs[e1000.rx_cur]->status & 0x01)) {
		void* buf = (void*) mm_vaddr((void*) (e1000.rx_descs[e1000.rx_cur]->addr));
		uint32_t length = e1000.rx_descs[e1000.rx_cur]->length;		

		//netif_packet_push(NETIF_RAW, buf, 0, buf, length);
		
#ifdef E1000_DEBUG
		kprintf("e1000: received packet on RX(%d) of %d Bytes.\n", e1000.rx_cur, length);
#endif
		
		e1000.rx_descs[e1000.rx_cur]->status = 0;
		
		int t = e1000.rx_cur;
		e1000.rx_cur = (e1000.rx_cur) % E1000_NUM_RX_DESC;

		cmdwr(REG_RXDESCTAIL, t);
	}
}

static void e1000_link(void) {

	int v = cmdrd(REG_CTRL);
	cmdwr(REG_CTRL, v | ECTRL_SLU);

	return;
}

static void e1000_handler(void* unused) {
	(void) unused;


	uint32_t s = cmdrd(0xC0);
#ifdef E1000_DEBUG
	kprintf("e1000: received interrupt with status %d!\n", s);
#endif
	if(s & 0x04)
		e1000_link();
	else if(s & 0x10)
		/* Nothing */;
	else
		e1000_recv();
}


static void e1000_sendpacket(void* buf, int length) {
	e1000.tx_descs[e1000.tx_cur]->addr = (uintptr_t) mm_paddr(buf);
	e1000.tx_descs[e1000.tx_cur]->length = length;
	e1000.tx_descs[e1000.tx_cur]->cmd = CMD_EOP | CMD_IFCS | CMD_RS | CMD_RPS;
	e1000.tx_descs[e1000.tx_cur]->status = 0;

	int t = e1000.tx_cur;
	e1000.tx_cur = (e1000.tx_cur + 1) % E1000_NUM_TX_DESC;
	
	cmdwr(REG_TXDESCTAIL, e1000.tx_cur);
	while(!(e1000.tx_descs[t]->status & 0xFF))
		cpu_wait();
}

int init() {
	pci_device_t* e1000_device;
	for(int i = 0; e1000_dev[i]; i++)
		if((e1000_device = (pci_device_t*) pci_find_by_id(INTEL_VEND, e1000_dev[i])))
			break;

	if(!e1000_device) {
#ifdef E1000_DEBUG
		kprintf("e1000: network card not found.\n");
#endif

		return -1;
	}


#ifdef E1000_DEBUG
	kprintf("e1000: %x/%x\n", e1000_device->vendorID, e1000_device->deviceID);
	kprintf("e1000: iobase -> 0x%x\n", e1000_device->iobase);
	kprintf("e1000: membase -> 0x%x\n", e1000_device->membase);
	kprintf("e1000: intr -> %d\n", e1000_device->intr_line);
#endif
	
	memset(&e1000, 0, sizeof(e1000_t));
	e1000.bar_type = 0;
	e1000.mem_base = e1000_device->membase;
	e1000.io_base = e1000_device->iobase;
	e1000.eeprom_exists = 0;
	

	macaddr_t macaddr;

	e1000.eeprom_exists = eeprom_detect();
	read_macaddr(&macaddr);

	e1000_link();	

	for(int i = 0; i < 0x80; i++)
		cmdwr(0x5200 + i * 4, 0);


	irq_set(e1000_device->intr_line, e1000_handler);
	e1000_intr();

	e1000_rx_init();
	e1000_tx_init();

#ifdef E1000_DEBUG
	kprintf("e1000: started successful!\n");
#endif
	

	char* pkt = kmalloc(60);
	memset(pkt, 0, 60);
	e1000_sendpacket(pkt, 60);

	kprintf("e1000: trasmitted\n");
	
	return 0;
}


int dnit() {
	return 0;
}

