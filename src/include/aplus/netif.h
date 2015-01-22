#ifndef _NETIF_H
#define _NETIF_H

#include <aplus.h>
#include <aplus/pci.h>
#include <aplus/attribute.h>
#include <stdint.h>
#include <stddef.h>


typedef uint8_t ipv4_t[4];
typedef uint16_t ipv6_t[8];
typedef uint8_t macaddr_t[6];


#define NETIF_RAW					0
#define NETIF_INET					1
#define NETIF_INET6					2
#define NETIF_ARP					3
#define NETIF_ETH					4
#define NETIF_UDP					5
#define NETIF_TCP					6
#define NETIF_ICMP					7
#define NETIF_TELNET				8


#define NETIF_FLAGS_ENABLE			1

#define NETIF_MAX_PROTOCOLS			32


#ifndef AF_INET
#define AF_INET						1
#endif

#ifndef AF_UNIX
#define AF_UNIX						2
#endif

#ifndef AF_LOCAL
#define AF_LOCAL					AF_UNIX
#endif

#ifndef AF_ISO
#define AF_ISO						AF_UNIX
#endif


struct sockaddr {
	uint16_t sa_family;
	char sa_data[14];
};


typedef struct netif {
	char name[32];
	void* data;
	
	ipv6_t ipv6;
	ipv4_t ipv4;
	ipv4_t netmask;	
	macaddr_t macaddr;

	struct {
		struct {
			ipv4_t ipv4;
			ipv6_t ipv6;
		} primary;

		struct {
			ipv4_t ipv4;
			ipv6_t ipv6;
		} secondary;
	} dns;

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
	int (*ifup)	(struct netif*);
	int (*ifdown) (struct netif*);

	uint32_t mtu;
	uint32_t flags;

	pci_device_t* device;
} netif_t;

typedef struct netif_packet {
	uint64_t id;
	uint16_t proto;

	struct {
		void* ptr;
		uint32_t length;
	} header;

	struct {
		void* ptr;
		uint32_t length;
		uint32_t offset;
	} data;

} netif_packet_t;


typedef struct netif_socket {
	int id;
	int domain;
	int type;
	int proto;

	struct sockaddr sockaddr;

	int (*send) (struct netif_socket*, void*, size_t);
	int (*check) (struct netif_socket*, struct netif_packet*);
	int (*close) (struct netif_socket*, int);

	netif_t* netif;
} netif_socket_t;


typedef struct netif_protocol {
	int domain;
	int id;
	char* name;	

	int (*send) (struct netif_socket*, void*, size_t);
	int (*check) (struct netif_socket*, struct netif_packet*);
	int (*close) (struct netif_socket*, int);
} netif_protocol_t;


#define netif_htons(x)			__builtin_bswap16(x)
#define netif_htonl(x)			__builtin_bswap32(x)
#define netif_htonll(x)			__builtin_bswap64(x)

#define netif_ntohs(x)			netif_htons(x)
#define netif_ntohl(x)			netif_htonl(x)
#define netif_ntohll(x)			netif_htonll(x)


#ifndef _NO_BRDCST
extern macaddr_t __netif_macaddr_broadcast;
#endif

#define MACADDR_BROADCAST		__netif_macaddr_broadcast


#define NETIF_PROTO(dom, id, n)								\
	static netif_protocol_t netif_proto_##n = {				\
		dom, id, #n, n##_send, n##_check, n##_close			\
	}; ATTRIBUTE("protocols", netif_proto_##n)






#ifndef _NO_STACK
extern list_t* netif_stack[];
#endif

#ifndef _NO_PROTO
extern netif_protocol_t* netif_protocols[];
#endif


#endif
