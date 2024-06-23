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



struct sys_mbox {
        queue_t queue;
};


err_t sys_mbox_new(struct sys_mbox** mbox, int size) {

    DEBUG_ASSERT(mbox);
    DEBUG_ASSERT(size);

    LWIP_UNUSED_ARG(size);


    struct sys_mbox* m = (struct sys_mbox*)kcalloc(sizeof(struct sys_mbox), 1, GFP_KERNEL);

    if (unlikely(!m)) {
        return ERR_MEM;
    }

    queue_init(&m->queue);


    SYS_STATS_INC(mbox.used);

    return *mbox = m, ERR_OK;
}


void sys_mbox_free(struct sys_mbox** mbox) {

    DEBUG_ASSERT(mbox);
    DEBUG_ASSERT(*mbox);

    queue_destroy(&(*mbox)->queue);
    kfree((*mbox));

    (*mbox) = NULL;

    SYS_STATS_DEC(mbox.used);
}


err_t sys_mbox_trypost_fromisr(struct sys_mbox** mbox, void* msg) {

    DEBUG_ASSERT(mbox);
    DEBUG_ASSERT(*mbox);

    return queue_enqueue(&(*mbox)->queue, msg, 0), ERR_OK;
}


err_t sys_mbox_trypost(struct sys_mbox** mbox, void* msg) {

    DEBUG_ASSERT(mbox);
    DEBUG_ASSERT(*mbox);

    return sys_mbox_trypost_fromisr(mbox, msg);
}


void sys_mbox_post(struct sys_mbox** mbox, void* msg) {

    DEBUG_ASSERT(mbox);
    DEBUG_ASSERT(*mbox);

    sys_mbox_trypost_fromisr(mbox, msg);
}


u32_t sys_arch_mbox_fetch(struct sys_mbox** mbox, void** msg, u32_t timeout) {

    DEBUG_ASSERT(mbox);
    DEBUG_ASSERT(*mbox);


    size_t e = 0;
    size_t r = 1;

    void* m = NULL;

    do {

        while (queue_is_empty(&(*mbox)->queue)) {

            if (timeout) {

                uint64_t t0 = arch_timer_generic_getms() + timeout;

                if ((e = arch_syscall3(SYSCALL_NR_TCPIP_WAIT, &((*mbox)->queue).size, 0, timeout)) < 0)
                    return e;

                if (arch_timer_generic_getms() >= t0)
                    return SYS_ARCH_TIMEOUT;

                r = t0 - arch_timer_generic_getms();

            } else {

                if ((e = arch_syscall3(SYSCALL_NR_TCPIP_WAIT, &((*mbox)->queue).size, 0, 0)) < 0)
                    return e;

                r = 1;
            }
        }

        m = queue_pop(&(*mbox)->queue);

    } while (m == NULL);


    if (likely(msg)) {
        *msg = m;
    }

    return r;
}


u32_t sys_arch_mbox_tryfetch(struct sys_mbox** mbox, void** msg) {

    DEBUG_ASSERT(mbox);
    DEBUG_ASSERT(*mbox);

    if (queue_is_empty(&(*mbox)->queue))
        return SYS_MBOX_EMPTY;

    *msg = queue_pop(&(*mbox)->queue);

    return 0;
}
