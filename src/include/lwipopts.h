#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

#include <stddef.h>
#include <aplus.h>

#ifdef NETWORK_DEBUG
#define LWIP_DEBUG 				1
#endif

#ifdef NETSTACK_DEBUG
#define LWIP_DBG_TYPES_ON		LWIP_DBG_ON
#define NETIF_DEBUG				LWIP_DBG_ON
#define PBUF_DEBUG				LWIP_DBG_ON
#define INET_DEBUG				LWIP_DBG_ON
#define IP_DEBUG				LWIP_DBG_ON
#define MEM_DEBUG				LWIP_DBG_ON
#define RAW_DEBUG				LWIP_DBG_ON
#define SYS_DEBUG				LWIP_DBG_ON
#define TIMERS_DEBUG			LWIP_DBG_OFF
#define TCP_DEBUG				LWIP_DBG_ON
#define TCPIP_DEBUG				LWIP_DBG_ON
#define UDP_DEBUG				LWIP_DBG_ON
#define DHCP_DEBUG				LWIP_DBG_ON
#define DNS_DEBUG				LWIP_DBG_OFF
#define TCP_INPUT_DEBUG			LWIP_DBG_ON
#define SOCKETS_DEBUG			LWIP_DBG_ON
#define API_LIB_DEBUG			LWIP_DBG_ON
#define API_MSG_DEBUG			LWIP_DBG_ON

#endif

#define LWIP_UDPLITE			1
#define LWIP_DNS				1
#define LWIP_DHCP 				1
#define LWIP_AUTOIP				1
#define LWIP_SNMP				1
#define LWIP_NETIF_LOOPBACK		1
#define LWIP_NETIF_HOSTNAME		1
#define LWIP_NETIF_API			1

#define LWIP_HAVE_LOOPIF		1
#define LWIP_PROVIDE_ERRNO		1


#define MEMP_NUM_SYS_TIMEOUT	10
#define MEM_LIBC_MALLOC			1
#define MEM_SIZE				1600000

#define TCP_SND_BUF				(2 * TCP_MSS)
#define TCP_LISTEN_BACKLOG		1

#define IP_REASSEMBLY			1


#define LWIP_TCPIP_CORE_LOCKING	1


extern void* kmalloc(size_t);
extern void kfree(void*);
extern void* kcalloc(size_t, size_t);

#define mem_malloc	kmalloc
#define mem_free	kfree
#define mem_calloc	kcalloc


WARNING("-Waddress")
WARNING("-Wparentheses")
WARNING("-Warray-bounds")
WARNING("-Wformat")

#endif
