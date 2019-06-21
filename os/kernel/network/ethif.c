/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <aplus.h>
#include <aplus/base.h>
#include <aplus/module.h>
#include <aplus/mm.h>
#include <aplus/vfs.h>
#include <aplus/debug.h>
#include <aplus/network.h>
#include <aplus/intr.h>
#include <stdint.h>


#ifndef ETHERNETIF_MAXFRAMES
#define ETHERNETIF_MAXFRAMES            1
#endif


/* See os/kernel/init/hostname.c */
extern char* hostname;

/* TODO */
void ethif_input(struct netif* netif) {
    struct eth_hdr* hdr;
    struct pbuf* p, *q;

    int len;
    int frames = 0;


    struct ethif* ethif = netif->state;
    do {
        if((len = ethif->low_level_startinput(ethif->internals)) == 0)
            break;
            
#if ETH_PAD_SIZE
        len += ETH_PAD_SIZE;
#endif


        p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
        if(!p) {
#if ETH_PAD_SIZE
            len -= ETH_PAD_SIZE;
#endif 

            ethif->low_level_input_nomem(ethif->internals, len);
            LINK_STATS_INC(link.memerr);
            LINK_STATS_INC(link.drop);
            return;
        }

#if ETH_PAD_SIZE
        pbuf_header(p, -ETH_PAD_SIZE);
#endif

        for(q = p; q; q = q->next)
            ethif->low_level_input(ethif->internals, q->payload, q->len);

        ethif->low_level_endinput(ethif->internals);

#if ETH_PAD_SIZE
        pbuf_header(p, ETH_PAD_SIZE);
#endif

        LINK_STATS_INC(link.recv);


        hdr = p->payload;
        switch(lwip_htons(hdr->type)) {
            case ETHTYPE_IP:
            case ETHTYPE_ARP:
#if PPPOE_SUPPORT
            case ETHTYPE_PPPOEDISC:
            case ETHTYPE_PPPOE:
#endif

                if(netif->input(p, netif) != ERR_OK) {
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

    } while((!ETHERNETIF_MAXFRAMES) || (++frames < ETHERNETIF_MAXFRAMES));
}


static err_t ethif_linkoutput(struct netif* netif, struct pbuf* p) {
    struct ethif* ethif = netif->state;
    struct pbuf* q;

    if(!ethif->low_level_startoutput(ethif->internals))
        return ERR_IF;

#if ETH_PAD_SIZE
    pbuf_header(p, -ETH_PAD_SIZE);
#endif
    
    for(q = p; q; q = q->next)
        ethif->low_level_output(ethif->internals, p->payload, p->len);

    ethif->low_level_endoutput(ethif->internals, p->tot_len);

#if ETH_PAD_SIZE
    pbuf_header(p, ETH_PAD_SIZE);
#endif

    LINK_STATS_INC(link.xmit);
    return ERR_OK;
}

err_t ethif_init(struct netif* netif) {
    LWIP_ASSERT("netif != NULL", (netif != NULL));
    LWIP_ASSERT("state != NULL", (netif->state != NULL));

    struct ethif* ethif = netif->state;

#if LWIP_NETIF_HOSTNAME
    netif->hostname = hostname;
#endif

    NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 100 * 1024 * 1024);

    netif->name[0] = 'e';
    netif->name[1] = 'n';

    netif->output = etharp_output;
    netif->linkoutput = ethif_linkoutput;

    netif->hwaddr_len = ETHARP_HWADDR_LEN;
    memcpy(netif->hwaddr, ethif->address, ETHARP_HWADDR_LEN);

    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    ethif->low_level_init(ethif->internals, ethif->address, NULL);
    return ERR_OK;
}

