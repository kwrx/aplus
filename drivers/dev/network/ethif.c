/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdio.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/memory.h>
#include <aplus/module.h>

#include <dev/interface.h>
#include <dev/network.h>



#ifndef ETHERNETIF_MAXFRAMES
    #define ETHERNETIF_MAXFRAMES 1
#endif


/* See kernel/init/hostname.c */
extern char *hostname;


void ethif_input(struct netif *netif) {

    DEBUG_ASSERT(netif);
    DEBUG_ASSERT(netif->state);


    struct eth_hdr *hdr;
    struct pbuf *p, *q;

    long len;
    long frames = 0;


    struct device *dev = netif->state;

    do {

        if ((len = dev->net.low_level_startinput(dev->net.internals)) == 0)
            break;

#if ETH_PAD_SIZE
        len += ETH_PAD_SIZE;
#endif


        p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
        if (!p) {
#if ETH_PAD_SIZE
            len -= ETH_PAD_SIZE;
#endif

            dev->net.low_level_input_nomem(dev->net.internals, len);

            LINK_STATS_INC(link.memerr);
            LINK_STATS_INC(link.drop);
            return;
        }


#if ETH_PAD_SIZE
        pbuf_header(p, -ETH_PAD_SIZE);
#endif

        for (q = p; q; q = q->next) {
            dev->net.low_level_input(dev->net.internals, q->payload, q->len);
        }

        dev->net.low_level_endinput(dev->net.internals);

#if ETH_PAD_SIZE
        pbuf_header(p, ETH_PAD_SIZE);
#endif

        LINK_STATS_INC(link.recv);


        hdr = p->payload;

        switch (lwip_htons(hdr->type)) {

            case ETHTYPE_IP:
            case ETHTYPE_ARP:
#if PPPOE_SUPPORT
            case ETHTYPE_PPPOEDISC:
            case ETHTYPE_PPPOE:
#endif

                if (netif->input(p, netif) != ERR_OK) {
                    LWIP_DEBUGF(NETIF_DEBUG, ("ethif_input(): IP input error\n"));
                    pbuf_free(p);
                    p = NULL;
                }
                break;


            default:

                pbuf_free(p);
                p = NULL;
                break;
        }

    } while ((!ETHERNETIF_MAXFRAMES) || (++frames < ETHERNETIF_MAXFRAMES));
}


static err_t ethif_linkoutput(struct netif *netif, struct pbuf *p) {

    DEBUG_ASSERT(netif);
    DEBUG_ASSERT(netif->state);

    DEBUG_ASSERT(p);
    DEBUG_ASSERT(p->payload);
    DEBUG_ASSERT(p->len);


    struct device *dev = netif->state;
    struct pbuf *q;


    if (!dev->net.low_level_startoutput(dev->net.internals))
        return ERR_IF;


#if ETH_PAD_SIZE
    pbuf_header(p, -ETH_PAD_SIZE);
#endif

    for (q = p; q; q = q->next)
        dev->net.low_level_output(dev->net.internals, p->payload, p->len);

    dev->net.low_level_endoutput(dev->net.internals, p->tot_len);

#if ETH_PAD_SIZE
    pbuf_header(p, ETH_PAD_SIZE);
#endif


    LINK_STATS_INC(link.xmit);
    return ERR_OK;
}

err_t ethif_init(struct netif *netif) {

    DEBUG_ASSERT(netif);
    DEBUG_ASSERT(netif->state);


    struct device *dev = netif->state;

#if LWIP_NETIF_HOSTNAME
    netif->hostname = hostname;
#endif

    NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 100 * 1024 * 1024);

    netif->name[0] = 'e';
    netif->name[1] = 'n';

    netif->output     = etharp_output;
    netif->linkoutput = ethif_linkoutput;

    netif->hwaddr_len = ETHARP_HWADDR_LEN;
    memcpy(netif->hwaddr, dev->net.address, ETHARP_HWADDR_LEN);

    netif->mtu   = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;


    dev->net.low_level_init(dev->net.internals, dev->net.address, NULL);

    return ERR_OK;
}
