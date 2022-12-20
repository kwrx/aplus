/*                                                                      
 * GPL3 License                                                         
 *                                                                      
 * Author(s):                                                              
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
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
                                                                        
#include <stdint.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/types.h>
#include <aplus.h>
#include <aplus/debug.h>



__nosanitize("undefined")
static void dec(intmax_t __v, ssize_t padding, char* buf, int* offset, bool negative) {

    if(negative) {

        if(__v < 0LL) {

            __v = -__v;

            buf[*offset + 0] = '-';
            buf[*offset + 1] = '\0';

            *offset += 1;

        }

    }


    uintmax_t v = (uintmax_t) __v;

    ssize_t digits   = 1;
    ssize_t radix_10 = 9;

    while(v > radix_10) {
        digits++;
        radix_10 = (radix_10 * 10) + 9;
    }


    if(padding > digits) {

        ssize_t i;

        for(i = 0; i < (padding - digits); i++) {
            buf[*offset + i] = '0';
        }

        buf[*offset + i] = '\0';

        *offset += i;

    }


    for(size_t i = digits; i > 0; i--) {

        buf[*offset + (i - 1)] = (v % 10) + '0';
        v /= 10;

    }

    *offset += digits;

}



__nosanitize("undefined")
static void hex(uintmax_t v, ssize_t padding, char* buf, int* offset, bool upper) {
    
    ssize_t digits   = 1;
    ssize_t radix_16 = 15;

    while(v > radix_16) {
        digits++;
        radix_16 = (radix_16 * 16) + 15;
    }

    if(padding > digits) {

        ssize_t i;

        for(i = 0; i < (padding - digits); i++) {
            buf[*offset + i] = '0';
        }

        buf[*offset + i] = '\0';

        *offset += i;

    }

    
    for(size_t i = digits; i > 0; i--) {

        uintmax_t digit = (v % 16);

        if(digit < 10)
            buf[*offset + (i - 1)] = digit + '0';
        else
            buf[*offset + (i - 1)] = (upper ? 'A' : 'a') + (digit - 10);

        v /= 16;

    }

    *offset += digits;

}


__nosanitize("undefined")
static void oct(uintmax_t v, ssize_t padding, char* buf, int* offset) {
    
    ssize_t digits  = 1;
    ssize_t radix_8 = 7;

    while(v > radix_8) {
        digits++;
        radix_8 = (radix_8 * 8) + 7;
    }

    if(padding > digits) {

        ssize_t i;

        for(i = 0; i < (padding - digits); i++) {
            buf[*offset + i] = '0';
        }

        buf[*offset + i] = '\0';

        *offset += i;

    }

    
    for(size_t i = digits; i > 0; i--) {

        buf[*offset + (i - 1)] = (v % 8) + '0';
        v /= 8;

    }

    *offset += digits;

}

__nosanitize("undefined")
int vsnprintf(char* buf, size_t size, const char* fmt, va_list v) {

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);
    DEBUG_ASSERT(fmt);

    int p = 0;

    for(; *fmt; fmt++) {

        if(p > size - 1)
            break;

        if(*fmt != '%') {
            buf[p++] = *fmt; 
            continue; 
        }

        fmt++;
        
        if(*fmt == '\0') {
            kpanicf("vsnprintf: invalid format string '%s'\n", fmt);
        }
        

        long w = 0;
        long m = LONG_MAX;
        long l = sizeof(int);

        if(*fmt == '*') {

            w = va_arg(v, int);

            fmt++;

        } else if(*fmt >= '0' && *fmt <= '9') {

            do {

                w *= 10;
                w += (*fmt - '0');

                fmt++;

            } while(*fmt >= '0' && *fmt <= '9');
            
        }


        if(*fmt == '.') {

            fmt++;

            if(*fmt == '*') {

                m = va_arg(v, int);

                fmt++;
            
            } else if(*fmt >= '0' && *fmt <= '9') {

                m = 0;

                do {

                    m *= 10;
                    m += (*fmt - '0');

                    fmt++;

                } while(*fmt >= '0' && *fmt <= '9');
                
            }

        } else {

            if(*fmt == 'z') { // size_t
                
                l = sizeof(size_t);

                fmt++;

            } else if(*fmt == 'j') { // intmax_t
                
                l = sizeof(intmax_t);

                fmt++;

            } else if(*fmt == 't') { // ptrdiff_t
                    
                l = sizeof(ptrdiff_t);

                fmt++;

            } else if(*fmt == 'h') { // short

                l = sizeof(short);

                fmt++;

                if (*fmt == 'h') { // char

                    l = sizeof(char);
                    fmt++;

                }

            } else if(*fmt == 'l') { // long

                l = sizeof(long);

                fmt++;

                if (*fmt == 'l') { // long long

                    l = sizeof(long long);
                    fmt++;

                }

            } else if(*fmt == 'L') { // long double

                kpanicf("vsnprintf: unsupported format specifier 'L'\n");

            }

        }


        switch(*fmt) {

            case 's':

                if(w > m) {
                    w = m;
                }

                if(w > 0) {

                    size_t i = 0;

                    for(char* s = va_arg(v, char*); s && *s && m--; s++, i++) {
                        buf[p++] = *s;
                    }

                    for(; i < w; i++) {
                        buf[p++] = ' ';
                    }

                } else {
                    
                    for(char* s = va_arg(v, char*); s && *s && m--; s++) {
                        buf[p++] = *s;
                    }
                
                }



                break;

            case 'c':

                buf[p++] = ((int8_t) va_arg(v, int));
                break;

            case 'x':
            case 'X':

                switch(l) {
                    case 1: hex((uintmax_t) ((uint8_t)  va_arg(v, int)), w, buf, &p, (*fmt == 'X')); break;
                    case 2: hex((uintmax_t) ((uint16_t) va_arg(v, int)), w, buf, &p, (*fmt == 'X')); break;
                    case 4: hex((uintmax_t) ((uint32_t) va_arg(v, int)), w, buf, &p, (*fmt == 'X')); break;
                    case 8: hex((uintmax_t) ((uint64_t) va_arg(v, int64_t)), w, buf, &p, (*fmt == 'X')); break;
                    default: DEBUG_ASSERT(0); break;
                }

                break;

            case 'o':

                switch(l) {
                    case 1: oct((uintmax_t) ((uint8_t)  va_arg(v, int)), w, buf, &p); break;
                    case 2: oct((uintmax_t) ((uint16_t) va_arg(v, int)), w, buf, &p); break;
                    case 4: oct((uintmax_t) ((uint32_t) va_arg(v, int)), w, buf, &p); break;
                    case 8: oct((uintmax_t) ((uint64_t) va_arg(v, int64_t)), w, buf, &p); break;
                    default: DEBUG_ASSERT(0); break;
                }

                break;

            case 'p':

                fmt++;

                if(*fmt == 'h') {

                    fmt++;

                    char space = ' ';

                    if(*fmt == 'C') {
                        space = ':';
                    } else if(*fmt == 'D') {
                        space = '-';
                    } else {
                        fmt--;
                    }

                    for(uint8_t* bytes = va_arg(v, void*); bytes && m--; bytes++) {
                        hex(*bytes, 2, buf, &p, 1);
                        buf[p++] = space;
                    }

                } else {

                    fmt--;

                    buf[p++] = '0';
                    buf[p++] = 'x';
                    hex((intmax_t) va_arg(v, void*), w, buf, &p, 1);

                }

                break;

            case 'd':
            case 'i':
            case 'u':

                switch(l) {
                    case 1: dec((intmax_t) ((int8_t)  va_arg(v, int)), w, buf, &p, *fmt != 'u'); break;
                    case 2: dec((intmax_t) ((int16_t) va_arg(v, int)), w, buf, &p, *fmt != 'u'); break;
                    case 4: dec((intmax_t) ((int32_t) va_arg(v, int)), w, buf, &p, *fmt != 'u'); break;
                    case 8: dec((intmax_t) ((int64_t) va_arg(v, int64_t)), w, buf, &p, *fmt != 'u'); break;
                    default: DEBUG_ASSERT(0); break;
                }

                break;


            case 'n':

                switch(l) {
                    case 1: *((int8_t*)  va_arg(v, int*)) = p; break;
                    case 2: *((int16_t*) va_arg(v, int*)) = p; break;
                    case 4: *((int32_t*) va_arg(v, int*)) = p; break;
                    case 8: *((int64_t*) va_arg(v, int64_t*)) = p; break;
                    default: DEBUG_ASSERT(0); break;
                }

                break;

            case '%':

                buf[p++] = '%';
                break;

            default:

                kpanicf("vsnprintf: unsupported format specifier '%c'\n", *fmt);

                buf[p++] = *fmt;
                break;

        }

    }

    buf[p++] = '\0';


    return p;

}
