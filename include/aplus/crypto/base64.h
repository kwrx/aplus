#ifndef _APLUS_CRYPTO_BASE64_H
#define _APLUS_CRYPTO_BASE64_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>

char* base64_encode(const unsigned char*, size_t);
unsigned char* base64_decode(const char*, size_t);

#ifdef __cplusplus
}
#endif
#endif