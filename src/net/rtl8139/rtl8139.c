#include <aplus.h>
#include <aplus/netif.h>
#include <aplus/bufio.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/spinlock.h>
#include <aplus/list.h>
#include <aplus/attribute.h>

#include <string.h>
#include <stdint.h>
#include <errno.h>

#include <aplus/net/eth.h>

#include "rtl8139.h"

typedef struct card {
	pci_device_t* device;
	netif_t* netif;
	
	char macaddr[6];
	
	char* rxBuffer;
	int rxBufferOffset;

	char* txBuffer;
	char txBufferUsed:1;
	char curBuffer; 

	int magic;
} card_t;


static card_t* card = NULL;


uint8_t rtl8139_rxbuffer[RX_BUFFER_SIZE + 16];
uint8_t rtl8139_txbuffer[TX_BUFFER_SIZE * 4 + 16]; 


int rtl8139_ifup(netif_t* netif) {
	netif->flags |= NETIF_FLAGS_ENABLE;
	return 0;
}

int rtl8139_ifdown(netif_t* netif) {
	netif->flags &= ~NETIF_FLAGS_ENABLE;
	return 0;
}

int rtl8139_send(netif_t* netif, void* buf, size_t len, int type) {
	if((netif->flags & NETIF_FLAGS_ENABLE) == 0)
		return 0;

	card_t* card = (card_t*) netif->data;
	if(card->magic != RTL8139_MAGIC)
		return 0;

	if(len > 1500) {
		netif->state.tx_errors += 1;
		return 0;
	}

	fastlock_waiton(card->txBufferUsed);

	memcpy(card->txBuffer, buf, len);
	if(len < 60) {
		memset((void*) ((uint32_t) card->txBuffer + len), 0, 60 - len);
		len = 60;
	}

	uint8_t curBuffer = card->curBuffer++;
	card->curBuffer %= 4;


	int_out32(card, REG_TRANSMIT_ADDR0 + (4 * curBuffer), (uint32_t) card->txBuffer + (TX_BUFFER_SIZE * curBuffer));
	int_out32(card, REG_TRANSMIT_STATUS0 + (4 * curBuffer), ((256 << 11) & 0x003F0000) | len);

	netif->state.tx_packets += 1;
	netif->state.tx_bytes += len;

	return len;
}


static void recvdata(card_t* card) {
	if((card->netif->flags & NETIF_FLAGS_ENABLE) == 0)
		return;

	while(1) {
		uint8_t cmd = int_in8(card, REG_COMMAND);
	
		if(cmd & CR_BUFFER_IS_EMPTY)
			break;

		uint16_t* rxBuffer = (uint16_t*) ((uint32_t) card->rxBuffer + card->rxBufferOffset);
		uint16_t head = *rxBuffer++;

		if((head & 1) == 0)
			break;

		uint16_t length = *rxBuffer++;
		length -= 4;

		card->rxBufferOffset += 4;


		void* data = kmalloc(length);
		if((card->rxBufferOffset + length) >= RX_BUFFER_SIZE) {
			memcpy(data, rxBuffer, RX_BUFFER_SIZE - card->rxBufferOffset);
			memcpy((void*) ((uint32_t) data + RX_BUFFER_SIZE - card->rxBufferOffset), card->rxBuffer, length - (RX_BUFFER_SIZE - card->rxBufferOffset));
		} else
			memcpy(data, rxBuffer, length);

#ifdef RTL8139_DEBUG
		kprintf("rtl8139: receveid %d bytes\n", length);
#endif

#ifdef USERNET
		if(udp_recv(card->netif, data, length) > 0) {
#else
		if(eth_recv(card->netif, data, length) > 0) {
#endif
			card->netif->state.rx_packets += 1;
			card->netif->state.rx_bytes += length;
		} else
			card->netif->state.rx_errors += 1;
		

		card->rxBufferOffset += length + 4;
		card->rxBufferOffset = (card->rxBufferOffset + 3) & ~3;
		card->rxBufferOffset %= RX_BUFFER_SIZE;

		int_out16(card, REG_CUR_READ_ADDR, card->rxBufferOffset - 0x10);
	}
}

static void rtl8139_handler(void* unused) {
	uint16_t isr = int_in16(card, REG_INTERRUPT_STATUS);
	uint16_t nsr = 0;

	if(isr & ISR_TRANSMIT_OK) {
#ifdef RTL8139_DEBUG
		kprintf("rtl8139: Transmitted data successfully\n");
#endif

		card->txBufferUsed = 0;
		nsr |= ISR_TRANSMIT_OK;
	}

	if(isr & ISR_RECEIVE_OK) {
#ifdef RTL8139_DEBUG
		kprintf("rtl8139: Received data\n");
#endif
		recvdata(card);
		nsr |= ISR_RECEIVE_OK;
	}

	int_out16(card, REG_INTERRUPT_STATUS, nsr);
}


int rtl8139_init() {
	pci_device_t* device = (pci_device_t*) pci_find_by_id(0x10EC, 0x8139);
	if(device == NULL) {
#ifdef RTL8139_DEBUG
		kprintf("rtl8139: no device found\n");
#endif
		return -1;
	}
	
	card = kmalloc(sizeof(card_t));
	card->magic = RTL8139_MAGIC;
	card->device = device;

	int_out8(card, REG_CONFIG1, 0);
	int_out8(card, REG_COMMAND, CR_RESET);

	while((int_in8(card, REG_COMMAND) & REG_COMMAND) == CR_RESET);

	memset(card->macaddr, 0, 6);
	for(int i = 0; i < 6; i++)
		card->macaddr[i] = int_in8(card, i);
	

#ifdef RTL8139_DEBUG
	kprintf("rtl8139: %d:%d.%d, iobase 0x%x, irq %d, MAC Address %02x:%02x:%02x:%02x:%02x:%02x\n",
		card->device->bus,
		card->device->dev,
		card->device->func,
		card->device->iobase,
		card->device->intr_line,
		card->macaddr[0],
		card->macaddr[1],
		card->macaddr[2],
		card->macaddr[3],
		card->macaddr[4],
		card->macaddr[5]
	);
#endif

	if(card->device->intr_line == 0xFF) {
		kprintf("rtl8139: network card isn't connected to the PIC\n");
		return -1;
	}

	card->rxBuffer = (char*) rtl8139_rxbuffer;
	card->txBuffer = (char*) rtl8139_txbuffer;

	card->rxBufferOffset = 0;
	card->curBuffer = 0;
	card->txBufferUsed = 0;

	memset(card->rxBuffer, 0, RX_BUFFER_SIZE + 16);
	memset(card->txBuffer, 0, TX_BUFFER_SIZE * 4 + 16);


	irq_set(card->device->intr_line, rtl8139_handler);

	int_out16(card, REG_INTERRUPT_MASK, 0x0005);
	int_out16(card, REG_INTERRUPT_STATUS, 0);

	int_out8(card, REG_COMMAND, CR_RECEIVER_ENABLE | CR_TRANSMITTER_ENABLE);

	int_out32(card, REG_RECEIVE_BUFFER, (uint32_t) card->rxBuffer);

	for(int i = 0; i < 4; i++)
		int_out32(card, REG_TRANSMIT_ADDR0 + (4 * i), (uint32_t) card->txBuffer + (TX_BUFFER_SIZE * i));

	int_out32(card, REG_RECEIVE_CONFIGURATION, RCR_MXDMA_UNLIMITED | RCR_ACCEPT_BROADCAST | RCR_ACCEPT_PHYS_MATCH);
	int_out32(card, REG_TRANSMIT_CONFIGURATION, TCR_IFG_STANDARD | TCR_MXDMA_256);

	int_out8(card, REG_COMMAND, CR_RECEIVER_ENABLE | CR_TRANSMITTER_ENABLE);


	card->netif = (netif_t*) kmalloc(sizeof(netif_t));
	memset(card->netif, 0, sizeof(netif_t));


	strcpy(card->netif->name, "eth0");
	memcpy(card->netif->macaddr, card->macaddr, sizeof(macaddr_t));

	card->netif->ipv4[0] = 192;
	card->netif->ipv4[1] = 168;
	card->netif->ipv4[2] = 1;
	card->netif->ipv4[3] = 80;

	card->netif->netmask[0] = 255;
	card->netif->netmask[1] = 255;
	card->netif->netmask[2] = 255;
	card->netif->netmask[3] = 0;

	card->netif->ipv6[0] = 0xfe80;
	card->netif->ipv6[1] = 0x0000;
	card->netif->ipv6[2] = 0x0000;
	card->netif->ipv6[3] = 0x0000;
	card->netif->ipv6[4] = 0x021d;
	card->netif->ipv6[5] = 0x72ff;
	card->netif->ipv6[6] = 0xfef9;
	card->netif->ipv6[7] = 0x9b71;

	card->netif->dns.primary.ipv4[0] = 8;
	card->netif->dns.primary.ipv4[1] = 8;
	card->netif->dns.primary.ipv4[2] = 8;
	card->netif->dns.primary.ipv4[3] = 8;

	card->netif->dns.secondary.ipv4[0] = 8;
	card->netif->dns.secondary.ipv4[1] = 8;
	card->netif->dns.secondary.ipv4[2] = 4;
	card->netif->dns.secondary.ipv4[3] = 4;
	

	card->netif->dns.primary.ipv6[0] = 0x2001;
	card->netif->dns.primary.ipv6[1] = 0x4860;
	card->netif->dns.primary.ipv6[2] = 0x4860;
	card->netif->dns.primary.ipv6[3] = 0x0000;
	card->netif->dns.primary.ipv6[4] = 0x0000;
	card->netif->dns.primary.ipv6[5] = 0x0000;
	card->netif->dns.primary.ipv6[6] = 0x0000;
	card->netif->dns.primary.ipv6[7] = 0x8888;
	card->netif->dns.secondary.ipv6[0] = 0x2001;
	card->netif->dns.secondary.ipv6[1] = 0x4860;
	card->netif->dns.secondary.ipv6[2] = 0x4860;
	card->netif->dns.secondary.ipv6[3] = 0x0000;
	card->netif->dns.secondary.ipv6[4] = 0x0000;
	card->netif->dns.secondary.ipv6[5] = 0x0000;
	card->netif->dns.secondary.ipv6[6] = 0x0000;
	card->netif->dns.secondary.ipv6[7] = 0x8844;


	card->netif->mtu = 1500;
	card->netif->send = rtl8139_send;
	card->netif->ifup = rtl8139_ifup;
	card->netif->ifdown = rtl8139_ifdown;
	card->netif->data = (void*) card;

#ifdef RTL8139_DEBUG
	kprintf("rtl8139: sending data for test (512 Bytes)\n");
	
	void* tmpbuf = kmalloc(512);
	memset(tmpbuf, 0xFF, 512);

	rtl8139_ifup(card->netif);
	eth_send(card->netif, tmpbuf, 512, NETIF_RAW); 
	rtl8139_ifdown(card->netif);

#endif

	netif_add(card->netif);
	return 0;
}



ATTRIBUTE("netif", rtl8139_init);




