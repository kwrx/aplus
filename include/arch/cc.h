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


#ifndef _CC_H
#define _CC_H

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <stdlib.h>
#include <stdint.h>

typedef int8_t s8_t;
typedef int16_t s16_t;
typedef int32_t s32_t;

typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;

typedef uintptr_t mem_ptr_t;


#define U8_F "c"
#define S8_F "c"
#define X8_F "x"
#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "u"

#define PACK_STRUCT_FIELD(x)                x
#define PACK_STRUCT_STRUCT                  __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END


#define LWIP_PLATFORM_DIAG(x)    do { kprintf x; } while(0)
#define LWIP_PLATFORM_ASSERT(x)                                     \
    do {                                                            \
        kprintf ("tcpip: ASSERT! (%s, %d) %s\n",                    \
            __FILE__,                                               \
            __LINE__,                                               \
            x                                                       \
        );                                                          \
        for(;;);                                                    \
    } while(0)

#define LWIP_RAND() ((u32_t)rand())


#define SYS_ARCH_DECL_PROTECT(x)            extern spinlock_t network_lock;
#define SYS_ARCH_PROTECT(x)                 spinlock_lock(&network_lock);
#define SYS_ARCH_UNPROTECT(x)               spinlock_unlock(&network_lock);

#ifndef BYTE_ORDER
#if __BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__
#define BYTE_ORDER                          LITTLE_ENDIAN
#else
#define BYTE_ORDER                          BIG_ENDIAN
#endif
#endif

#define LWIP_CHKSUM_ALGORITHM               2

#endif
