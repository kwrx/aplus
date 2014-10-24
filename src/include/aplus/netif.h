#ifndef _NETIF_H
#define _NETIF_H

#include <aplus.h>
#include <aplus/pci.h>
#include <stdint.h>
#include <stddef.h>


typedef uint8_t ipv4_t[4];
typedef uint16_t ipv6_t[8];
typedef uint8_t macaddr_t[6];


#define NETIF_INET			1
#define NETIF_INET6			2
#define NETIF_ARP			3
#define NETIF_ETH			4
#define NETIF_UDP			5
#define NETIF_TCP			6
#define NETIF_ICMP			7
#define NETIF_TELNET		8


typedef struct netif {
	char name[32];
	void* data;
	
	ipv6_t ipv6;
	ipv4_t ipv4;
	ipv4_t netmask;	
	macaddr_t macaddr;

	struct {
		uint64_t rx_packets;
		uint64_t rx_bytes;
		uint64_t rx_errors;
		uint64_t rx_dropped;

		uint64_t tx_packets;
		uint64_t tx_bytes;
		uint64_t tx_errors;
		uint64_t tx_dropped;
	} state;

	int (*send) (struct netif*, void*, size_t, int);
	int (*recv) (struct netif*, void*, size_t, int);

	uint32_t mtu;

	pci_device_t* device;
} netif_t;

#endif
