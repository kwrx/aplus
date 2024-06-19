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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/task.h>
#include <aplus/syscall.h>
#include <aplus/hal.h>



#include "lwip/opt.h"

#include "lwip/api.h"
#include "lwip/arch.h"
#include "lwip/autoip.h"
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/icmp.h"
#include "lwip/igmp.h"
#include "lwip/inet.h"
#include "lwip/inet_chksum.h"
#include "lwip/init.h"
#include "lwip/ip.h"
#include "lwip/ip_addr.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/netbuf.h"
#include "lwip/netdb.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/opt.h"
#include "lwip/pbuf.h"
#include "lwip/raw.h"
#include "lwip/sio.h"
#include "lwip/snmp.h"
#include "lwip/sockets.h"
#include "lwip/stats.h"
#include "lwip/sys.h"
#include "lwip/tcp.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"



struct sys_sem {
    semaphore_t sem;
    volatile long flags;
};


err_t sys_sem_new(struct sys_sem** sem, u8_t count) {

    DEBUG_ASSERT(sem);


    *(sem) = (struct sys_sem*) kcalloc(sizeof(struct sys_sem), 1, GFP_KERNEL);

    if(unlikely(*(sem) == NULL)) {
        return ERR_MEM;
    }

    sem_init(&(*(sem))->sem, count);


    SYS_STATS_INC(sem.used);

    return ERR_OK;

}


void sys_sem_free(struct sys_sem** sem) {

    DEBUG_ASSERT(sem);
    DEBUG_ASSERT(*sem);

    kfree((*sem));
    (*sem) = NULL;

    SYS_STATS_DEC(sem.used);

}


void sys_sem_signal(struct sys_sem** sem) {

    DEBUG_ASSERT(sem);
    DEBUG_ASSERT(*sem);

    sem_post(&(*(sem))->sem);

}


u32_t sys_arch_sem_wait(struct sys_sem** sem, u32_t timeout) {

    DEBUG_ASSERT(sem);
    DEBUG_ASSERT(*sem);


    size_t e = 0;

    if(timeout) {

        uint64_t t0 = arch_timer_generic_getms() + timeout;

        if((e = arch_syscall3(SYSCALL_NR_TCPIP_WAIT, &(*sem)->sem, 0, timeout)) < 0)
            return e;

        if(arch_timer_generic_getms() >= t0)
            return SYS_ARCH_TIMEOUT;

        return t0 - arch_timer_generic_getms();

    } else {

        if((e = arch_syscall3(SYSCALL_NR_TCPIP_WAIT, &(*sem)->sem, 0, timeout)) < 0)
            return e;

        return 1;

    }

}

