#include <aplus.h>
#include <aplus/list.h>
#include <aplus/netif.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <aplus/net/eth.h>
#include <aplus/net/ipv4.h>


uint16_t ipv4_checksum(ipv4_header_t* pkt) {
	int length = sizeof(ipv4_header_t);
	int sum = 0;
	uint8_t* buf = (uint8_t*) pkt;

	while(length > 1) {
		sum += 0xFFFF & *((uint16_t*) buf);
		buf += sizeof(uint16_t);
		length -= sizeof(uint16_t);
	}

	if(length)
		sum += (0xFF & *buf) << 8;

	while(sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	uint16_t cksum = ((uint16_t) sum ^ 0xFFFF);
	return (((cksum & 0x00FF) << 8) | ((cksum & 0xFF00) >> 8)) & 0xFFFF;
}

void* ipv4_create_packet(netif_t* netif, void* buf, size_t length, int type, ipv4_t dest, int fragment, int offset) {

	ipv4_header_t* ip = (ipv4_header_t*) kmalloc(length + sizeof(ipv4_header_t));
	ip->info = (((sizeof(ipv4_header_t) >> 2) & 0xF) << 4) | 4;	/* length: 20 byte; Version: ipv4; */
	ip->tos = 0;
	ip->length = length + sizeof(ipv4_header_t);
	ip->id = 0;
	ip->ttl = 0xFF;

	if(fragment == -1)
		ip->offset = offset;
	else
		ip->offset = IPV4_FLAGS_MF | (offset * IPV4_MAX_LENGTH);

	switch(type) {
		case NETIF_UDP:
			ip->protocol = IPV4_PROTO_UDP;
			break;
		case NETIF_TCP:
			ip->protocol = IPV4_PROTO_TCP;
			break;
		case NETIF_ICMP:
			ip->protocol = IPV4_PROTO_ICMP;
			break;
		default:
			ip->protocol = IPV4_PROTO_RAW;
			break;
	}

	memcpy(ip->source, netif->ipv4, sizeof(ipv4_t));
	memcpy(ip->dest, dest, sizeof(ipv4_t));
	memcpy((void*) ((uint32_t) ip + sizeof(ipv4_header_t)), buf, length);

	ip->checksum = ipv4_checksum(ip);
	return (void*) ip;
}

int ipv4_recv(netif_t* netif, void* buf, size_t length) {
	ipv4_header_t* ip = (ipv4_header_t*) buf;

	if(IPV4_CHECK_VERSION(ip) == 0)
		return 0;

	if(IPV4_FLAGS(ip) & IPV4_FLAGS_MF)
		panic("ipv4: packet fragmentation not supported in recv mode");


	#define __params	\
		netif, (void*) ((uint32_t) ip + IPV4_HEADER_LENGTH(ip)), length - IPV4_HEADER_LENGTH(ip)

	switch(ip->protocol) {
		case IPV4_PROTO_UDP:
			if(udp_recv(__params) == 0)
				return 0;
			return length;
		
		case IPV4_PROTO_TCP:
			//if(tcp_recv(__params) == 0)		/* Support for TCP ?? -> pfff.. ù.ù */
			//	return 0;
			return length;

		case IPV4_PROTO_ICMP:
			//if(icmp_recv(__params) == 0)
			//	return 0;
			return length;
	}

	/* IPV4_PROTO_RAW */
	netif_packets_add (
		netif_packets_create (
							NETIF_INET, 
							length, 
							IPV4_HEADER_LENGTH(ip), 
							buf
		)
	);

	kprintf("ok\n");
	return length;
}

int ipv4_send(netif_t* netif, void* buf, size_t length, int type, ipv4_t dest) {
	if(length < IPV4_MAX_LENGTH) {
		void* pkt = ipv4_create_packet(netif, buf, length, type, dest, -1, 0);
		int ret = eth_send(netif, pkt, length + sizeof(ipv4_header_t), NETIF_INET);
		kfree(pkt);

		if(ret)
			return length;
	
		return 0;
	}

	int ret = 0;
	int i = 0;
	int max = length / IPV4_MAX_LENGTH;

	if(length % IPV4_MAX_LENGTH == 0)
		max -= 1;

	for(i = 0; i < max; i++) {
		void* pkt = ipv4_create_packet(netif, (void*) ((uint32_t) buf + (i * IPV4_MAX_LENGTH)), IPV4_MAX_LENGTH, type, dest, 0, i);
		ret += eth_send(netif, pkt, IPV4_MAX_LENGTH, NETIF_INET);
		kfree(pkt);
	}


	void* pkt = ipv4_create_packet(netif, (void*) ((uint32_t) buf + (i * IPV4_MAX_LENGTH)), length - ((i - 1) * IPV4_MAX_LENGTH), type, dest, -1, i);
	ret += eth_send(netif, pkt, IPV4_MAX_LENGTH, NETIF_INET);
	kfree(pkt);

	if(ret)
		return length;
	
	return 0;
}

