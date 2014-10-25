#ifndef _IPV6_H
#define _IPV6_H

#include <stdint.h>
#include <aplus/netif.h>


#define IPV6_CHECK_VERSION(head)		(head->version == 6)

#define IPV6_PROTO_RAW					0
#define IPV6_PROTO_ICMP					1
#define IPV6_PROTO_TCP					6
#define IPV6_PROTO_UDP					17

typedef struct ipv6_header {
	uint32_t version:4;
	uint32_t traffic:8;
	uint32_t flow:20;

	uint16_t length;
	uint8_t protocol;
	uint8_t ttl;

	ipv6_t source;
	ipv6_t dest;
} __attribute__((packed)) ipv6_header_t;

#endif
