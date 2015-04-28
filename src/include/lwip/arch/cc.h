#ifndef _CC_H
#define _CC_H

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

#define PACK_STRUCT_FIELD(x)	x
#define PACK_STRUCT_STRUCT		__attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END


#include <stdio.h>
#include <stdlib.h>
#define LWIP_PLATFORM_DIAG(x)	do { printf x; } while(0)
#define LWIP_PLATFORM_ASSERT(x)

#define LWIP_RAND() ((u32_t)rand())


#define SYS_ARCH_DECL_PROTECT(x)
#define SYS_ARCH_PROTECT(x)
#define SYS_ARCH_UNPROTECT(x)

#define BYTE_ORDER	LITTLE_ENDIAN
#define LWIP_CHKSUM_ALGORITHM 2

#endif
