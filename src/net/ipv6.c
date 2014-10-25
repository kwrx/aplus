#include <aplus.h>
#include <aplus/list.h>
#include <aplus/netif.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <aplus/net/eth.h>
#include <aplus/net/ipv6.h>



int ipv6_recv(netif_t* netif, void* buf, size_t length, int type) {
	ipv6_header_t* ip = (ipv6_header_t*) buf;
	
	if(IPV6_CHECK_VERSION(ip) == 0)
		return 0;


	#define __params	\
		netif, (void*) ((uint32_t) ip + sizeof(ipv6_header_t)), length - sizeof(ipv6_header_t)

	switch(ip->protocol) {
		case IPV6_PROTO_UDP:
			//if(udp_recv(__params) == 0)
			//	return 0;
			return length;
		
		case IPV6_PROTO_TCP:
			//if(tcp_recv(__params) == 0)		/* Support for TCP ?? -> pfff.. ù.ù */
			//	return 0;
			return length;

		case IPV6_PROTO_ICMP:
			//if(icmp_recv(__params) == 0)
			//	return 0;
			return length;
	}

	/* IPV6_PROTO_RAW */
	netif_packets_add (
		netif_packets_create (
							NETIF_INET6, 
							length, 
							sizeof(ipv6_header_t), 
							buf
		)
	);

	return length;
}

int ipv6_send(netif_t* netif, void* buf, size_t length, int type, ipv6_t dest) {
	ipv6_header_t* ip = (ipv6_header_t*) kmalloc(length + sizeof(ipv6_header_t));
	ip->version = 6;
	ip->traffic = 0;
	ip->flow = 0;
	ip->length = length;
	ip->ttl = 0xFF;
	
	switch(type) {
		case NETIF_UDP:
			ip->protocol = IPV6_PROTO_UDP;
			break;
		case NETIF_TCP:
			ip->protocol = IPV6_PROTO_TCP;
			break;
		case NETIF_ICMP:
			ip->protocol = IPV6_PROTO_ICMP;
			break;
		default:
			ip->protocol = IPV6_PROTO_RAW;
			break;
	}

	memcpy(ip->dest, dest, sizeof(ipv6_t));
	memcpy(ip->source, netif->ipv6, sizeof(ipv6_t));
	memcpy((void*) ((uint32_t) ip + sizeof(ipv6_header_t)), buf, length);

	int ret = eth_send(netif, (void*) ip, length + sizeof(ipv6_header_t), NETIF_INET6);
	kfree(ip);

	if(ret)
		return length;
	
	return 0;
}
