/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


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