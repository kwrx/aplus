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


#include <aplus/base.h>
#include <aplus/utils/rotate-bits.h>
#include <aplus/crypto/sha384.h>
#include <aplus/crypto/sha512.h>
#include <string.h>
#include <stdio.h>


void
sha384_init(sha384_t *ctx)
{
    ctx->h_dig.h[0] = 0xCBBB9D5DC1059ED8LL;
    ctx->h_dig.h[1] = 0x629A292A367CD507LL;
    ctx->h_dig.h[2] = 0x9159015A3070DD17LL;
    ctx->h_dig.h[3] = 0x152FECD8F70E5939LL;
    ctx->h_dig.h[4] = 0x67332667FFC00B31LL;
    ctx->h_dig.h[5] = 0x8EB44A8768581511LL;
    ctx->h_dig.h[6] = 0xDB0C2E0D64F98FA7LL;
    ctx->h_dig.h[7] = 0x47B5481DBEFA4FA4LL;

    ctx->size =
    ctx->totalSize = 0;
}

char*
sha384(const char* data)
{
  char* out = (char*) __libaplus_malloc(SHA384_DIGEST_SIZE * 2);

  sha384_t hash;
  char buf[SHA384_DIGEST_SIZE];
  sha384_init(&hash);
  sha384_update(&hash, data, strlen(data));
  sha384_final(&hash, buf);


  int i;
  for(i = 0; i < SHA384_DIGEST_SIZE; i++)
      sprintf(&out[i * 2], "%02x", buf[i] & 0xFF);


  return out;
}


void
sha384_update(sha384_t *p, const unsigned char *data, size_t size)
{
  sha512_update((sha512_t*) p, data, size);
}


void
sha384_final(sha384_t *p, unsigned char _digest[SHA384_DIGEST_SIZE])
{
    unsigned char d[SHA512_DIGEST_SIZE];
  sha512_final((sha512_t*) p, d);
  memcpy(_digest, d, SHA384_DIGEST_SIZE);
}