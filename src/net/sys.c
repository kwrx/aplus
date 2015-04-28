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
#include "netif/etharp.h"


void sys_init(void) {
#ifdef NET_DEBUG
	kprintf("tcpip: loading network stack\n");
#endif
}


err_t sys_sem_new(sys_sem_t* s, u8_t count) {
	if(s == NULL)	
		s = (sys_sem_t*) kmalloc(sizeof(sys_sem_t));
	*s = count;

	return ERR_OK;
}

void sys_sem_free(sys_sem_t* sem) {
	*sem = 0;
	kfree(sem);
}

void sys_sem_signal(sys_sem_t* sem) {
	__sync_lock_test_and_set(sem, 1);
}

u32_t sys_arch_sem_wait(sys_sem_t* sem, u32_t timeout) {
	if(timeout == 0) {	
		while(
				!__sync_bool_compare_and_swap(
					sem,
					0,
					1
				)
		) cpu_wait();

		return SYS_ARCH_TIMEOUT;
	}

	while(
			!__sync_bool_compare_and_swap(
				sem,
				0,
				1
			)
		&&
			!(timeout < timer_getms())
	) cpu_wait();
	
	int s = timeout - (timeout - timer_getms());
	if(s < timeout)
		return s;
	
	return SYS_ARCH_TIMEOUT;
}


err_t sys_mbox_new(sys_mbox_t* mbox, int size) {
	if(mbox == NULL)
		mbox = (sys_mbox_t*) kmalloc(sizeof(sys_mbox_t));

	mbox->size = size;
	mbox->count = 0;
	mbox->msg = kmalloc(sizeof(void*) * size);

	return ERR_OK;
}

void sys_mbox_free(sys_mbox_t* mbox) {
	kfree(mbox);
}

err_t sys_mbox_trypost(sys_mbox_t* mbox, void* msg) {
	if(mbox->count > mbox->size)
		return ERR_MEM;

	mbox->msg[mbox->count++] = msg;
	return ERR_OK;
}

void sys_mbox_post(sys_mbox_t* mbox, void* msg) {
	sys_mbox_trypost(mbox, msg);
}

u32_t sys_arch_mbox_fetch(sys_mbox_t* mbox, void** msg, u32_t timeout) {
	if(timeout == 0)
		timeout = ~0;
	
	while((mbox->count == 0) && (timeout < timer_getms()))
		cpu_wait();

	if(msg)
		for(int i = 0; i < mbox->count; i++)
			*msg++ = mbox->msg[i];

	mbox->count = 0;

	int s = timeout - (timeout - timer_getms());
	if(s < timeout)
		return s;

	return SYS_ARCH_TIMEOUT;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t* mbox, void** msg) {
	if(mbox->count == 0)
		return SYS_MBOX_EMPTY;

	if(msg)
		for(int i = 0; i < mbox->count; i++)
			*msg++ = mbox->msg[i];
	mbox->count = 0;

	return 0;
}


sys_thread_t sys_thread_new(const char* name, lwip_thread_fn thread, void* arg, int stacksize, int prio) {
	(void) name;
	(void) stacksize;
	(void) prio;

	return sys_clone(thread, NULL, __CLONE_THREAD, arg);
}


sys_prot_t sys_arch_protect(void) {
	return 0;
}

void sys_arch_unprotect(sys_prot_t pval) {
	return;
}

