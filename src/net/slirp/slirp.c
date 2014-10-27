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

#include "slirp.h"

typedef struct slirpcard {
	uint32_t magic;	
	uint8_t* buffer;
	uint8_t used;
	
	int offset;
	int ioport;
	netif_t* netif;
} slirpcard_t;

static slirpcard_t* slirpcard = NULL;

void slirp_handler(void* unused) {
	if(slirpcard == NULL)
		return;

	netif_t* netif = slirpcard->netif;

	if((netif->flags & NETIF_FLAGS_ENABLE) == 0)
		return;
	
	kprintf("slirp: received data\n");

	uint8_t d = serial_recv(slirpcard->ioport);
	if(slirpcard->used == 0) {
		if(d != SLIRP_PACKET_END)
			return;

		if(slirpcard->buffer)
			kfree(slirpcard->buffer);

		slirpcard->buffer = (uint8_t*) kmalloc(sizeof(uint8_t) * SLIRP_BUFSIZ);
		slirpcard->offset = 0;
		slirpcard->used = 1;
		return;
	}

	if((d == SLIRP_PACKET_END && slirpcard->used) || slirpcard->offset >= SLIRP_BUFSIZ) {
		slirpcard->used = 0;
			
		if(ipv4_recv(netif, slirpcard->buffer, slirpcard->offset) > 0) {
			netif->state.rx_packets += 1;
			netif->state.rx_bytes += slirpcard->offset;
		} else
			netif->state.rx_errors += 1;

		slirpcard->offset = 0;
	}

	switch(d) {
		case SLIRP_PACKET_ESC: {
			d = serial_recv(slirpcard->ioport);
			switch(d) {
				case SLIRP_PACKET_ESC_END:
					d = SLIRP_PACKET_END;
					break;
				case SLIRP_PACKET_ESC_ESC:
					d = SLIRP_PACKET_ESC;
					break;
			}
		}

		default:
			slirpcard->buffer[slirpcard->offset++] = d;
	}
	
}


int slirp_send(netif_t* netif, void* buf, size_t length, int type) {
	if((netif->flags & NETIF_FLAGS_ENABLE) == 0)
		return 0;

	slirpcard_t* slirpcard = (slirpcard_t*) netif->data;
	if(slirpcard->magic != SLIRP_MAGIC)
		return 0;

	
	serial_send(slirpcard->ioport, SLIRP_PACKET_END);

	uint8_t* b = (uint8_t*) buf;
	for(int i = 0; i < length; i++) {
		switch(b[i]) {
			case SLIRP_PACKET_END:
				serial_send(slirpcard->ioport, SLIRP_PACKET_ESC);
				serial_send(slirpcard->ioport, SLIRP_PACKET_ESC_END);
				break;

			case SLIRP_PACKET_ESC:
				serial_send(slirpcard->ioport, SLIRP_PACKET_ESC);
				serial_send(slirpcard->ioport, SLIRP_PACKET_ESC_ESC);
				break;
	
			default:
				serial_send(slirpcard->ioport, b[i]);
				break;
		}
	}

	serial_send(slirpcard->ioport, SLIRP_PACKET_END);
	return length;
}

int slirp_ifup(netif_t* netif) {
	netif->flags |= NETIF_FLAGS_ENABLE;
	return 0;
}

int slirp_ifdown(netif_t* netif) {
	netif->flags &= ~NETIF_FLAGS_ENABLE;
	return 0;
}

int slirp_init() {
	
	slirpcard = (slirpcard_t*) kmalloc(sizeof(slirpcard_t));
	slirpcard->magic = SLIRP_MAGIC;
	slirpcard->buffer = NULL;
	slirpcard->offset = 0;
	slirpcard->used = 0;
	slirpcard->ioport = 1;

	netif_t* netif = (netif_t*) kmalloc(sizeof(netif_t));
	memset(netif, 0, sizeof(netif_t));


	strcpy(netif->name, "slp0");


	netif->macaddr[0] = 10;
	netif->macaddr[1] = 0;
	netif->macaddr[2] = 2;
	netif->macaddr[3] = 0;
	netif->macaddr[4] = 255;
	netif->macaddr[5] = 255;

	netif->ipv4[0] = 10;
	netif->ipv4[1] = 0;
	netif->ipv4[2] = 2;
	netif->ipv4[3] = 0;

	netif->netmask[0] = 255;
	netif->netmask[1] = 255;
	netif->netmask[2] = 255;
	netif->netmask[3] = 0;

	netif->ipv6[0] = 0xfe80;
	netif->ipv6[1] = 0x0000;
	netif->ipv6[2] = 0x0000;
	netif->ipv6[3] = 0x0000;
	netif->ipv6[4] = 0x0000;
	netif->ipv6[5] = 0x10ff;
	netif->ipv6[6] = 0xcef9;
	netif->ipv6[7] = 0x9b70;

	netif->dns.primary.ipv4[0] = 8;
	netif->dns.primary.ipv4[1] = 8;
	netif->dns.primary.ipv4[2] = 8;
	netif->dns.primary.ipv4[3] = 8;

	netif->dns.secondary.ipv4[0] = 8;
	netif->dns.secondary.ipv4[1] = 8;
	netif->dns.secondary.ipv4[2] = 4;
	netif->dns.secondary.ipv4[3] = 4;
	

	netif->dns.primary.ipv6[0] = 0x2001;
	netif->dns.primary.ipv6[1] = 0x4860;
	netif->dns.primary.ipv6[2] = 0x4860;
	netif->dns.primary.ipv6[3] = 0x0000;
	netif->dns.primary.ipv6[4] = 0x0000;
	netif->dns.primary.ipv6[5] = 0x0000;
	netif->dns.primary.ipv6[6] = 0x0000;
	netif->dns.primary.ipv6[7] = 0x8888;
	netif->dns.secondary.ipv6[0] = 0x2001;
	netif->dns.secondary.ipv6[1] = 0x4860;
	netif->dns.secondary.ipv6[2] = 0x4860;
	netif->dns.secondary.ipv6[3] = 0x0000;
	netif->dns.secondary.ipv6[4] = 0x0000;
	netif->dns.secondary.ipv6[5] = 0x0000;
	netif->dns.secondary.ipv6[6] = 0x0000;
	netif->dns.secondary.ipv6[7] = 0x8844;


	netif->mtu = SLIRP_MTU;
	netif->send = slirp_send;
	netif->ifup = slirp_ifup;
	netif->ifdown = slirp_ifdown;
	netif->data = (void*) slirpcard;

	slirpcard->netif = netif;
	netif_add(netif);
	

	irq_set(4, slirp_handler);
	irq_set(3, slirp_handler);
	return 0;
}

ATTRIBUTE("netif", slirp_init);
