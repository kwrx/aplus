#include <aplus.h>
#include <aplus/list.h>
#include <aplus/netif.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <aplus/net/eth.h>
#include <aplus/net/udp.h>

int udp_recv(netif_t* netif, void* buf, size_t length) {
	
	netif_packets_add (
		netif_packets_create (
							netif,
							NETIF_UDP, 
							length, 
							sizeof(udp_header_t), 
							buf
		)
	);

	return length;
}

/* TODO: udp_send */
