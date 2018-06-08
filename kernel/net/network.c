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
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>


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
#include "netif/etharp.h"

#if CONFIG_NETWORK

static void tcpip_init_done(void* arg) {
    current_task->name = "[tcpipd]";
    current_task->description = "TCP/IP Stack daemon";
    current_task->priority = TASK_PRIO_MIN;


#if DEBUG
    kprintf(LOG "[%d] tcpip: initialized in %d MS\n", sys_getpid(), (uintptr_t) (timer_getms() - (uintptr_t) arg));
#else
    (void) arg;
#endif



    struct netif* lo = netif_find("lo0");
    if(likely(lo)) {
        netif_set_up(lo);
        netif_set_default(lo);
    } else
        kprintf(WARN "netif: Loopback interface not found\n");




    ip_addr_t dns[2];
    IP4_ADDR(&dns[0], 8, 8, 8, 8);
    IP4_ADDR(&dns[1], 8, 8, 4, 4);

    dns_setserver(0, &dns[0]);
    dns_setserver(1, &dns[1]);
}


int network_init() {
#if DEBUG
    tcpip_init(tcpip_init_done, (void*) ((uintptr_t) timer_getms()));
#else
    tcpip_init(tcpip_init_done, NULL);
#endif

    return 0;
}

#endif
