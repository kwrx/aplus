#include <aplus.h>
#include <aplus/list.h>
#include <aplus/netif.h>
#include <aplus/attribute.h>

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

int netif_add(netif_t* netdev) {
	if(lst_netif == NULL) {
		list_init(lst_netif);
	}

	if(netif_find_by_ipv4(&netdev->ipv4) != NULL) {
		kprintf("netif: conflitto di ipv4\n");
		return -1;
	}


	if(netif_find_by_macaddr(&netdev->macaddr) != NULL) {
		kprintf("netif: conflitto di macaddr\n");
		return -1;
	}


	kprintf("%s:\tipv4 %d.%d.%d.%d, netmask: %d.%d.%d.%d\n",
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

	kprintf("\tmacaddr %02x:%02x:%02x:%02x:%02x:%02x, mtu %d\n",
			netdev->macaddr[0],
			netdev->macaddr[1],
			netdev->macaddr[2],
			netdev->macaddr[3],
			netdev->macaddr[4],
			netdev->macaddr[5],
			netdev->mtu
	);

	return list_add(lst_netif, (listval_t) netdev);
}

int netif_remove(netif_t* netdev) {
	if(lst_netif == NULL)
		return -1;

	return list_remove(lst_netif, (listval_t) netdev);
}

int netif_init() {
	list_t* lst_netif = attribute("netif");
	if(list_empty(lst_netif))
		return -1;

	list_foreach(value, lst_netif) {
		int (*handler) () = (int (*) ()) value;

		if(handler)
			handler();
	}
	
	list_destroy(lst_netif);
	return 0;
}

