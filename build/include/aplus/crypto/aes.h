#ifndef _APLUS_CRYPTO_AES_H
#define _APLUS_CRYPTO_AES_H

#include <stdint.h>
#include <sys/types.h>

#define AES_MAXROUNDS                       14
#define AES_BLOCKSIZE                       16
#define AES_IV_SIZE                         16

#define AES_MODE_128                        128
#define AES_MODE_256                        256

typedef struct {
    uint16_t rounds;
    uint16_t key_size;
    uint32_t ks[(AES_MAXROUNDS + 1) * 8];
    uint8_t iv[AES_IV_SIZE];
} aes_t;


void aes_key(aes_t* ctx, const uint8_t* key, const uint8_t* iv, int mode);
void aes_encrypt(aes_t* ctx, const uint8_t* msg, uint8_t* outdata, size_t size);
void aes_decrypt(aes_t* ctx, const uint8_t* msg, uint8_t* outdata, size_t size);

#endif