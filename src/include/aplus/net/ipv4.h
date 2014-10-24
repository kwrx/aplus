#ifndef _IPV4_H
#define _IPV4_h

#include <stdint.h>
#include <aplus/netif.h>


#define IPV4_FLAGS_DF		0x01	/* Don't fragment */
#define IPV4_FLAGS_MF		0x02	/* More fragment */

#define IPV4_CHECK_VERSION(head)		((head->info & 0x0F) == 4)
#define IPV4_HEADER_LENGTH(head)		(((head->info & 0xF0) >> 4) * 4)
#define IPV4_FLAGS(head)				(head->offset & 3)
#define IPV4_OFFSET(head)				((head->offset >> 2) * 8)

#define IPV4_TOS_PRIORITY(tos)			(tos & 3)
#define IPV4_TOS_LATENCY(tos)			((tos >> 2) & 1)
#define IPV4_TOS_TROUGHPUT(tos)			((tos >> 3) & 1)
#define IPV4_TOS_RELIABILITY(tos)		((tos >> 4) & 1)


#define IPV4_PROTO_RAW					0
#define IPV4_PROTO_ICMP					1
#define IPV4_PROTO_TCP					6
#define IPV4_PROTO_TELNET				14
#define IPV4_PROTO_UDP					17

#define IPV4_MAX_LENGTH					65536


typedef struct ipv4_header {
	uint8_t info;
	uint8_t tos;
	uint16_t length;
	uint16_t id;
	uint16_t offset;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t checksum;
	
	ipv4_t source;
	ipv4_t dest;
	
} ipv4_header_t;

#endif
