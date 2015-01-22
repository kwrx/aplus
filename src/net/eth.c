#include <aplus.h>
#include <aplus/netif.h>
#include <aplus/spinlock.h>
#include <aplus/attribute.h>

#include <string.h>
#include <stdint.h>
#include <errno.h>


#include <aplus/net/eth.h>


int eth_recv(netif_t* netif, void* buf, size_t length) {
	eth_t* pkt = (eth_t*) buf;

	#define __p											\
		netif, 											\
		pkt->data,										\
		length - sizeof(eth_t)


	switch(pkt->type) {
		case ETH_TYPE_IPV4:
			/*if(unlikely(ipv4_recv(__p) == 0))
				return 0;*/
			break;
		case ETH_TYPE_IPV6:
			/*if(unlikely(ipv6_recv(__p) == 0))
				return 0;*/
			break;
		case ETH_TYPE_ARP:
			/*if(unlikely(arp_recv(__p) == 0))
				return 0;*/
			break;
	}

	netif_packet_push(NETIF_ETH, pkt, sizeof(eth_t), pkt->data, length - sizeof(eth_t));
	return length;
}


eth_t* eth_packet(netif_t* netif, macaddr_t dest, int type, void* data, size_t* length) {
	if(unlikely(!length))
		return NULL;
	
	
	eth_t* p = (eth_t*) kmalloc(sizeof(eth_t) + *length);

	memcpy(p->dest, dest, sizeof(macaddr_t));
	memcpy(p->source, netif->macaddr, sizeof(macaddr_t));


	switch(type) {
		case NETIF_INET:
			p->type = ETH_TYPE_IPV4;
			break;
		case NETIF_INET6:
			p->type = ETH_TYPE_IPV6;
			break;
		case NETIF_ARP:
			p->type = ETH_TYPE_ARP;
			break;
		default:
			p->type = ETH_TYPE_RAW;
			break;
	}

	if(likely(data))
		memcpy(p->data, data, *length);

	*length += sizeof(eth_t);
	
	return p;
} 


int eth_check(netif_socket_t* sock, netif_packet_t* packet) {
	if(unlikely(!packet))
		return 0;

	eth_t* eth = (eth_t*) packet->header.ptr;

	if (
		(memcmp(eth->dest, sock->netif->macaddr, sizeof(macaddr_t)) == 0)	||
		(memcmp(eth->dest, MACADDR_BROADCAST, sizeof(macaddr_t)) == 0)
	) return 1;

	return 0;
}


int eth_send(netif_socket_t* sock, void* buf, size_t size) {
	if(unlikely(!buf || !size || !sock))
		return 0;

	eth_t* eth = eth_packet(sock->netif, ((struct sockaddr_eth*) (&sock->sockaddr))->set_addr.eth_addr, NETIF_RAW, buf, &size);

	if(size > sock->netif->mtu)
		size = sock->netif->mtu;

	int r = sock->netif->send(sock->netif, (void*) eth, size, NETIF_ETH);
	kfree(eth);

	return r - sizeof(eth_t);
}

int eth_close(netif_socket_t* sock, int status) {
	return 0;
}

NETIF_PROTO(AF_INET, NETIF_ETH, eth);
