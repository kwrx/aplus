#include <aplus.h>
#include <aplus/list.h>
#include <aplus/netif.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <aplus/net/eth.h>

int eth_recv(netif_t* netif, void* buf, size_t length) {
	eth_header_t* ethpkt = (eth_header_t*) buf;

	#define __params	\
		netif, (void*) ((uint32_t) buf + sizeof(eth_header_t)), length - sizeof(eth_header_t)

	switch(ethpkt->type) {
		case ETH_TYPE_IPV4:
			if(ipv4_recv(__params) == 0)
				return 0;
			return length;

		case ETH_TYPE_IPV6:
			if(ipv6_recv(__params) == 0)
				return 0;
			return length;

		case ETH_TYPE_ARP:
			//if(arp_recv(__params) == 0)
			//	return 0;
			return length;
	}


	/* ETH_TYPE_RAW */
	netif_packets_add (
		netif_packets_create (
							NETIF_ETH, 
							length, 
							sizeof(eth_header_t), 
							buf
		)
	);
	return length;
}


static int eth_send_packet(netif_t* netif, void* buf, size_t length, int type) {
	eth_header_t* ethpkt = kmalloc(length + sizeof(eth_header_t));
	memset((void*) ethpkt->dest, 0xFF, sizeof(macaddr_t));
	memcpy((void*) ethpkt->source, (void*) netif->macaddr, sizeof(macaddr_t));
	memcpy((void*) ((uint32_t) ethpkt + sizeof(eth_header_t)), buf, length);

	switch(type) {
		case NETIF_INET:
			ethpkt->type = ETH_TYPE_IPV4;
			break;
		case NETIF_INET6:
			ethpkt->type = ETH_TYPE_IPV6;
			break;
		case NETIF_ARP:
			ethpkt->type = ETH_TYPE_ARP;
			break;
		default:
			ethpkt->type = ETH_TYPE_RAW;
			break;
	}

	
	length += sizeof(eth_header_t);

	
	int ret = netif->send(netif, ethpkt, length, NETIF_ETH);
	kfree(ethpkt);
	
	return ret;
}

int eth_send(netif_t* netif, void* buf, size_t length, int type) {

	if(length + sizeof(eth_header_t) < netif->mtu) {
		if(eth_send_packet(netif, buf, length, type) > 0)
			return length;
		else
			return 0;
	}

	int delta = netif->mtu - sizeof(eth_header_t);
	int ret = 0;


	for(int i = 0; i < length; i += delta)
		ret += eth_send_packet(netif, (void*) ((uint32_t) buf + i), delta, type);

	if((length % delta) != 0)
		ret += eth_send_packet(netif, (void*) ((uint32_t) buf + length - (length % delta)), length % delta, type);
	
	
	if(ret)
		return length;
	
	return 0;
}



