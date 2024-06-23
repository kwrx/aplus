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

#include <stdint.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/task.h>


#include "lwip/opt.h"

#include "lwip/autoip.h"
#include "lwip/dns.h"
#include "lwip/igmp.h"
#include "lwip/init.h"
#include "lwip/ip.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/raw.h"
#include "lwip/sockets.h"
#include "lwip/stats.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "netif/etharp.h"



void dhcp_set_ntp_servers(uint8_t num_ntp_servers, ip_addr_t* ntp_server_addrs) {
    kpanicf("%s(): PANIC! not implemented! num_ntp_servers(%d)", __func__, num_ntp_servers);
}

void dhcp6_set_ntp_servers(uint8_t num_ntp_servers, ip_addr_t* ntp_server_addrs) {
    kpanicf("%s(): PANIC! not implemented! num_ntp_servers(%d)", __func__, num_ntp_servers);
}
