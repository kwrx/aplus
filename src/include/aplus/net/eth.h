#ifndef _ETH_H
#define _ETH_H

#include <stdint.h>
#include <aplus/netif.h>

#define ETH_TYPE_IPV4			0x0800
#define ETH_TYPE_ARP			0x0806
#define ETH_TYPE_IPV6			0x86DD
#define ETH_TYPE_WAKEONLAN		0x0842
#define ETH_TYPE_RARP			0x8035
#define ETH_TYPE_LOOPBACK		0x9000
#define ETH_TYPE_RAW			0xFFFF


typedef struct eth {
	macaddr_t dest;
	macaddr_t source;
	uint16_t type;
	char data[0];
} __attribute__((packed)) eth_t;


struct sockaddr_eth {
	uint16_t set_family;
	struct {
		macaddr_t eth_addr;
	} set_addr;
	char set_zero[8];
};

#endif
