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


#ifndef _APLUS_NETWORK_H
#define _APLUS_NETWORK_H


#ifndef __ASSEMBLY__


    #include <time.h>

    #include <aplus.h>
    #include <aplus/vfs.h>

    #include "lwip/opt.h"
    #include "lwipopts.h"

    #include "lwip/autoip.h"
    #include "lwip/dns.h"
    #include "lwip/igmp.h"
    #include "lwip/init.h"
    #include "lwip/ip.h"
    #include "lwip/ip_addr.h"
    #include "lwip/mem.h"
    #include "lwip/memp.h"
    #include "lwip/netif.h"
    #include "lwip/pbuf.h"
    #include "lwip/raw.h"
    #include "lwip/snmp.h"
    #include "lwip/sockets.h"
    #include "lwip/stats.h"
    #include "lwip/sys.h"
    #include "lwip/tcpip.h"
    #include "lwip/udp.h"
    #include "netif/etharp.h"



__BEGIN_DECLS

/**
 * @brief An lwIP socket is an ordinary descriptor backed by an anonymous inode.
 */

int socket_install(int socket, int flags);
int socket_from_fd(int fd);
int socket_from_inode(inode_t* inode);
int socket_poll_arm(inode_t* inode, int events, struct timespec* timeout);

/**
 * @brief Largest socket option value the kernel will carry between user and lwIP.
 */
    #define SOCKOPT_MAX_OPTLEN 256

/**
 * @brief Translate a Linux socket option level and name into the lwIP values.
 * @param level In/out, the Linux level on entry and the lwIP one on success.
 * @param optname In/out, the Linux option on entry and the lwIP one on success.
 * @return 0 on success, -ENOPROTOOPT when lwIP has no equivalent.
 */
long socket_sockopt_translate(int* level, int* optname);


void network_init(void);

void ethif_input(struct netif* netif);
err_t ethif_init(struct netif* netif);

__END_DECLS

#endif
#endif
