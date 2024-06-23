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
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/syscall.h>
#include <aplus/task.h>



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


spinlock_t tcpip_lock;
spinlock_t tcpip_core_locking;


SYSCALL(
    SYSCALL_NR_TCPIP_WAIT, tcpip_wait, long sys_tcpip_wait(uint32_t *s, u32_t value, u32_t timeout) {
        DEBUG_ASSERT(s);

        if (!timeout) {

            futex_wait(current_task, s, value, NULL);

        } else {

            struct timespec ts;
            ts.tv_sec  = timeout / 1000;
            ts.tv_nsec = (timeout % 1000) * 1000000;

            futex_wait(current_task, s, value, &ts);
        }

        thread_suspend(current_task);
        thread_restart_sched(current_task);

        return 0;
    });



void sys_init(void) {
    spinlock_init(&tcpip_lock);
    spinlock_init(&tcpip_core_locking);
}



sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio) {

    DEBUG_ASSERT(name);
    DEBUG_ASSERT(thread);

    LWIP_UNUSED_ARG(prio);

    return (sys_thread_t)arch_task_spawn_kthread(name, (void (*)(void *))thread, stacksize, arg);
}


sys_prot_t sys_arch_protect(void) {
    return spinlock_lock(&tcpip_lock), 0;
}

void sys_arch_unprotect(sys_prot_t pval) {
    spinlock_unlock(&tcpip_lock);
}

u32_t sys_now() {
    return (u32_t)arch_timer_generic_getms();
}
