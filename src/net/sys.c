#include <aplus.h>
#include <aplus/spinlock.h>
#include <aplus/mm.h>
#include <aplus/task.h>


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
#include "lwip/tcp_impl.h"
#include "lwip/snmp_msg.h"
#include "lwip/autoip.h"
#include "lwip/igmp.h"
#include "lwip/dns.h"
#include "lwip/timers.h"
#include "lwip/netdb.h"
#include "netif/etharp.h"



struct sys_sem {
	int lock;
};

struct sys_mbox {
	uint16_t size;
	uint16_t count;
	void** msg;
};



void sys_init(void) {
	return;
}


err_t sys_sem_new(struct sys_sem** s, u8_t count) {
	struct sys_sem* sx = (struct sys_sem*) kmalloc(sizeof(struct sys_sem));
	sx->lock = count;

	*s = sx; 
	return ERR_OK;
}

void sys_sem_free(struct sys_sem** sem) {
	kfree(*sem);
	(*sem) = NULL;
}

void sys_sem_signal(struct sys_sem** sem) {
#if HAVE_SMP
	__sync_synchronize();
#endif
	(*sem)->lock++;
}

u32_t sys_arch_sem_wait(struct sys_sem** sem, u32_t __timeout) {

	register u32_t timeout = __timeout;

	if(timeout)
		timeout += timer_getms();

#if HAVE_SMP
	__sync_synchronize();
#endif
	(*sem)->lock--;

	while(
		((*sem)->lock < 0)
		&&
		(__timeout ? timeout > timer_getms() : 1)
	) sys_yield();


	if(__timeout == 0)
		return SYS_ARCH_TIMEOUT;
	
	int s = timeout - (timeout - timer_getms());
	if(s < timeout)
		return s;
	
	return SYS_ARCH_TIMEOUT;
}


err_t sys_mbox_new(struct sys_mbox** mbox, int size) {

	struct sys_mbox* mb = (struct sys_mbox*) kmalloc(sizeof(struct sys_mbox));	

	mb->size = size;
	mb->count = 0;
	mb->msg = kmalloc(sizeof(void*) * size);

	*mbox = mb;

	return ERR_OK;
}

void sys_mbox_free(struct sys_mbox** mbox) {
	if(unlikely(!(*mbox)))
		return;

	kfree((*mbox)->msg);
	kfree((*mbox));

	(*mbox) = NULL;
}

err_t sys_mbox_trypost(struct sys_mbox** mbox, void* msg) {
	if((*mbox)->count > (*mbox)->size)
		return ERR_MEM;

	(*mbox)->msg[(*mbox)->count++] = msg;
	return ERR_OK;
}

void sys_mbox_post(struct sys_mbox** mbox, void* msg) {
	sys_mbox_trypost(mbox, msg);
}

u32_t sys_arch_mbox_fetch(struct sys_mbox** mbox, void** msg, u32_t timeout) {
	
	if(timeout) {
		timeout += timer_getms();	
		while(((*mbox)->count == 0) && (timeout > timer_getms()))
			sys_yield();
	}
	else
		while(((*mbox)->count == 0))
			sys_yield();

	if((*mbox)->count == 0)
		return SYS_ARCH_TIMEOUT;

	//void* mx = (*mbox)->msg[--(*mbox)->count];
	void* mx = (*mbox)->msg[0];
	memcpy(&(*mbox)->msg[0], &(*mbox)->msg[1], sizeof(void*) * (--(*mbox)->count));
	
	if(msg)
		*msg = mx;		


	if(timeout == 0)
		return SYS_ARCH_TIMEOUT;

	int s = timeout - (timeout - timer_getms());
	if(s < timeout)
		return s;

	return SYS_ARCH_TIMEOUT;
}

u32_t sys_arch_mbox_tryfetch(struct sys_mbox** mbox, void** msg) {
	if((*mbox)->count == 0)
		return SYS_MBOX_EMPTY;

	//void* mx = (*mbox)->msg[--(*mbox)->count];
	void* mx = (*mbox)->msg[0];
	memcpy(&(*mbox)->msg[0], &(*mbox)->msg[1], sizeof(void*) * (--(*mbox)->count));
	
	if(msg)
		*msg = mx;		

	return 0;
}


sys_thread_t sys_thread_new(const char* name, lwip_thread_fn thread, void* arg, int stacksize, int prio) {
#ifndef NETWORK_DEBUG
	(void) name;
#endif
	(void) stacksize;
	(void) prio;

#ifdef NETWORK_DEBUG
	kprintf("tcpip: new thread \"%s\" at 0x%x\n", name, thread);
#endif

	return sys_clone(thread, NULL, __CLONE_THREAD, arg);
}


sys_prot_t sys_arch_protect(void) {
	return 0;
}

void sys_arch_unprotect(sys_prot_t pval) {
	return;
}


u32_t sys_now() {
	return timer_getms();
}


EXPORT_SYMBOL(lwip_accept);
EXPORT_SYMBOL(lwip_bind);
EXPORT_SYMBOL(lwip_shutdown);
EXPORT_SYMBOL(lwip_getpeername);
EXPORT_SYMBOL(lwip_getsockname);
EXPORT_SYMBOL(lwip_getsockopt);
EXPORT_SYMBOL(lwip_setsockopt);
EXPORT_SYMBOL(lwip_close);
EXPORT_SYMBOL(lwip_connect);
EXPORT_SYMBOL(lwip_listen);
EXPORT_SYMBOL(lwip_recv);
EXPORT_SYMBOL(lwip_read);
EXPORT_SYMBOL(lwip_recvfrom);
EXPORT_SYMBOL(lwip_send);
EXPORT_SYMBOL(lwip_sendto);
EXPORT_SYMBOL(lwip_socket);
EXPORT_SYMBOL(lwip_write);
EXPORT_SYMBOL(lwip_select);
EXPORT_SYMBOL(lwip_ioctl);
EXPORT_SYMBOL(lwip_fcntl);

EXPORT_SYMBOL(lwip_gethostbyname);
