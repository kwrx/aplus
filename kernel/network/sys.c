/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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
} __packed;

struct sys_mbox {
    spinlock_t lock;
    uint16_t size;
    uint16_t count;
    void** msg;
} __packed;


spinlock_t network_lock;
spinlock_t tcpip_core_locking;


void sys_init(void) {
    spinlock_init(&network_lock);
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
    arch_intr_enable((*s)->flags);

}

u32_t sys_arch_sem_wait(struct sys_sem** s, u32_t __timeout) {
   
    DEBUG_ASSERT(s);
    DEBUG_ASSERT(*s);
    
    if(!__timeout) {
        sem_wait(&(*s)->sem);
        (*s)->flags = arch_intr_disable();    

    } else {

        uint64_t t0 = arch_timer_generic_getms();
        t0 += __timeout;

        while(!sem_trywait(&(*s)->sem) && t0 > arch_timer_generic_getms())
            sched_yield();

        (*s)->flags = arch_intr_disable();    


        if(t0 <= arch_timer_generic_getms())
            return SYS_ARCH_TIMEOUT;


        return t0 - arch_timer_generic_getms();
    }

    return 1;

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


u32_t sys_arch_mbox_fetch(struct sys_mbox** mbox, void** msg, u32_t __timeout) {
    
    DEBUG_ASSERT(mbox);
    DEBUG_ASSERT(*mbox);


    if(msg)
        *msg = NULL;


    uint64_t t0 = arch_timer_generic_getms();

    if(__timeout) {

        t0 += __timeout;

        while(((*mbox)->count == 0) && t0 > arch_timer_generic_getms())
            sched_yield();


        if(t0 <= arch_timer_generic_getms())
            return SYS_ARCH_TIMEOUT;

    }
    else
        while(((*mbox)->count == 0))
            sched_yield();



    __lock(&(*mbox)->lock, {
    
        void* mx = (*mbox)->msg[--(*mbox)->count];

        if(msg)
            *msg = mx;

    });


    if(__timeout == 0)
        return 1;

    return (u32_t) (t0 - arch_timer_generic_getms());

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

    spinlock_lock(&network_lock);
    return 0;

}

void sys_arch_unprotect(sys_prot_t pval) {
    spinlock_unlock(&network_lock);
}

u32_t sys_now() {
    return (u32_t) arch_timer_generic_getms();
}

