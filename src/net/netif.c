#include <aplus.h>
#include <aplus/list.h>

#define _NO_STACK
#define _NO_PROTO
#define _NO_BRDCST
#include <aplus/netif.h>
#include <aplus/attribute.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>


netif_protocol_t* netif_protocols[NETIF_MAX_PROTOCOLS] = { 0 };

list_t* lst_netif = NULL;
list_t* netif_stack[NETIF_MAX_PROTOCOLS] = { 0 };



macaddr_t __netif_macaddr_broadcast = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };


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

		netif->flags |= NETIF_FLAGS_ENABLE;
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

		netif->flags &= ~NETIF_FLAGS_ENABLE;
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

#ifdef DEBUG
	kprintf("netif: loaded interface \"%s\"\n", netdev->name);
#endif

#ifdef NETIF_DEBUG
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
#endif

	return list_add(lst_netif, (listval_t) netdev);
}

int netif_remove(netif_t* netdev) {
	if(lst_netif == NULL)
		return -1;

	return list_remove(lst_netif, (listval_t) netdev);
}

int netif_init() {
	memset(netif_protocols, 0, sizeof(netif_protocol_t*) * NETIF_MAX_PROTOCOLS);
	memset(netif_stack, 0, sizeof(list_t*) * NETIF_MAX_PROTOCOLS);

	list_t* lst = attribute("netif");
	if(list_empty(lst))
		return -1;

	list_foreach(value, lst) {
		int (*handler) () = (int (*) ()) value;

		if(handler)
			handler();
	}
	
	list_destroy(lst);


	for(int i = 0; i < NETIF_MAX_PROTOCOLS; i++)
		{ list_init(netif_stack[i]); }


	lst = attribute("protocols");
	if(list_empty(lst))
		goto done;

	list_foreach(value, lst) {
		netif_protocols[((netif_protocol_t*) value)->id] = (netif_protocol_t*) value;

#ifdef NETIF_DEBUG
		kprintf("netif: registered protocol \"%s\" (%d)\n", ((netif_protocol_t*) value)->name, ((netif_protocol_t*) value)->id);
#endif	
	}

done:
	netif_ifup();
	return 0;
}


uint64_t netif_packet_push(uint16_t proto, void* header, uint32_t hlen, void* data, uint32_t dlen) {
	if(unlikely(!header || !data)) {
		errno = EINVAL;	
		return -1;
	}

	
	netif_packet_t* p = (netif_packet_t*) kmalloc(sizeof(netif_packet_t) + hlen + dlen);
	if(unlikely(!p)) {
		errno = ENOMEM;
		return -1;
	}


	static uint64_t packet_id = 1;
	static spinlock_t p_lock = 0;

	spinlock_lock(&p_lock);
	
	p->id = packet_id++;
	p->proto = proto;
	p->header.length = hlen;
	p->header.ptr = (void*) ((uint32_t) p + sizeof(netif_packet_t));
	p->data.length = dlen;
	p->data.ptr = (void*) ((uint32_t) p + sizeof(netif_packet_t) + hlen);



	memcpy(p->header.ptr, header, hlen);
	memcpy(p->data.ptr, data, dlen);


	list_add(netif_stack[proto], (listval_t) p);

	spinlock_unlock(&p_lock);
	return p->id;
}

int netif_packet_pop(netif_packet_t* p) {
	if(unlikely(!p)) {
		errno = EINVAL;	
		return -1;
	}
	
	return list_remove(netif_stack[p->proto], (listval_t) p);
}


netif_t* netif_get_interface(int index) {
	return (netif_t*) list_at_index(lst_netif, index);
}


