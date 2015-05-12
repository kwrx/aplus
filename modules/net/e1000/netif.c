#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include <lwip/tcpip.h>
#include "netif/etharp.h"
#include "netif/ppp_oe.h"

#include <aplus.h>
#include <aplus/spinlock.h>

#include "e1000.h"

#define E1000	e1000_t* e1000 = (e1000_t*) netif->state

static void ll_init(struct netif* netif) {
	E1000;

	netif->hwaddr_len = ETHARP_HWADDR_LEN;
	memcpy(netif->hwaddr, e1000->macaddr, ETHARP_HWADDR_LEN);

	netif->mtu = 1500;
	netif->flags = 	NETIF_FLAG_BROADCAST 	|
#ifdef E1000_DHCP
					NETIF_FLAG_DHCP			|
#endif
					NETIF_FLAG_ETHARP;

	
}


static err_t ll_output(struct netif* netif, struct pbuf* p) {
	E1000;
	struct pbuf* q;

#if ETH_PAD_SIZE
	pbuf_header(p, -ETH_PAD_SIZE);
#endif

	for(q = p; q; q = q->next)
		e1000_sendpacket(e1000, q->payload, q->len);

#if ETH_PAD_SIZE
	pbuf_header(p, ETH_PAD_SIZE);
#endif

	LINK_STATS_INC(link.xmit);
	return ERR_OK;
}

static struct pbuf* ll_input(struct netif* netif, void* data, u16_t len) {
	E1000;
	struct pbuf* p;
	struct pbuf* q;

#if ETH_PAD_SIZE
	len += ETH_PAD_SIZE;
#endif

	p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
	if(p != NULL) {
#if ETH_PAD_SIZE
		pbuf_header(p, -ETH_PAD_SIZE);
#endif

		uintptr_t cp = 0;
		for(q = p; q; q = q->next) {
			memcpy(q->payload, (void*) ((uintptr_t) data + cp), q->len);
			cp += q->len;
		}

#if ETH_PAD_SIZE
		pbuf_header(p, ETH_PAD_SIZE);
#endif

		LINK_STATS_INC(link.recv);

	} else {
		LINK_STATS_INC(link.memerr);
		LINK_STATS_INC(link.drop);

#ifdef E1000_DEBUG
		LWIP_DEBUGF(NETIF_DEBUG, ("e1000: recv error, cannot allocate pbuf!\n"));
#endif
	}

	return p;
}


void if_input(struct netif* netif, void* data, u32_t len) {
	E1000;
	struct pbuf* p;

	p = ll_input(netif, data, len);
	if(p == NULL)
		return;

	struct eth_hdr* ethhdr = (struct eth_hdr*) p->payload;
	switch(htons(ethhdr->type)) {
		case ETHTYPE_IP:
		case ETHTYPE_ARP:
#if PPPOE_SUPPORT
		case ETHTYPE_PPPOEDISC:
		case ETHTYPE_PPPOE:
#endif
			if(netif->input(p, netif) != ERR_OK) {
				LWIP_DEBUGF(NETIF_DEBUG, ("e1000: IP input error\n"));
				pbuf_free(p);
	
				p = NULL;
			}
			break;
	default:
		pbuf_free(p);
		p = NULL;
		break;
	}
}


static err_t if_init(struct netif* netif) {
	E1000;

#if LWIP_NETIF_HOSTNAME
	netif->hostname = "lwip";
#endif

	NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 100 * 1024 * 1024);

	netif->name[0] = 'e';
	netif->name[1] = 't';

	netif->output = etharp_output;
	netif->linkoutput = ll_output;

	ll_init(netif);

	return ERR_OK;
}


struct netif* e1000_netif_init(e1000_t* e1000) {

	ip_addr_t ip;
	ip_addr_t nm;
	ip_addr_t gw;

	IP4_ADDR(&ip, 192, 168, 1, 25);
	IP4_ADDR(&nm, 255, 255, 255, 0);
	IP4_ADDR(&gw, 192, 168, 1, 254);

	struct netif* netif = (struct netif*) kmalloc(sizeof(struct netif));
	netif_add(netif, &ip, &nm, &gw, (void*) e1000, if_init, tcpip_input);


#ifdef E1000_DHCP
	if(dhcp_start(netif) != ERR_OK) {
#ifdef E1000_DEBUG
		kprintf("e1000: dhcp error!\n");
#endif
		return NULL;
	}
#else
	netif_set_up(netif);
#endif

	netif_set_default(netif);
	return netif;
}
