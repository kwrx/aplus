/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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


#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H


#include <stdint.h>

#include <aplus.h>
#include <aplus/memory.h>
#include <aplus/debug.h>


#define NUM_SHIFT                           4


#define NO_SYS                              0
#define LWIP_TIMERS                         1
#define LWIP_TIMERS_CUSTOM                  0
#define LWIP_MPU_COMPATIBLE                 1
#define LWIP_TCPIP_CORE_LOCKING             1
#define LWIP_TCPIP_CORE_LOCKING_INPUT       1  // FIXME
#define SYS_LIGHTWEIGHT_PROT                1

#define MEM_LIBC_MALLOC                     1
#define MEMP_MEM_MALLOC                     0
#define MEMP_MEM_INIT                       1
#define MEM_ALIGNMENT                       16 // FIXME
#define MEM_SIZE                            (1 << 30)
#define MEMP_OVERFLOW_CHECK                 0
#define MEMP_SANITY_CHECK                   0
#define MEM_OVERFLOW_CHECK                  0
#define MEM_SANITY_CHECK                    0
#define MEM_USE_POOLS                       0
#define MEM_USE_POOLS_TRY_BIGGER_POOL       0
#define MEMP_USE_CUSTOM_POOLS               0
#define LWIP_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT 0

#define MEMP_NUM_PBUF                       (16 << NUM_SHIFT)
#define MEMP_NUM_UDP_PCB                    (4  << NUM_SHIFT)
#define MEMP_NUM_TCP_PCB                    (5  << NUM_SHIFT)
#define MEMP_NUM_TCP_PCB_LISTEN             (8  << NUM_SHIFT)
#define MEMP_NUM_TCP_SEG                    (16 << NUM_SHIFT)
#define MEMP_NUM_REASSDATA                  (5  << NUM_SHIFT)
#define MEMP_NUM_FRAG_PBUF                  (15 << NUM_SHIFT)
#define MEMP_NUM_ARP_QUEUE                  (30 << NUM_SHIFT)
#define MEMP_NUM_IGMP_GROUP                 (8  << NUM_SHIFT)
#define MEMP_NUM_NETBUF                     (2  << NUM_SHIFT)
#define MEMP_NUM_NETCONN                    (4  << NUM_SHIFT)
#define MEMP_NUM_SELECT_CB                  (4  << NUM_SHIFT)
#define MEMP_NUM_TCPIP_MSG_API              (8  << NUM_SHIFT)
#define MEMP_NUM_TCPIP_MSG_INPKT            (8  << NUM_SHIFT)
#define MEMP_NUM_NETDB                      (1  << NUM_SHIFT)
#define MEMP_NUM_LOCALHOSTLIST              (1  << NUM_SHIFT)
#define PBUF_POOL_SIZE                      (16 << NUM_SHIFT)

// * ARP Options *
#define LWIP_ARP                            1
#define ARP_TABLE_SIZE                      (10 << NUM_SHIFT)
#define ARP_MAXAGE                          300
#define ARP_QUEUEING                        1
#define ARP_QUEUE_LEN                       (3  << NUM_SHIFT)
#define ETHARP_SUPPORT_VLAN                 0 // FIXME
#define ETH_PAD_SIZE                        0
#define ETHARP_SUPPORT_STATIC_ENTRIES       0

// * IP Options *
#define LWIP_IPV4                           1
#define IP_FORWARD                          1
#define IP_REASSEMBLY                       1
#define IP_FRAG                             1
#define IP_OPTIONS_ALLOWED                  1
#define IP_REASS_MAXAGE                     15
#define IP_REASS_MAX_PBUFS                  (10 << NUM_SHIFT)
#define IP_DEFAULT_TTL                      255
#define IP_SOF_BROADCAST                    0 // FIXME
#define IP_SOF_BROADCAST_RECV               0 // FIXME
#define IP_FORWARD_ALLOW_TX_ON_RX_NETIF     0

// * ICMP Options *
#define LWIP_ICMP                           1
#define LWIP_BROADCAST_PING                 1
#define LWIP_MULTICAST_PING                 1

// * RAW Options *
#define LWIP_RAW                            1

// * DHCP Options *
#define LWIP_DHCP                           1
#define LWIP_DHCP_GET_NTP_SRV               1 // FIXME
#define LWIP_DHCP_MAX_NTP_SERVERS           (1  << NUM_SHIFT)

// * AUTOIP Options *
#define LWIP_AUTOIP                         0

// * SNMP MIB2 Options *
#define LWIP_MIB2_CALLBACKS                 0

// * IGMP Options *
#define LWIP_IGMP                           1

// * DNS Options *
#define LWIP_DNS                            1
#define DNS_TABLE_SIZE                      (4  << NUM_SHIFT)
#define DNX_MAX_NAME_LENGTH                 256
#define DNS_MAX_SERVERS                     2
#define DNS_MAX_RETRIES                     4
#define DNS_DOES_NAME_CHECK                 1
#define DNS_LOCAL_HOSTLIST                  0 // FIXME
#define DNS_LOCAL_HOSTLIST_IS_DYNAMIC       0 // FIXME
#define LWIP_DNS_SUPPORT_MDNS_QUERIES       0

#define DNS_LOCAL_HOSTLIST_INIT {                                               \
    DNS_LOCAL_HOSTLIST_ELEM("host_ip4",  IPADDR4_INIT_BYTES(127, 0, 0, 1)),     \
    DNS_LOCAL_HOSTLIST_ELEM("host_ip6",  IPADDR6_INIT_HOST(0, 0, 0, 1)),        \
}


// * UDP Options *
#define LWIP_UDP                            1
#define LWIP_UDPLITE                        1
#define LWIP_NETBUF_RECVINFO                0

// * TCP Options *
#define LWIP_TCP                            1
#define TCP_WND                             ((4 << NUM_SHIFT) * TCP_MSS)
#define TCP_MAXRTX                          12
#define TCP_SYNMAXRTX                       6
#define LWIP_TCP_SACK_OUT                   1
#define LWIP_TCP_MAX_SACK_NUM               (4  << NUM_SHIFT)
#define TCP_CALCULATE_EFF_SEND_MSS          1
#define TCP_SND_BUF                         ((2 << NUM_SHIFT) * TCP_MSS)
#define TCP_LISTEN_BACKLOG                  1
#define TCP_DEFAULT_LISTEN_BACKLOG          255
#define LWIP_TCP_TIMESTAMPS                 0 // FIXME
#define LWIP_WND_SCALE                      1 // FIXME
#define TCP_RCV_SCALE                       NUM_SHIFT

// * PBUF Options *
#define LWIP_PBUF_REF_T                     u16_t

// * Network Interfaces Options *
#define LWIP_SINGLE_NETIF                   0
#define LWIP_NETIF_HOSTNAME                 1
#define LWIP_NETIF_API                      1
#define LWIP_NETIF_STATUS_CALLBACK          1 // FIXME
#define LWIP_NETIF_LINK_CALLBACK            1
#define LWIP_NETIF_REMOVE_CALLBACK          1
#define LWIP_NETIF_HWADDRHINT               1 // FIXME
#define LWIP_NETIF_TX_SINGLE_PBUF           1 // FIXME

//* LOOPIF Options *
#define LWIP_NETIF_LOOPBACK                 1
#define LWIP_LOOPIF_MULTICAST               1

// * Thread Options *
#define TCPIP_THREAD_NAME                   "[tcpip]"
#define TCPIP_THREAD_STACKSIZE              (1024 << 10)
#define TCPIP_THREAD_PRIO                   (0)
#define TCPIP_MBOX_SIZE                     (64 << NUM_SHIFT)
#define DEFAULT_THREAD_NAME                 "[networkd]"
#define DEFAULT_THREAD_STACKSIZE            (1024 << 10)
#define DEFAULT_THREAD_PRIO                 (0)
#define DEFAULT_RAW_RECVMBOX_SIZE           (64 << NUM_SHIFT)
#define DEFAULT_UDP_RECVMBOX_SIZE           (64 << NUM_SHIFT)
#define DEFAULT_TCP_RECVMBOX_SIZE           (64 << NUM_SHIFT)
#define DEFAULT_ACCEPTMBOX_SIZE             (64 << NUM_SHIFT)

// * Netconn API Options *
#define LWIP_NETCONN                        1
#define LWIP_TCPIP_TIMEOUT                  0
#define LWIP_NETCONN_SEM_PER_THREAD         0
#define LWIP_NETCONN_FULLDUPLEX             0

// * Socket Options *
#define LWIP_SOCKET                         1
#define LWIP_COMPAT_SOCKETS                 0
#define LWIP_POSIX_SOCKETS_IO_NAMES         0
#define LWIP_SOCKET_OFFSET                  0 // FIXME
#define LWIP_TCP_KEEPALIVE                  1
#define LWIP_SO_SNDTIMEO                    1
#define LWIP_SO_RCVTIMEO                    1
#define LWIP_SO_SNDRCVTIMEO_NONSTANDARD     0 // FIXME
#define LWIP_SO_RCVBUF                      1
#define LWIP_SO_LINGER                      1
#define SO_REUSE                            1
#define SO_REUSE_RXTOALL                    0
#define LWIP_FIONREAD_LINUXMODE             0
#define LWIP_SOCKET_SELECT                  1
#define LWIP_SOCKET_POLL                    1

// * Stats Options *
#define LWIP_STATS                          1

// * Checksum Options *
#define LWIP_CHECKSUM_CTRL_PER_NETIF        1

// * IPv6 Options *
#define LWIP_IPV6                           1
#define LWIP_IPV6_FORWARD                   1
#define LWIP_IPV6_FRAG                      1
#define LWIP_IPV6_DHCP6                     1
#define LWIP_DHCP6_GET_NTP_SRV              LWIP_DHCP_GET_NTP_SRV
#define LWIP_DHCP6_MAX_NTP_SERVERS          LWIP_DHCP_MAX_NTP_SERVERS

//* Performance Tracking Options *
#define LWIP_PERF                           0




#if defined(DEBUG)
#define LWIP_DEBUG                          1
#else
#define LWIP_DEBUG                          0
#endif

#if defined(DEBUG)
#define LWIP_NOASSERT                       0
#else
#define LWIP_NOASSERT                       1
#endif


#define NO_SYS                              0
#define LWIP_DBG_TYPES_ON                   LWIP_DBG_OFF
#define NETIF_DEBUG                         LWIP_DBG_OFF
#define PBUF_DEBUG                          LWIP_DBG_OFF
#define INET_DEBUG                          LWIP_DBG_OFF
#define IP_DEBUG                            LWIP_DBG_OFF
#define MEM_DEBUG                           LWIP_DBG_OFF
#define RAW_DEBUG                           LWIP_DBG_OFF
#define SYS_DEBUG                           LWIP_DBG_OFF
#define TIMERS_DEBUG                        LWIP_DBG_OFF
#define TCP_DEBUG                           LWIP_DBG_OFF
#define TCPIP_DEBUG                         LWIP_DBG_OFF
#define UDP_DEBUG                           LWIP_DBG_OFF
#define DHCP_DEBUG                          LWIP_DBG_OFF
#define DNS_DEBUG                           LWIP_DBG_OFF
#define TCP_INPUT_DEBUG                     LWIP_DBG_OFF
#define SOCKETS_DEBUG                       LWIP_DBG_OFF
#define API_LIB_DEBUG                       LWIP_DBG_OFF
#define API_MSG_DEBUG                       LWIP_DBG_OFF



// * Arch Options *
#define LWIP_NO_CTYPE_H                     1
#define LWIP_ERRNO_STDINCLUDE               0
#define LWIP_ERRNO_INCLUDE                  <aplus/errno.h>
#define LWIP_TIMEVAL_PRIVATE                0

#define SSIZE_MAX                           INT_MAX
//#define SA_FAMILY_T_DEFINED                 1


// * Sys Options *
#define LWIP_COMPAT_MUTEX                  1
#define LWIP_COMPAT_MUTEX_ALLOWED          1


extern spinlock_t tcpip_core_locking;

#define LOCK_TCPIP_CORE()                   \
    spinlock_lock(&tcpip_core_locking);

#define UNLOCK_TCPIP_CORE()                 \
    spinlock_unlock(&tcpip_core_locking);


static inline void* __kmalloc(size_t s) {
    return kmalloc(s, GFP_ATOMIC);
}

static inline void* __kcalloc(size_t n, size_t m) {
    return kcalloc(n, m, GFP_ATOMIC);
}


#define mem_clib_malloc                     __kmalloc
#define mem_clib_free                       kfree
#define mem_clib_calloc                     __kcalloc


WARNING("-Waddress")
WARNING("-Wparentheses")
WARNING("-Warray-bounds")
WARNING("-Wformat")

#endif
