#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>

#if DEBUG
#define LWIP_DEBUG 				        1
#endif


#define LWIP_DBG_TYPES_ON			    LWIP_DBG_OFF
#define NETIF_DEBUG				        LWIP_DBG_OFF
#define PBUF_DEBUG				        LWIP_DBG_OFF
#define INET_DEBUG				        LWIP_DBG_OFF
#define IP_DEBUG				        LWIP_DBG_OFF
#define MEM_DEBUG				        LWIP_DBG_OFF
#define RAW_DEBUG				        LWIP_DBG_OFF
#define SYS_DEBUG				        LWIP_DBG_OFF
#define TIMERS_DEBUG				    LWIP_DBG_OFF
#define TCP_DEBUG				        LWIP_DBG_OFF
#define TCPIP_DEBUG				        LWIP_DBG_OFF
#define UDP_DEBUG				        LWIP_DBG_OFF
#define DHCP_DEBUG				        LWIP_DBG_OFF
#define DNS_DEBUG				        LWIP_DBG_OFF
#define TCP_INPUT_DEBUG				    LWIP_DBG_OFF
#define SOCKETS_DEBUG				    LWIP_DBG_OFF
#define API_LIB_DEBUG				    LWIP_DBG_OFF
#define API_MSG_DEBUG				    LWIP_DBG_OFF



#define LWIP_IGMP				        1
#define LWIP_ICMP				        1
#define LWIP_BROADCAST_PING			    1
#define LWIP_MULTICAST_PING			    1
#define LWIP_RAW				        1
#define LWIP_ARP				        1
#define LWIP_UDPLITE				    1
#define LWIP_DNS				        1
#define LWIP_DHCP 				        1
#define LWIP_AUTOIP				        1
#define LWIP_SNMP				        1
#define LWIP_NETCONN				    1
#define LWIP_SOCKET				        1
#define LWIP_COMPAT_SOCKETS			    0
#define LWIP_POSIX_SOCKETS_IO_NAMES		0
#define LWIP_MPU_COMPATIBLE             1
#define LWIP_COMPAT_MUTEX_ALLOWED       1

#define LWIP_NETIF_LOOPBACK			    1
#define LWIP_NETIF_HOSTNAME			    1
#define LWIP_NETIF_API				    1

#define LWIP_TCPIP_CORE_LOCKING			1
#define LWIP_TIMEVAL_PRIVATE			0

#define LWIP_HAVE_LOOPIF			    1
#define LWIP_NETIF_LOOPBACK             1
//#define LWIP_PROVIDE_ERRNO			1

#define LWIP_TCP_KEEPALIVE			    1
#define TCP_KEEPIDLE_DEFAULT			10000UL
#define	TCP_KEEPINTVL_DEFAULT			2000UL
#define TCP_KEEPCNT_DEFAULT			    9U

#define MEM_LIBC_MALLOC				    1
#define MEM_SIZE				        (8192 * 1024 /* 8MiB */)

#define MEMP_NUM_SYS_TIMEOUT			10
#define MEMP_MEM_MALLOC				    1
#define MEMP_NUM_PBUF				    1024
#define MEMP_NUM_UDP_PCB			    64
#define MEMP_NUM_TCP_PCB_LISTEN			64
#define MEMP_NUM_TCP_SEG			    256
#define MEMP_NUM_REASSDATA			    32
#define MEMP_NUM_NETBUF				    1024
#define MEMP_NUM_NETCONN			    64


#define DNS_TABLE_SIZE				    16


#define TCP_SND_BUF				        (12 * TCP_MSS)
#define TCP_WND					        (10 * TCP_MSS)
#define TCP_LISTEN_BACKLOG			    1
#define TCP_SND_QUEUELEN                40


#define IP_REASSEMBLY				    1
#define IP_FORWARD				        1





#define mem_clib_malloc				    std_kmalloc
#define mem_clib_free				    std_kfree
#define mem_clib_calloc				    std_kcalloc


WARNING("-Waddress")
WARNING("-Wparentheses")
WARNING("-Warray-bounds")
WARNING("-Wformat")

#endif
