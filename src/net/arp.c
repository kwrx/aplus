#include <aplus.h>
#include <aplus/list.h>
#include <aplus/netif.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <aplus/net/eth.h>
#include <aplus/net/arp.h>


list_t* arp_cache = NULL;


arp_cache_entry_t* arp_cache_find_by_macaddr(macaddr_t macaddr) {
	if(arp_cache == NULL)
		return NULL;

	if(list_empty(arp_cache))
		return NULL;

	list_foreach(value, arp_cache) {
		arp_cache_entry_t* arp = (arp_cache_entry_t*) value;

		if(memcmp(arp->macaddr, macaddr, sizeof(macaddr_t)) == 0)
			return arp;
	}

	return NULL;
}

int arp_cache_add(macaddr_t macaddr, ipv4_t ipv4) {
	if(arp_cache == NULL) {
		list_init(arp_cache);
	}


	arp_cache_entry_t* arp = arp_cache_find_by_macaddr(macaddr);
	if(arp)
		list_remove(arp_cache, (listval_t) arp);


	arp = kmalloc(sizeof(arp_cache_entry_t));
	memcpy(arp->macaddr, macaddr, sizeof(macaddr_t));
	memcpy(arp->ipv4, ipv4, sizeof(ipv4_t));

	//arp->ttl = time(NULL) + ARP_TTL;


#ifdef ARP_DEBUG
	kprintf("arp: added new cache entry (%02x.%02x.%02x.%02x)\n",
		arp->ipv4[0],
		arp->ipv4[1],
		arp->ipv4[2],
		arp->ipv4[3]
	);
#endif

	netif_t* netif = (netif_t*) netif_find_by_macaddr(arp->macaddr);
	if(netif)
		memcpy(netif->ipv4, arp->ipv4, sizeof(ipv4_t));

	return list_add(arp_cache, (listval_t) arp);
}

int arp_recv(netif_t* netif, void* buf, size_t length) {
	arp_header_t* arp = (arp_header_t*) buf;


	if(arp->operation == ARP_OPERATION_REPLY)
		if(memcmp(netif->macaddr, arp->tha, sizeof(macaddr_t)) == 0)
			arp_cache_add(arp->tha, arp->tpa);

	return length;
}


int arp_send(netif_t* netif) {
	arp_header_t* arp = (arp_header_t*) kmalloc(sizeof(arp_header_t));
	arp->hwtype = ARP_HWTYPE;
	arp->prtype = ARP_PRTYPE;
	arp->hwlen = ARP_HWLEN;
	arp->prlen = ARP_PRLEN;
	arp->operation = ARP_OPERATION_REQUEST;
	
	memcpy(arp->sha, netif->macaddr, sizeof(macaddr_t));
	memset(arp->tha, 0xFF, sizeof(macaddr_t));
	memset(arp->spa, 0, sizeof(ipv4_t));
	memset(arp->tpa, 0, sizeof(ipv4_t));

#ifdef ARP_DEBUG
	kprintf("arp: sending request for %s\n", netif->name);
#endif

	int ret = eth_send(netif, arp, sizeof(arp_header_t), NETIF_ARP);
	kfree(arp);
	
	return ret;
}
