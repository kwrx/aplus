#ifndef _APLUS_ENDIAN_H
#define _APLUS_ENDIAN_H


#ifndef __ASSEMBLY__

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#define be16_to_cpu(i)  __builtin_bswap16(i)
#define be32_to_cpu(i)  __builtin_bswap32(i)
#define be64_to_cpu(i)  __builtin_bswap64(i)

#define cpu_to_be16(i)  __builtin_bswap16(i)
#define cpu_to_be32(i)  __builtin_bswap32(i)
#define cpu_to_be64(i)  __builtin_bswap64(i)

#define le16_to_cpu(i)  (i)
#define le32_to_cpu(i)  (i)
#define le64_to_cpu(i)  (i)

#define cpu_to_le16(i)  (i)
#define cpu_to_le32(i)  (i)
#define cpu_to_le64(i)  (i)


#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

#define le16_to_cpu(i)  __builtin_bswap16(i)
#define le32_to_cpu(i)  __builtin_bswap32(i)
#define le64_to_cpu(i)  __builtin_bswap64(i)

#define cpu_to_le16(i)  __builtin_bswap16(i)
#define cpu_to_le32(i)  __builtin_bswap32(i)
#define cpu_to_le64(i)  __builtin_bswap64(i)

#define be16_to_cpu(i)  (i)
#define be32_to_cpu(i)  (i)
#define be64_to_cpu(i)  (i)

#define cpu_to_be16(i)  (i)
#define cpu_to_be32(i)  (i)
#define cpu_to_be64(i)  (i)

#else
#error "__BYTE_ORDER__ not supported or not defined"
#endif

#endif
#endif