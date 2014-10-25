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


#define LOOPBACK_MAGIC				0x127001FF
#define LOOPBACK_MTU				65563


int loopback_ifup(netif_t* netif) {
	netif->flags |= NETIF_FLAGS_ENABLE;
	return 0;
}

int loopback_ifdown(netif_t* netif) {
	netif->flags &= ~NETIF_FLAGS_ENABLE;
	return 0;
}



int loopback_recv(netif_t* netif, void* buf, size_t len) {
	if((netif->flags & NETIF_FLAGS_ENABLE) == 0)
		return 0;

	if(eth_recv(netif, buf, len) > 0) {
		netif->state.rx_packets += 1;
		netif->state.rx_bytes += len;

		return len;
	}
		
	netif->state.rx_errors += 1;
	return 0;
}

int loopback_send(netif_t* netif, void* buf, size_t len, int type) {
	if((netif->flags & NETIF_FLAGS_ENABLE) == 0)
		return 0;

	if(len > LOOPBACK_MTU) {
		netif->state.tx_errors += 1;
		return 0;
	}


	netif->state.tx_packets += 1;
	netif->state.tx_bytes += len;


	loopback_recv(netif, buf, len);	
	return len;
}


int loopback_init() {

	netif_t* netif = (netif_t*) kmalloc(sizeof(netif_t));
	memset(netif, 0, sizeof(netif_t));


	strcpy(netif->name, "lo");
	memset(netif->macaddr, 0xFF, sizeof(macaddr_t));

	netif->ipv4[0] = 127;
	netif->ipv4[1] = 0;
	netif->ipv4[2] = 0;
	netif->ipv4[3] = 1;

	netif->ipv6[0] = 0;
	netif->ipv6[1] = 0;
	netif->ipv6[2] = 0;
	netif->ipv6[3] = 0;
	netif->ipv6[4] = 0;
	netif->ipv6[5] = 0;
	netif->ipv6[6] = 0;
	netif->ipv6[7] = 1;

	netif->netmask[0] = 255;
	netif->netmask[1] = 255;
	netif->netmask[2] = 255;
	netif->netmask[3] = 255;


	netif->mtu = LOOPBACK_MTU;
	netif->send = loopback_send;
	netif->ifup = loopback_ifup;
	netif->ifdown = loopback_ifdown;
	netif->data = NULL;

	netif_add(netif);
	return 0;
}

ATTRIBUTE("netif", loopback_init);
