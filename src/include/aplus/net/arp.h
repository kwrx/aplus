#ifndef _ARP_H
#define _ARP_H

#include <stdint.h>
#include <aplus/netif.h>
#include <aplus/net/eth.h>

#include <time.h>



#define ARP_PRTYPE				ETH_TYPE_IPV4
#define ARP_PRLEN				4

#define ARP_HWTYPE				0x0001
#define ARP_HWLEN				6

#define ARP_OPERATION_REQUEST	1
#define ARP_OPERATION_REPLY		2

#define ARP_TTL					600		/* 10min */




typedef struct arp_header {
	uint16_t hwtype;
	uint16_t prtype;
	uint8_t hwlen;
	uint8_t prlen;
	uint16_t operation;
	macaddr_t sha;
	ipv4_t spa;
	macaddr_t tha;
	ipv4_t tpa;
} __attribute__((packed)) arp_header_t;


typedef struct arp_cache_entry {
	ipv4_t ipv4;
	macaddr_t macaddr;

	time_t ttl;
} arp_cache_entry_t;

#endif
