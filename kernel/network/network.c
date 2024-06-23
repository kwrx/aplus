/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2018 Antonino Natale
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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/task.h>


#include "lwip/opt.h"

#include "lwip/autoip.h"
#include "lwip/dns.h"
#include "lwip/igmp.h"
#include "lwip/init.h"
#include "lwip/ip.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/raw.h"
#include "lwip/sockets.h"
#include "lwip/stats.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "netif/etharp.h"

/* See os/kernel/init/hostname.c */
extern char *hostname;


semaphore_t tcpip_done = {0};


static void tcpip_init_done(void *arg) {

    DEBUG_ASSERT(arg);


    struct netif *lo = netif_find("lo0");

    if (likely(lo)) {

        netif_set_up(lo);
        netif_set_default(lo);

    } else {
        kpanicf("network: unable to find loopback interface\n");
    }


    ip_addr_t *dns = (ip_addr_t *)arg;

    dns_setserver(0, &dns[0]);
    dns_setserver(1, &dns[1]);

    sem_post(&tcpip_done);
}


void network_init() {

    sem_init(&tcpip_done, 0);

    ip_addr_t dns[2];
    IP_ADDR4(&dns[0], 1, 1, 1, 1);
    IP_ADDR4(&dns[1], 1, 0, 0, 1);

#if DEBUG_LEVEL_INFO
    kprintf("network: host(%s) lwip(%s) dns1(%d.%d.%d.%d) dns2(%d.%d.%d.%d)\n", hostname, LWIP_VERSION_STRING, (dns[0].u_addr.ip4.addr >> 0) & 0xFF, (dns[0].u_addr.ip4.addr >> 8) & 0xFF, (dns[0].u_addr.ip4.addr >> 16) & 0xFF,
            (dns[0].u_addr.ip4.addr >> 24) & 0xFF, (dns[1].u_addr.ip4.addr >> 0) & 0xFF, (dns[1].u_addr.ip4.addr >> 8) & 0xFF, (dns[1].u_addr.ip4.addr >> 16) & 0xFF, (dns[1].u_addr.ip4.addr >> 24) & 0xFF);
#endif


    tcpip_init(&tcpip_init_done, &dns);


    sem_wait(&tcpip_done);

    kprintf("network: up!\n");
}
