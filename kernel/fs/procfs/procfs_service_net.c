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

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/memory.h>
#include <aplus/network.h>
#include <aplus/vfs.h>

#include "procfs.h"


/**
 * @brief The inode slots of /proc/net and the files in it.
 */
#define PROCFS_NET_SLOT_DIR   11
#define PROCFS_NET_SLOT_ROUTE 12

/**
 * @brief How many interfaces /proc/net/route reports, and the Linux route flags it uses.
 */
#define PROCFS_NET_IFACES_MAX  8
#define PROCFS_NET_RTF_UP      0x0001
#define PROCFS_NET_RTF_GATEWAY 0x0002


/**
 * @brief One interface's routes, copied out under the lwIP core lock.
 */
typedef struct {

    char name[16];

    uint32_t ip;
    uint32_t mask;
    uint32_t gw;
    uint16_t mtu;

    bool up;
    bool fallback;

} procfs_net_iface_t;


/**
 * @brief Generates /proc/net/route as Linux spells it: one subnet route per interface, and the default route.
 *
 * @param inode The /proc/net/route inode.
 * @param buf Receives the generated contents.
 * @param size Receives the length of the contents.
 * @param arg Unused.
 * @return 0 on success.
 */
static int procfs_service_net_route_fetch(inode_t* inode, char** buf, size_t* size, void* arg) {

    (void)arg;

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    procfs_net_iface_t ifaces[PROCFS_NET_IFACES_MAX];

    size_t count = 0;

    LOCK_TCPIP_CORE();

    struct netif* netif;

    NETIF_FOREACH(netif) {

        if (count == PROCFS_NET_IFACES_MAX)
            break;

        if (netif->name[0] == 'l' && netif->name[1] == 'o')
            continue;

        procfs_net_iface_t* iface = &ifaces[count++];

        snprintf(iface->name, sizeof(iface->name), "%c%c%u", netif->name[0], netif->name[1], (unsigned)netif->num);

        iface->ip       = ip4_addr_get_u32(netif_ip4_addr(netif));
        iface->mask     = ip4_addr_get_u32(netif_ip4_netmask(netif));
        iface->gw       = ip4_addr_get_u32(netif_ip4_gw(netif));
        iface->mtu      = netif->mtu;
        iface->up       = netif_is_up(netif);
        iface->fallback = netif == netif_default;
    }

    UNLOCK_TCPIP_CORE();


    procfs_buf_t b = procfs_scratch();

    procfs_bprintf(&b, "Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU\tWindow\tIRTT\n");

    for (size_t i = 0; i < count; i++) {

        if (!ifaces[i].up || ifaces[i].ip == 0)
            continue;

        if (ifaces[i].fallback && ifaces[i].gw != 0)
            procfs_bprintf(&b, "%s\t%08X\t%08X\t%04X\t0\t0\t0\t%08X\t%u\t0\t0\n", ifaces[i].name, 0U, ifaces[i].gw, PROCFS_NET_RTF_UP | PROCFS_NET_RTF_GATEWAY, 0U, (unsigned)ifaces[i].mtu);

        procfs_bprintf(&b, "%s\t%08X\t%08X\t%04X\t0\t0\t0\t%08X\t%u\t0\t0\n", ifaces[i].name, ifaces[i].ip & ifaces[i].mask, 0U, PROCFS_NET_RTF_UP, ifaces[i].mask, (unsigned)ifaces[i].mtu);
    }

    *buf  = b.data;
    *size = b.length;

    return 0;
}


/**
 * @brief Reports /proc/net/route, created the first time it is looked up.
 *
 * @param parent The /proc/net inode.
 * @return The inode.
 */
static inode_t* procfs_service_net_route_inode(inode_t* parent) {

    static inode_t* inode = NULL;

    if (inode == NULL) {
        inode      = procfs_service_inode(parent, "route", S_IFREG | 0444, procfs_service_net_route_fetch, NULL);
        inode->ino = PROCFS_INO_STATIC(PROCFS_NET_SLOT_ROUTE);
    }

    return inode;
}


/**
 * @brief Looks a name up in /proc/net.
 *
 * @param inode The /proc/net inode.
 * @param name The name.
 * @return The file, or NULL with errno set to ENOENT.
 */
static inode_t* procfs_service_net_finddir(inode_t* inode, const char* name) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);

    if (strcmp(name, "route") == 0)
        return procfs_service_net_route_inode(inode);

    return errno = ENOENT, NULL;
}


/**
 * @brief Lists /proc/net.
 *
 * @param inode The /proc/net inode.
 * @param e The array of dirents to fill in.
 * @param pos The ordinal of the first entry to report.
 * @param count The number of entries the caller's buffer holds.
 * @return The number of entries written, 0 past the end, or -1 with errno set.
 */
static ssize_t procfs_service_net_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(e);

    if (unlikely(pos < 0))
        return errno = EINVAL, -1;


    static const struct {
        const char* name;
        ino_t ino;
        unsigned char type;
    } entries[] = {
        {".",     PROCFS_INO_STATIC(PROCFS_NET_SLOT_DIR),   DT_DIR},
        {"..",    PROCFS_INO_ROOT,                          DT_DIR},
        {"route", PROCFS_INO_STATIC(PROCFS_NET_SLOT_ROUTE), DT_REG},
    };

    size_t i = 0;

    for (size_t j = (size_t)pos; j < sizeof(entries) / sizeof(entries[0]) && i < count; j++, i++) {

        e[i].d_ino    = entries[j].ino;
        e[i].d_off    = (off_t)i;
        e[i].d_type   = entries[j].type;
        e[i].d_reclen = sizeof(struct dirent);

        strncpy(e[i].d_name, entries[j].name, sizeof(e[i].d_name));
        e[i].d_name[sizeof(e[i].d_name) - 1] = '\0';
    }

    return (ssize_t)i;
}


/**
 * @brief Reports /proc/net, created the first time it is looked up.
 *
 * @param parent The /proc root.
 * @return The inode.
 */
inode_t* procfs_service_net_inode(inode_t* parent) {

    static inode_t* inode = NULL;

    if (inode == NULL) {
        inode              = procfs_service_inode(parent, "net", S_IFDIR | 0555, NULL, NULL);
        inode->ino         = PROCFS_INO_STATIC(PROCFS_NET_SLOT_DIR);
        inode->ops.finddir = procfs_service_net_finddir;
        inode->ops.readdir = procfs_service_net_readdir;
    }

    return inode;
}
