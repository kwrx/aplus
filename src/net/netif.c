#include <aplus.h>
#include <aplus/list.h>
#include <aplus/netif.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>


static list_t* lst_netif = NULL;


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

	return list_add(lst_netif, (listval_t) netdev);
}

int netif_remove(netif_t* netdev) {
	if(lst_netif == NULL)
		return -1;

	return list_remove(lst_netif, (listval_t) netdev);
}

