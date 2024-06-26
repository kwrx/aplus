/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <aplus/base.h>
#include <aplus/crypto/base64.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char b64_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};


char* base64_encode(const unsigned char* src, size_t len) {
    int i       = 0;
    int j       = 0;
    char* enc   = NULL;
    size_t size = 0;
    unsigned char buf[4];
    unsigned char tmp[3];

    // alloc
    enc = (char*)__libaplus_malloc(1);
    if (NULL == enc) {
        return NULL;
    }

    // parse until end of source
    while (len--) {
        // read up to 3 bytes at a time into `tmp'
        tmp[i++] = *(src++);

        // if 3 bytes read then encode into `buf'
        if (3 == i) {
            buf[0] = (tmp[0] & 0xfc) >> 2;
            buf[1] = ((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4);
            buf[2] = ((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6);
            buf[3] = tmp[2] & 0x3f;

            // allocate 4 new byts for `enc` and
            // then translate each encoded buffer
            // part by index from the base 64 index table
            // into `enc' unsigned char array
            char* p = (char*)__libaplus_malloc(size + 4);
            if (!p)
                return NULL;
            memcpy(p, enc, size);
            __libaplus_free(enc);
            enc = p;

            for (i = 0; i < 4; ++i) {
                enc[size++] = b64_table[buf[i]];
            }

            // reset index
            i = 0;
        }
    }

    // remainder
    if (i > 0) {
        // fill `tmp' with `\0' at most 3 times
        for (j = i; j < 3; ++j) {
            tmp[j] = '\0';
        }

        // perform same codec as above
        buf[0] = (tmp[0] & 0xfc) >> 2;
        buf[1] = ((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4);
        buf[2] = ((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6);
        buf[3] = tmp[2] & 0x3f;

        // perform same write to `enc` with new allocation
        for (j = 0; (j < i + 1); ++j) {
            char* p = (char*)__libaplus_malloc(size + 1);
            if (!p)
                return NULL;
            memcpy(p, enc, size);
            __libaplus_free(enc);
            enc         = p;
            enc[size++] = b64_table[buf[j]];
        }

        // while there is still a remainder
        // append `=' to `enc'
        while ((i++ < 3)) {
            char* p = (char*)__libaplus_malloc(size + 1);
            if (!p)
                return NULL;
            memcpy(p, enc, size);
            __libaplus_free(enc);
            enc         = p;
            enc[size++] = '=';
        }
    }

    // Make sure we have enough space to add '\0' character at end.
    char* p = (char*)__libaplus_malloc(size + 1);
    if (!p)
        return NULL;
    memcpy(p, enc, size);
    __libaplus_free(enc);
    enc       = p;
    enc[size] = '\0';

    return enc;
}



unsigned char* base64_decode(const char* src, size_t len) {
    int i              = 0;
    int j              = 0;
    int l              = 0;
    size_t size        = 0;
    unsigned char* dec = NULL;
    unsigned char buf[3];
    unsigned char tmp[4];

    // alloc
    dec = (unsigned char*)__libaplus_malloc(1);
    if (NULL == dec) {
        return NULL;
    }

    // parse until end of source
    while (len--) {
        // break if char is `=' or not base64 char
        if ('=' == src[j]) {
            break;
        }
        if (!(isalnum(src[j]) || '+' == src[j] || '/' == src[j])) {
            break;
        }

        // read up to 4 bytes at a time into `tmp'
        tmp[i++] = src[j++];

        // if 4 bytes read then decode into `buf'
        if (4 == i) {
            // translate values in `tmp' from table
            for (i = 0; i < 4; ++i) {
                // find translation char in `b64_table'
                for (l = 0; l < 64; ++l) {
                    if (tmp[i] == b64_table[l]) {
                        tmp[i] = l;
                        break;
                    }
                }
            }

            // decode
            buf[0] = (tmp[0] << 2) + ((tmp[1] & 0x30) >> 4);
            buf[1] = ((tmp[1] & 0xf) << 4) + ((tmp[2] & 0x3c) >> 2);
            buf[2] = ((tmp[2] & 0x3) << 6) + tmp[3];


            char* p = (unsigned char*)__libaplus_malloc(size + 3);
            if (!p)
                return NULL;
            memcpy(p, dec, size);
            __libaplus_free(dec);
            dec = p;

            for (i = 0; i < 3; ++i) {
                dec[size++] = buf[i];
            }

            // reset
            i = 0;
        }
    }

    // remainder
    if (i > 0) {
        // fill `tmp' with `\0' at most 4 times
        for (j = i; j < 4; ++j) {
            tmp[j] = '\0';
        }

        // translate remainder
        for (j = 0; j < 4; ++j) {
            // find translation char in `b64_table'
            for (l = 0; l < 64; ++l) {
                if (tmp[j] == b64_table[l]) {
                    tmp[j] = l;
                    break;
                }
            }
        }

        // decode remainder
        buf[0] = (tmp[0] << 2) + ((tmp[1] & 0x30) >> 4);
        buf[1] = ((tmp[1] & 0xf) << 4) + ((tmp[2] & 0x3c) >> 2);
        buf[2] = ((tmp[2] & 0x3) << 6) + tmp[3];

        // write remainer decoded buffer to `dec'
        char* p = (unsigned char*)__libaplus_malloc(size + (i - 1));
        if (!p)
            return NULL;
        memcpy(p, dec, size);
        __libaplus_free(dec);
        dec = p;

        for (j = 0; (j < i - 1); ++j) {
            dec[size++] = buf[j];
        }
    }

    // Make sure we have enough space to add '\0' character at end.
    char* p = (unsigned char*)__libaplus_malloc(size + 1);
    if (!p)
        return NULL;
    memcpy(p, dec, size);
    __libaplus_free(dec);
    dec       = p;
    dec[size] = '\0';


    return dec;
}
