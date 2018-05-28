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


#ifndef _NETWORK_H
#define _NETWORK_H

#include "lwipopts.h"
#include "lwip/opt.h"

#include "lwip/init.h"
#include "lwip/stats.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/sockets.h"
#include "lwip/ip.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/autoip.h"
#include "lwip/igmp.h"
#include "lwip/dns.h"
#include "lwip/tcpip.h"
#include "lwip/snmp.h"
#include "lwip/ip_addr.h"
#include "netif/etharp.h"


#ifndef __ASSEMBLY__


struct ethif {
    void (*low_level_init) (void*, uint8_t*, void*);
    int (*low_level_startoutput) (void*);
    void (*low_level_output) (void*, void*, uint16_t);
    void (*low_level_endoutput) (void*, uint16_t);
    int (*low_level_startinput) (void*);
    void (*low_level_input) (void*, void*, uint16_t);
    void (*low_level_endinput) (void*);
    void (*low_level_input_nomem) (void*, uint16_t);

    void* internals;
    uint8_t address[6];

    ip_addr_t ip;
    ip_addr_t nm;
    ip_addr_t gw;
};

int network_init(void);

void ethif_input(struct netif* netif);
err_t ethif_init(struct netif* netif);
#endif


#endif
