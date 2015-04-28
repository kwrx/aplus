#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

#include <stddef.h>
#include <aplus.h>

#define LWIP_UDPLITE		1
#define LWIP_DNS			1
#define LWIP_DHCP 			1
#define LWIP_AUTOIP			1
#define LWIP_SNMP			1



#define LWIP_HAVE_LOOPIF	1

#define LWIP_PROVIDE_ERRNO	1
#define MEM_LIBC_MALLOC		1

#define TCP_SND_BUF			(2 * TCP_MSS)
#define TCP_LISTEN_BACKLOG	1

#ifdef NET_DEBUG
#define LWIP_DEBUG 			1
#endif



#define MEMP_NUM_SYS_TIMEOUT	10


extern void* kmalloc(size_t);
extern void kfree(void*);
extern void* kcalloc(size_t, size_t);

#define mem_malloc	kmalloc
#define mem_free	kfree
#define mem_calloc	kcalloc

#endif
