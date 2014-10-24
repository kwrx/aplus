#ifndef _NETIF_H
#define _NETIF_H

#include <aplus.h>
#include <aplus/pci.h>
#include <stdint.h>
#include <stddef.h>


typedef uint8_t ipv4_t[4];
typedef uint8_t ipv6_t[6];
typedef uint8_t macaddr_t[6];



typedef struct netif {
	char name[64];
	void* data;
	
	ipv4_t ipv4;
	ipv6_t ipv6;
	macaddr_t macaddr;

	uint32_t pkt_recv_count;
	uint32_t pkt_send_count;

	int errno;

	int (*send) (struct netif*, void*, size_t);
	int (*recv) (struct netif*, void*, size_t);

	pci_device_t* device;
} netif_t;

#endif
