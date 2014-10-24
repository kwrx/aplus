#ifndef _ETH_H
#define _ETH_H

#include <stdint.h>
#include <aplus/netif.h>

/* ETH:
 * 	[dest] [source] [type] [data...] [crc32] 
*/


#define ETH_TYPE_IPV4			0x0800
#define ETH_TYPE_ARP			0x0806
#define ETH_TYPE_IPV6			0x86DD
#define ETH_TYPE_RAW			0xFFFF


typedef struct eth_header {
	macaddr_t dest[6];
	macaddr_t source[6];
	uint16_t type;
} eth_header_t;

#endif
