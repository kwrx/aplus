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


#ifndef _APLUS_CRYPTO_SHA512_H
#define _APLUS_CRYPTO_SHA512_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>

#define SHA512_DIGEST_SIZE                64

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
} sha512_t;

void sha512_init(sha512_t* context);
void sha512_update(sha512_t* context, const unsigned char *data, size_t len);
void sha512_final(sha512_t* context, unsigned char digest[SHA512_DIGEST_SIZE]);
char* sha512(const char *str);


#ifdef __cplusplus
}
#endif
#endif