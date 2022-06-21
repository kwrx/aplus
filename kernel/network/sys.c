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

#define NR_tcpip_wait       500



struct sys_sem {
    semaphore_t sem;
    volatile long flags;
} __packed;

struct sys_mbox {
    spinlock_t lock;
    uint32_t size;
    uint32_t count;
    void** msg;
} __packed;

spinlock_t tcpip_lock;
spinlock_t tcpip_core_locking;




SYSCALL(NR_tcpip_wait, tcpip_wait,
long sys_tcpip_wait(uint32_t* s, u32_t timeout) {

    DEBUG_ASSERT(s);

    if(!timeout) {

        futex_wait(current_task, s, 0, NULL);

    } else {

        struct timespec ts;
        ts.tv_sec  = timeout / 1000;
        ts.tv_nsec = (timeout % 1000) * 1000000;

        futex_wait(current_task, s, 0, &ts);

    }

    thread_suspend(current_task);
    thread_restart_sched(current_task);

    return 0;

});




void sys_init(void) {
    spinlock_init(&tcpip_lock);
    spinlock_init(&tcpip_core_locking);
}

err_t sys_sem_new(struct sys_sem** s, u8_t count) {
    
    DEBUG_ASSERT(s);
    //DEBUG_ASSERT(count == 1);
    
    (*s) = (struct sys_sem*) kcalloc(sizeof(struct sys_sem), 1, GFP_KERNEL);

    sem_init(&(*s)->sem, count);    
    
    SYS_STATS_INC_USED(sem);
    return ERR_OK;

}

void sys_sem_free(struct sys_sem** s) {

    DEBUG_ASSERT(s);
    DEBUG_ASSERT(*s);

    SYS_STATS_DEC(sem.used);

    kfree(*s);
    (*s) = NULL;

}

void sys_sem_signal(struct sys_sem** s) {
    
    DEBUG_ASSERT(s);
    DEBUG_ASSERT(*s);

    sem_post(&(*s)->sem);

}

u32_t sys_arch_sem_wait(struct sys_sem** s, u32_t timeout) {

    ssize_t e;

    if(timeout) {

        uint64_t t0 = arch_timer_generic_getms() + timeout;
  
        if((e = arch_syscall2(NR_tcpip_wait, &(*s)->sem, timeout)) < 0)
            return e;

        if(arch_timer_generic_getms() >= t0)
            return SYS_ARCH_TIMEOUT;

        return t0 - arch_timer_generic_getms();

    } else {

        if((e = arch_syscall2(NR_tcpip_wait, &(*s)->sem, 0)) < 0)
            return e;

        return 1;

    }

}


err_t sys_mbox_new(struct sys_mbox** mbox, int size) {
    
    DEBUG_ASSERT(mbox);
    DEBUG_ASSERT(size);


    struct sys_mbox* mb = (struct sys_mbox*) kcalloc(sizeof(struct sys_mbox), 1, GFP_KERNEL);

    mb->size = size;
    mb->count = 0;
    mb->msg = kcalloc(sizeof(void*), size, GFP_KERNEL);

    spinlock_init(&mb->lock);


    SYS_STATS_INC_USED(mbox);
    *mbox = mb;

    return ERR_OK;

}


void sys_mbox_free(struct sys_mbox** mbox) {
    
    DEBUG_ASSERT(mbox);
    DEBUG_ASSERT(*mbox);

    SYS_STATS_DEC(mbox.used);

    kfree((*mbox)->msg);
    kfree((*mbox));

    (*mbox) = NULL;

}



err_t sys_mbox_trypost(struct sys_mbox** mbox, void* msg) {
    
    DEBUG_ASSERT(mbox);
    DEBUG_ASSERT(*mbox);
    //DEBUG_ASSERT((*mbox)->count <= (*mbox)->size);

    if((*mbox)->count > (*mbox)->size)
        return ERR_MEM;

    __lock(&(*mbox)->lock,
        (*mbox)->msg[(*mbox)->count++] = msg);

    return ERR_OK;

}



err_t sys_mbox_trypost_fromisr(struct sys_mbox** mbox, void* msg) {
    
    DEBUG_ASSERT(mbox);
    DEBUG_ASSERT(*mbox);
    //DEBUG_ASSERT((*mbox)->count <= (*mbox)->size);

    if((*mbox)->count > (*mbox)->size)
        return ERR_MEM;

    __lock(&(*mbox)->lock,
        (*mbox)->msg[(*mbox)->count++] = msg);

    return ERR_OK;

}



void sys_mbox_post(struct sys_mbox** mbox, void* msg) {
    
    DEBUG_ASSERT(mbox);
    DEBUG_ASSERT(*mbox);
    DEBUG_ASSERT(msg);

    sys_mbox_trypost(mbox, msg);

}


u32_t sys_arch_mbox_fetch(struct sys_mbox** m, void** msg, u32_t timeout) {
    
    DEBUG_ASSERT(m);
    DEBUG_ASSERT(*m);

    ssize_t e = 0;
    ssize_t r = 1;


    if(__atomic_load_n(&(*m)->count, __ATOMIC_SEQ_CST) == 0) {

        if(timeout) {

            uint64_t t0 = arch_timer_generic_getms() + timeout;

            if((e = arch_syscall2(NR_tcpip_wait, &(*m)->count, timeout)) < 0)
                return e;

            if(arch_timer_generic_getms() >= t0)
                return SYS_ARCH_TIMEOUT;

            r = t0 - arch_timer_generic_getms();

        } else {

            if((e = arch_syscall2(NR_tcpip_wait, &(*m)->count, 0)) < 0)
                return e;

            r = 1;

        }
        
    }

    __lock(&(*m)->lock, {

        DEBUG_ASSERT((*m)->count > 0);

        if(likely(msg)) {
            *msg = (*m)->msg[(*m)->count - 1];
        }

        __atomic_sub_fetch(&(*m)->count, 1, __ATOMIC_SEQ_CST);

    });


    return r;

}

u32_t sys_arch_mbox_tryfetch(struct sys_mbox** mbox, void** msg) {
    
    DEBUG_ASSERT(mbox);
    DEBUG_ASSERT(*mbox);

    if(msg)
        *msg = NULL;

    if((*mbox)->count == 0)
        return SYS_MBOX_EMPTY;


    __lock(&(*mbox)->lock, {

        void* mx = (*mbox)->msg[--(*mbox)->count];

        if(msg)
            *msg = mx;

    });


    return 0;

}


sys_thread_t sys_thread_new(const char* name, lwip_thread_fn thread, void* arg, int stacksize, int prio) {
    
    LWIP_UNUSED_ARG(prio);

    return (sys_thread_t) arch_task_spawn_kthread(name, (void (*)(void*)) thread, stacksize, arg); 
}


sys_prot_t sys_arch_protect(void) {
    spinlock_lock(&tcpip_lock);
    return 0;
}

void sys_arch_unprotect(sys_prot_t pval) {
    spinlock_unlock(&tcpip_lock);
}

u32_t sys_now() {
    return (u32_t) arch_timer_generic_getms();
}

