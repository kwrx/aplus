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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <stdint.h>

#include <aplus/network.h>


#define LINUX_SOL_SOCKET 1

#define LINUX_SO_DEBUG        1
#define LINUX_SO_REUSEADDR    2
#define LINUX_SO_TYPE         3
#define LINUX_SO_ERROR        4
#define LINUX_SO_DONTROUTE    5
#define LINUX_SO_BROADCAST    6
#define LINUX_SO_SNDBUF       7
#define LINUX_SO_RCVBUF       8
#define LINUX_SO_KEEPALIVE    9
#define LINUX_SO_OOBINLINE    10
#define LINUX_SO_NO_CHECK     11
#define LINUX_SO_LINGER       13
#define LINUX_SO_REUSEPORT    15
#define LINUX_SO_RCVLOWAT     18
#define LINUX_SO_SNDLOWAT     19
#define LINUX_SO_RCVTIMEO     20
#define LINUX_SO_SNDTIMEO     21
#define LINUX_SO_BINDTODEVICE 25
#define LINUX_SO_ACCEPTCONN   30

#define LINUX_IP_TOS             1
#define LINUX_IP_TTL             2
#define LINUX_IP_PKTINFO         8
#define LINUX_IP_MULTICAST_IF    32
#define LINUX_IP_MULTICAST_TTL   33
#define LINUX_IP_MULTICAST_LOOP  34
#define LINUX_IP_ADD_MEMBERSHIP  35
#define LINUX_IP_DROP_MEMBERSHIP 36

#define LINUX_TCP_NODELAY   1
#define LINUX_TCP_KEEPIDLE  4
#define LINUX_TCP_KEEPINTVL 5
#define LINUX_TCP_KEEPCNT   6

#define LINUX_IPV6_CHECKSUM    7
#define LINUX_IPV6_JOIN_GROUP  20
#define LINUX_IPV6_LEAVE_GROUP 21
#define LINUX_IPV6_V6ONLY      26


/**
 * @brief One Linux socket option and the lwIP option it corresponds to.
 */
struct sockopt_entry {
    int from;
    int to;
};

static const struct sockopt_entry sockopt_socket[] = {
    {LINUX_SO_DEBUG,        SO_DEBUG       },
    {LINUX_SO_REUSEADDR,    SO_REUSEADDR   },
    {LINUX_SO_TYPE,         SO_TYPE        },
    {LINUX_SO_ERROR,        SO_ERROR       },
    {LINUX_SO_DONTROUTE,    SO_DONTROUTE   },
    {LINUX_SO_BROADCAST,    SO_BROADCAST   },
    {LINUX_SO_SNDBUF,       SO_SNDBUF      },
    {LINUX_SO_RCVBUF,       SO_RCVBUF      },
    {LINUX_SO_KEEPALIVE,    SO_KEEPALIVE   },
    {LINUX_SO_OOBINLINE,    SO_OOBINLINE   },
    {LINUX_SO_NO_CHECK,     SO_NO_CHECK    },
    {LINUX_SO_LINGER,       SO_LINGER      },
    {LINUX_SO_REUSEPORT,    SO_REUSEPORT   },
    {LINUX_SO_RCVLOWAT,     SO_RCVLOWAT    },
    {LINUX_SO_SNDLOWAT,     SO_SNDLOWAT    },
    {LINUX_SO_RCVTIMEO,     SO_RCVTIMEO    },
    {LINUX_SO_SNDTIMEO,     SO_SNDTIMEO    },
    {LINUX_SO_BINDTODEVICE, SO_BINDTODEVICE},
    {LINUX_SO_ACCEPTCONN,   SO_ACCEPTCONN  },
};

static const struct sockopt_entry sockopt_ip[] = {
    {LINUX_IP_TOS,             IP_TOS            },
    {LINUX_IP_TTL,             IP_TTL            },
    #if LWIP_IPV4 && LWIP_MULTICAST_TX_OPTIONS
    {LINUX_IP_MULTICAST_IF,    IP_MULTICAST_IF   },
    {LINUX_IP_MULTICAST_TTL,   IP_MULTICAST_TTL  },
    {LINUX_IP_MULTICAST_LOOP,  IP_MULTICAST_LOOP },
    #endif
    #if LWIP_IPV4 && LWIP_IGMP
    {LINUX_IP_ADD_MEMBERSHIP,  IP_ADD_MEMBERSHIP },
    {LINUX_IP_DROP_MEMBERSHIP, IP_DROP_MEMBERSHIP},
    #endif
    #if LWIP_IPV4 && IP_SOF_BROADCAST
    {LINUX_IP_PKTINFO,         IP_PKTINFO        },
    #endif
};

static const struct sockopt_entry sockopt_tcp[] = {
    {LINUX_TCP_NODELAY,   TCP_NODELAY  },
    #if LWIP_TCP_KEEPALIVE
    {LINUX_TCP_KEEPIDLE,  TCP_KEEPIDLE },
    {LINUX_TCP_KEEPINTVL, TCP_KEEPINTVL},
    {LINUX_TCP_KEEPCNT,   TCP_KEEPCNT  },
    #endif
};

    #if LWIP_IPV6
static const struct sockopt_entry sockopt_ipv6[] = {
    {LINUX_IPV6_CHECKSUM,    IPV6_CHECKSUM   },
    {LINUX_IPV6_V6ONLY,      IPV6_V6ONLY     },
        #if LWIP_IPV6_MLD
    {LINUX_IPV6_JOIN_GROUP,  IPV6_JOIN_GROUP },
    {LINUX_IPV6_LEAVE_GROUP, IPV6_LEAVE_GROUP},
        #endif
};
    #endif


/**
 * @brief Translate a Linux socket option level and name into the lwIP values.
 * @param level In/out, the Linux level on entry and the lwIP one on success.
 * @param optname In/out, the Linux option on entry and the lwIP one on success.
 * @return 0 on success, -ENOPROTOOPT when lwIP has no equivalent.
 */
long socket_sockopt_translate(int* level, int* optname) {

    DEBUG_ASSERT(level);
    DEBUG_ASSERT(optname);

    const struct sockopt_entry* entries;
    size_t count;
    int lwip_level;

    switch (*level) {

        case LINUX_SOL_SOCKET:
            entries    = sockopt_socket;
            count      = sizeof(sockopt_socket) / sizeof(sockopt_socket[0]);
            lwip_level = SOL_SOCKET;
            break;

        case IPPROTO_IP:
            entries    = sockopt_ip;
            count      = sizeof(sockopt_ip) / sizeof(sockopt_ip[0]);
            lwip_level = IPPROTO_IP;
            break;

        case IPPROTO_TCP:
            entries    = sockopt_tcp;
            count      = sizeof(sockopt_tcp) / sizeof(sockopt_tcp[0]);
            lwip_level = IPPROTO_TCP;
            break;

    #if LWIP_IPV6
        case IPPROTO_IPV6:
            entries    = sockopt_ipv6;
            count      = sizeof(sockopt_ipv6) / sizeof(sockopt_ipv6[0]);
            lwip_level = IPPROTO_IPV6;
            break;
    #endif

        default:
            return -ENOPROTOOPT;
    }

    for (size_t i = 0; i < count; i++) {

        if (entries[i].from != *optname)
            continue;

        *level   = lwip_level;
        *optname = entries[i].to;

        return 0;
    }

    return -ENOPROTOOPT;
}
