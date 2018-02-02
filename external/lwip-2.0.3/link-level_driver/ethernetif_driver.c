/**
 * @file ethernetif.c
 * Ethernet Interface
 *
 * Modified by SRC from the original ethernetif.c distributed with lwIP.
 * See that file for copyright and licensing information
 */

#include <stdint.h>
#include <string.h>
#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include "netif/etharp.h"
#include "netif/ppp_oe.h"

#include "ethernetif.h"

/* Define those to better describe your network interface. */
#define IFNAME0 'e'
#define IFNAME1 'n'

#ifndef ETHERNETIF_MAXFRAMES
#define ETHERNETIF_MAXFRAMES 0
#endif
/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
void
ethernetif_input(struct netif *netif)
{
  struct ethernetif *ethernetif;
  struct eth_hdr *ethhdr;
  struct pbuf *p, *q;
  int len;
  int frames = 0;

  ethernetif = netif->state;
  do {
    if((len = ethernetif->low_level_startinput(ethernetif->internals)) == 0)
      break;

    /* move received packet into a new pbuf */
#if ETH_PAD_SIZE
    len += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

    /* We allocate a pbuf chain of pbufs from the pool. */
    p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
  
    if (p != NULL) {

#if ETH_PAD_SIZE
      pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

      /* We iterate over the pbuf chain until we have read the entire
       * packet into the pbuf. */
      for(q = p; q != NULL; q = q->next) {
        /* Read enough bytes to fill this pbuf in the chain. The
         * available data in the pbuf is given by the q->len
         * variable.
         * This does not necessarily have to be a memcpy, you can also preallocate
         * pbufs for a DMA-enabled MAC and after receiving truncate it to the
         * actually received size. In this case, ensure the tot_len member of the
         * pbuf is the sum of the chained pbuf len members.
         */
        ethernetif->low_level_input(ethernetif->internals, q->payload, q->len);
      }
      ethernetif->low_level_endinput(ethernetif->internals);

#if ETH_PAD_SIZE
      pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

      LINK_STATS_INC(link.recv);
    } else {
	  /* In many embedded systems, we might have less available RAM than what the
	  Ethernet chip has, so if we can't allocate a pbuf, the frame can sit
	  waiting in the chip and there is no need to drop it. 
	  Let the driver decide what to do */
#if ETH_PAD_SIZE
      len -= ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif
      ethernetif->low_level_input_nomem(ethernetif->internals, len);
      LINK_STATS_INC(link.memerr);
      LINK_STATS_INC(link.drop);
      return;
    }
    /* points to packet payload, which starts with an Ethernet header */
    ethhdr = p->payload;

    switch (htons(ethhdr->type)) {
    /* IP or ARP packet? */
    case ETHTYPE_IP:
    case ETHTYPE_ARP:
#if PPPOE_SUPPORT
    /* PPPoE packet? */
    case ETHTYPE_PPPOEDISC:
    case ETHTYPE_PPPOE:
#endif /* PPPOE_SUPPORT */
      /* full packet send to tcpip_thread to process */
      if (netif->input(p, netif)!=ERR_OK)
       { LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
         pbuf_free(p);
         p = NULL;
       }
      break;

    default:
      pbuf_free(p);
      p = NULL;
      break;
    }
  } while((!ETHERNETIF_MAXFRAMES) || (++frames < ETHERNETIF_MAXFRAMES));
}   
          
/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         ERR_IF if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become availale since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

static err_t
ethernetif_linkoutput(struct netif *netif, struct pbuf *p)
{
  struct ethernetif *ethernetif = netif->state;
  struct pbuf *q;

  if(!ethernetif->low_level_startoutput(ethernetif->internals))
	  return ERR_IF;
  
#if ETH_PAD_SIZE
  pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

  for(q = p; q != NULL; q = q->next) {
    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */
    ethernetif->low_level_output(ethernetif->internals, q->payload, q->len);
  }

  ethernetif->low_level_endoutput(ethernetif->internals, p->tot_len);

#if ETH_PAD_SIZE
  pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif
  
  LINK_STATS_INC(link.xmit);

  return ERR_OK;
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t
ethernetif_init(struct netif *netif)
{
  struct ethernetif *ethernetif;

  LWIP_ASSERT("netif != NULL", (netif != NULL));
  LWIP_ASSERT("state != NULL", (netif->state != NULL));
    
  ethernetif = netif->state;

#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

  /*
   * Initialize the snmp variables and counters inside the struct netif.
   * The last argument should be replaced with your link speed, in units
   * of bits per second.
   */
  NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS);

  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */
  netif->output = etharp_output;
  netif->linkoutput = ethernetif_linkoutput;
  
  /* set MAC hardware address length */
  netif->hwaddr_len = ETHARP_HWADDR_LEN;

  /* set MAC hardware address */
  memcpy(netif->hwaddr, ethernetif->address, ETHARP_HWADDR_LEN);

  /* maximum transfer unit */
  netif->mtu = ETHERNET_MTU;
  
  /* device capabilities */
  /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

  /* initialize the hardware */
  ethernetif->low_level_init(ethernetif->internals, ethernetif->address, NULL);

  return ERR_OK;
}
