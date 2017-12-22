#ifndef _APLUS_CRYPTO_SHA1_H
#define _APLUS_CRYPTO_SHA1_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>

#define SHA1_DIGEST_SIZE                20

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} sha1_t;

void sha1_init(sha1_t* context);
void sha1_update(sha1_t* context, const unsigned char *data, size_t len);
void sha1_final(sha1_t* context, unsigned char digest[SHA1_DIGEST_SIZE]);
char* sha1(const char *str);


#ifdef __cplusplus
}
#endif
#endif