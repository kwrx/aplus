#include <aplus.h>
#include <aplus/list.h>
#include <aplus/netif.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <aplus/net/eth.h>

int eth_recv(netif_t* netif, void* buf, size_t length, int type) {
	eth_header_t* ethpkt = kmalloc(length + sizeof(eth_header_t));
	int ret = 0;

	if((ret = netif->recv(netif, ethpkt, length + sizeof(eth_header_t), NETIF_ETH)) == 0) {
		kfree(ethpkt);		
		return 0;
	}
	
	length = ret - sizeof(eth_header_t);
	
	memcpy(buf, (void*) ((uint32_t) ethpkt + sizeof(eth_header_t)), length);
	kfree(ethpkt);


	return length;
}

int eth_send(netif_t* netif, void* buf, size_t length, int type) {

	if(type == NETIF_ETH)
		return netif->send(netif, buf, length, NETIF_ETH);
	
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

	int ret = netif->send(netif, ethpkt, length + sizeof(eth_header_t), NETIF_ETH);
	kfree(ethpkt);
	
	return ret;
}



