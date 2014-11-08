#include <aplus.h>
#include <aplus/list.h>
#include <aplus/netif.h>
#include <aplus/attribute.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <aplus/net/arp.h>


static list_t* lst_netif = NULL;
static list_t* lst_packets = NULL;



uint32_t netif_htonl(uint32_t h) {
	return ((h & 0xFF000000) >> 24) | ((h & 0x000000FF) << 24) |
			((h & 0x00FF0000) >> 8) | ((h & 0x0000FF00) << 8);
}

uint16_t netif_htons(uint16_t h) {
	return ((h & 0xFF00) >> 8) | ((h & 0x00FF) << 8);
}

uint32_t netif_ntohl(uint32_t h) {
	return ((h & 0xFF000000) >> 24) | ((h & 0x000000FF) << 24) |
			((h & 0x00FF0000) >> 8) | ((h & 0x0000FF00) << 8);
}

uint32_t netif_ntohs(uint32_t h) {
	return ((h & 0xFF00) >> 8) | ((h & 0x00FF) << 8);
}

netif_t* netif_find_by_ipv4(ipv4_t* ipv4) {
	if(lst_netif == NULL)
		return NULL;

	list_foreach(value, lst_netif) {
		netif_t* netif = (netif_t*) value;

		if(memcmp(netif->ipv4, ipv4, sizeof(ipv4_t)) == 0)
			return netif;
	}

	return NULL;
}

netif_t* netif_find_by_ipv6(ipv6_t* ipv6) {
	if(lst_netif == NULL)
		return NULL;

	list_foreach(value, lst_netif) {
		netif_t* netif = (netif_t*) value;

		if(memcmp(netif->ipv6, ipv6, sizeof(ipv6_t)) == 0)
			return netif;
	}

	return NULL;
}



netif_t* netif_find_by_macaddr(macaddr_t* macaddr) {
	if(lst_netif == NULL)
		return NULL;

	list_foreach(value, lst_netif) {
		netif_t* netif = (netif_t*) value;

		if(memcmp(netif->macaddr, macaddr, sizeof(macaddr_t)) == 0)
			return netif;
	}

	return NULL;
}

netif_t* netif_find_by_name(char* name) {
	if(lst_netif == NULL)
		return NULL;

	list_foreach(value, lst_netif) {
		netif_t* netif = (netif_t*) value;

		if(strcmp(netif->name, name) == 0)
			return netif;
	}

	return NULL;
}

int netif_ifup() {
	if(lst_netif == NULL)
		return -1;

	list_foreach(value, lst_netif) {
		netif_t* netif = (netif_t*) value;

		if(netif->ifup)
			netif->ifup(netif);

		arp_send(netif);
	}

	return -1;
}

int netif_ifdown() {
	if(lst_netif == NULL)
		return -1;

	list_foreach(value, lst_netif) {
		netif_t* netif = (netif_t*) value;

		if(netif->ifdown)
			netif->ifdown(netif);
	}

	return -1;
}

int netif_add(netif_t* netdev) {
	if(lst_netif == NULL) {
		list_init(lst_netif);
	}


	if(netif_find_by_ipv4(&netdev->ipv4) != NULL) {
		kprintf("netif: conflitto di ipv4\n");
		return -1;
	}

	if(netif_find_by_ipv6(&netdev->ipv6) != NULL) {
		kprintf("netif: conflitto di ipv6\n");
		return -1;
	}


	if(netif_find_by_macaddr(&netdev->macaddr) != NULL) {
		kprintf("netif: conflitto di macaddr\n");
		return -1;
	}



	kprintf("\n%s:\tipv4\t%d.%d.%d.%d\n\tnetmask\t%d.%d.%d.%d\n",
			netdev->name,
			netdev->ipv4[0],
			netdev->ipv4[1],
			netdev->ipv4[2],
			netdev->ipv4[3],
			netdev->netmask[0],
			netdev->netmask[1],
			netdev->netmask[2],
			netdev->netmask[3]
	);

	kprintf("\tipv6\t%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
			netdev->ipv6[0],
			netdev->ipv6[1],
			netdev->ipv6[2],
			netdev->ipv6[3],
			netdev->ipv6[4],
			netdev->ipv6[5],
			netdev->ipv6[6],
			netdev->ipv6[7]
	);

	kprintf("\tmacaddr\t%02x:%02x:%02x:%02x:%02x:%02x\n\tmtu\t%d bytes\n",
			netdev->macaddr[0],
			netdev->macaddr[1],
			netdev->macaddr[2],
			netdev->macaddr[3],
			netdev->macaddr[4],
			netdev->macaddr[5],
			netdev->mtu
	);

	kprintf("\tdns\t%d.%d.%d.%d\n\t\t%d.%d.%d.%d\n",
			netdev->dns.primary.ipv4[0],
			netdev->dns.primary.ipv4[1],
			netdev->dns.primary.ipv4[2],
			netdev->dns.primary.ipv4[3],
			netdev->dns.secondary.ipv4[0],
			netdev->dns.secondary.ipv4[1],
			netdev->dns.secondary.ipv4[2],
			netdev->dns.secondary.ipv4[3]
	);

	kprintf("\t\t%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n\t\t%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
			netdev->dns.primary.ipv6[0],
			netdev->dns.primary.ipv6[1],
			netdev->dns.primary.ipv6[2],
			netdev->dns.primary.ipv6[3],
			netdev->dns.primary.ipv6[4],
			netdev->dns.primary.ipv6[5],
			netdev->dns.primary.ipv6[6],
			netdev->dns.primary.ipv6[7],
			netdev->dns.secondary.ipv6[0],
			netdev->dns.secondary.ipv6[1],
			netdev->dns.secondary.ipv6[2],
			netdev->dns.secondary.ipv6[3],
			netdev->dns.secondary.ipv6[4],
			netdev->dns.secondary.ipv6[5],
			netdev->dns.secondary.ipv6[6],
			netdev->dns.secondary.ipv6[7]
	);

	return list_add(lst_netif, (listval_t) netdev);
}

int netif_remove(netif_t* netdev) {
	if(lst_netif == NULL)
		return -1;

	return list_remove(lst_netif, (listval_t) netdev);
}

int netif_init() {
	list_init(lst_packets);

	list_t* lst_netif = attribute("netif");
	if(list_empty(lst_netif))
		return -1;

	list_foreach(value, lst_netif) {
		int (*handler) () = (int (*) ()) value;

		if(handler)
			handler();
	}
	
	list_destroy(lst_netif);

	netif_ifup();
	return 0;
}


int netif_packets_add(netif_packet_t* packet) {
	static uint64_t nextid = 0;
	
	packet->id = nextid++;
	return list_add(lst_packets, (listval_t) packet);
}

int netif_packets_remove(netif_packet_t* packet) {
	return list_remove(lst_packets, (listval_t) packet);
}

netif_packet_t* netif_packets_find_by_id(uint64_t id) {
	list_foreach(value, lst_packets) {
		netif_packet_t* pkt = (netif_packet_t*) value;

		if(pkt->id == id)
			return pkt;
	}

	return NULL;
}

list_t* netif_packets_find_by_protocol(int protocol) {
	list_t* tmp = NULL;
	list_init(tmp);


	list_foreach(value, lst_packets) {
		netif_packet_t* pkt = (netif_packet_t*) value;

		if(pkt->protocol == protocol)
			list_add(tmp, (listval_t) pkt);
	}

	return tmp;
}

netif_packet_t* netif_packets_create(netif_t* netif, int protocol, int tot_length, int head_length, void* data) {
	netif_packet_t* pkt = (netif_packet_t*) kmalloc(sizeof(netif_packet_t) + tot_length);
	pkt->netif = netif;
	pkt->protocol = protocol;
	pkt->length = tot_length - head_length;
	
	memcpy(pkt->header, data, head_length);
	memcpy(pkt->data, (void*) ((uint32_t) data + head_length), pkt->length);

	return pkt;
}

