#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/ipc.h>
#include <xdev/mm.h>
#include <xdev/task.h>
#include <xdev/timer.h>
#include <libc.h>



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
#include "lwip/ip_frag.h"
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
#include "lwip/timers.h"
#include "lwip/udp.h"



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
	struct sys_sem* sx = (struct sys_sem*) kmalloc(sizeof(struct sys_sem), GFP_KERNEL);
	sx->lock = count;

	SYS_STATS_INC_USED(sem);
	*s = sx; 
	return ERR_OK;
}

void sys_sem_free(struct sys_sem** sem) {
	SYS_STATS_DEC(sem.used);

	kfree(*sem);
	(*sem) = NULL;
}

void sys_sem_signal(struct sys_sem** sem) {
#if CONFIG_SMP
	__sync_synchronize();
#endif
	(*sem)->lock++;
}

u32_t sys_arch_sem_wait(struct sys_sem** sem, u32_t __timeout) {

	register u32_t timeout = __timeout;

	if(timeout)
		timeout += timer_getms();

#if CONFIG_SMP
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

	struct sys_mbox* mb = (struct sys_mbox*) kmalloc(sizeof(struct sys_mbox), GFP_KERNEL);	

	mb->size = size;
	mb->count = 0;
	mb->msg = kmalloc(sizeof(void*) * size, GFP_KERNEL);

	SYS_STATS_INC_USED(mbox);
	*mbox = mb;

	return ERR_OK;
}

void sys_mbox_free(struct sys_mbox** mbox) {
	if(unlikely(!(*mbox)))
		return;

	SYS_STATS_DEC(mbox.used);

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


	void* mx = (*mbox)->msg[0];
	memcpy(&(*mbox)->msg[0], &(*mbox)->msg[1], sizeof(void*) * (--(*mbox)->count));
	
	if(msg)
		*msg = mx;		

	return 0;
}


sys_thread_t sys_thread_new(const char* name, lwip_thread_fn thread, void* arg, int stacksize, int prio) {
	LWIP_UNUSED_ARG(name);
	LWIP_UNUSED_ARG(stacksize);
	LWIP_UNUSED_ARG(prio);

	return sys_clone((int (*)(void*)) thread, NULL, CLONE_VM | CLONE_FILES | CLONE_FS | CLONE_SIGHAND, arg);
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


EXPORT(lwip_accept);
EXPORT(lwip_bind);
EXPORT(lwip_shutdown);
EXPORT(lwip_getpeername);
EXPORT(lwip_getsockname);
EXPORT(lwip_getsockopt);
EXPORT(lwip_setsockopt);
EXPORT(lwip_close);
EXPORT(lwip_connect);
EXPORT(lwip_listen);
EXPORT(lwip_recv);
EXPORT(lwip_read);
EXPORT(lwip_recvfrom);
EXPORT(lwip_send);
EXPORT(lwip_sendto);
EXPORT(lwip_socket);
EXPORT(lwip_write);
EXPORT(lwip_select);
EXPORT(lwip_ioctl);
EXPORT(lwip_fcntl);

EXPORT(lwip_gethostbyname);

EXPORT(lwip_htons);
EXPORT(lwip_htonl);
EXPORT(lwip_ntohs);
EXPORT(lwip_ntohl);

EXPORT(etharp_output);
EXPORT(tcpip_input);

EXPORT(pbuf_alloc);
EXPORT(pbuf_free);
EXPORT(pbuf_header);

EXPORT(dhcp_start);
EXPORT(dhcp_stop);

EXPORT(netif_add);
EXPORT(netif_remove);
EXPORT(netif_find);
EXPORT(netif_set_up);
EXPORT(netif_set_down);
EXPORT(netif_set_default);


EXPORT(lwip_stats);
