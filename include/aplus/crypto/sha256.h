#ifndef _APLUS_CRYPTO_SHA256_H
#define _APLUS_CRYPTO_SHA256_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>

#define SHA256_DIGEST_SIZE                32

typedef struct {
    uint32_t state[8];
    uint64_t count;
    unsigned char buffer[64];
} sha256_t;

void sha256_init(sha256_t* context);
void sha256_update(sha256_t* context, const unsigned char *data, size_t len);
void sha256_final(sha256_t* context, unsigned char digest[SHA256_DIGEST_SIZE]);
char* sha256(const char *str);


#ifdef __cplusplus
}
#endif
#endif