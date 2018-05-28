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