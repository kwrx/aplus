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
#include <aplus/smp.h>
#include <aplus/timer.h>
#include <stdint.h>


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


semaphore_t tcpip_done;


static void tcpip_init_done(void* arg) {

    DEBUG_ASSERT(arg);

    
    static char* __argv[] = {
        "[tcpipd]",
        NULL
    };

    current_task->argv = __argv;
    current_task->priority = TASK_PRIO_MIN;



    struct netif* lo = netif_find("lo0");
    if(likely(lo)) {
        netif_set_up(lo);
        netif_set_default(lo);
    } else
        kprintf("netif: WARN! Loopback interface not found\n");




    ip_addr_t* dns = (ip_addr_t*) arg;

    dns_setserver(0, &dns[0]);
    dns_setserver(1, &dns[1]);


    sem_post(&tcpip_done);
}


void network_init() {

    sem_init(&tcpip_done, 0);

    ip_addr_t dns[2];
    IP_ADDR4(&dns[0], 8, 8, 8, 8);
    IP_ADDR4(&dns[1], 8, 8, 4, 4);


    kprintf("network: initialize and setup local network...\n");
    kprintf(" -> dns: %d.%d.%d.%d - %d.%d.%d.%d\n", 
        (dns[0].u_addr.ip4.addr >>  0) & 0xFF,
        (dns[0].u_addr.ip4.addr >>  8) & 0xFF,
        (dns[0].u_addr.ip4.addr >> 16) & 0xFF,
        (dns[0].u_addr.ip4.addr >> 24) & 0xFF,
        (dns[1].u_addr.ip4.addr >>  0) & 0xFF,
        (dns[1].u_addr.ip4.addr >>  8) & 0xFF,
        (dns[1].u_addr.ip4.addr >> 16) & 0xFF,
        (dns[1].u_addr.ip4.addr >> 24) & 0xFF);

    
    tcpip_init(&tcpip_init_done, &dns);
    sem_wait(&tcpip_done);

    kprintf("network: up!\n");

}
