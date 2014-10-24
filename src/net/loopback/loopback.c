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


int loopback_send(netif_t* netif, void* buf, size_t len, int type) {
	if(len > 1500) {
		netif->state.tx_errors += 1;
		return 0;
	}

	bufio_t* pkt = (bufio_t*) bufio_alloc(len);
	pkt->type = LOOPBACK_MAGIC;

	memcpy(pkt->raw, buf, len);

	netif->state.tx_packets += 1;
	netif->state.tx_bytes += len;

	return len;
}

int loopback_recv(netif_t* netif, void* buf, size_t len, int type) {

	list_t* lst_packets = (list_t*) bufio_find_by_type(LOOPBACK_MAGIC);
	if(list_empty(lst_packets)) {
		list_destroy(lst_packets);
		return 0;
	}

	bufio_t* pkt = (bufio_t*) list_head(lst_packets);
	list_destroy(lst_packets);

	int ret = bufio_read(pkt, buf, len);
	if(ret == 0 || ret >= pkt->size)
		bufio_free(pkt);

	return ret;
}



int loopback_init() {
	
	static macaddr_t macaddr = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	netif_t* netif = (netif_t*) kmalloc(sizeof(netif_t));
	memset(netif, 0, sizeof(netif_t));


	strcpy(netif->name, "lo");
	memcpy(netif->macaddr, macaddr, sizeof(macaddr_t));

	netif->ipv4[0] = 127;
	netif->ipv4[1] = 0;
	netif->ipv4[2] = 0;
	netif->ipv4[3] = 1;

	netif->netmask[0] = 255;
	netif->netmask[1] = 255;
	netif->netmask[2] = 255;
	netif->netmask[3] = 255;
	

	netif->mtu = 1500;
	netif->send = loopback_send;
	netif->recv = loopback_recv;
	netif->data = NULL;

	netif_add(netif);
	return 0;
}

ATTRIBUTE("netif", loopback_init);
