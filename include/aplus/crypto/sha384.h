#ifndef _APLUS_CRYPTO_SHA384_H
#define _APLUS_CRYPTO_SHA384_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>

#define SHA384_DIGEST_SIZE                48

typedef struct {
    union {
        uint64_t h[8];
        uint8_t digest[64];
    } h_dig;

    union {
        uint64_t w[80];
        uint8_t buffer[128];
    } w_buf;

    size_t size;
    uint64_t totalSize;
} sha384_t;

void sha384_init(sha384_t* context);
void sha384_update(sha384_t* context, const unsigned char *data, size_t len);
void sha384_final(sha384_t* context, unsigned char digest[SHA384_DIGEST_SIZE]);
char* sha384(const char *str);


#ifdef __cplusplus
}
#endif
#endif