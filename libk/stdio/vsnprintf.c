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
static void dec(int64_t v, size_t padding, char* buf, int* offset, bool negative, size_t size) {

    switch(size) {

        case 0:
            v = (int64_t) ((int) v);
            break;

        case 1:
            v = (int64_t) ((long) v);
            break;

        case 2:
            v = (int64_t) ((long long) v);
            break;

        default:
            break;

    }


    if(negative) {

        if(v < 0LL) {

            v = -v;

            buf[*offset + 0] = '-';
            buf[*offset + 1] = '\0';

            *offset += 1;

        }

    }


    size_t digits   = 1;
    size_t radix_10 = 9;

    while(v > radix_10) {
        digits++;
        radix_10 = (radix_10 * 10) + 9;
    }


    if(padding > digits) {

        size_t i;

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
static void hex(unsigned long v, int w, char* buf, int* p, int upper) {
    
    long n = 1;
    long i = 0xF;

    while(v > i) {
        n++;
        i = (i * 0x10) + 0xF;
    }

    long o = 0;
    while(n + o < w)
        { buf[*p] = '0'; *p += 1; o++; }


    i = n;
    while(i > 0) {

        char t = "0123456789abcdef"[(v % 0x10)];

        if(upper && (t >= 'a' && t <= 'f'))
            t -= 32;

        buf[*p + --i] = t;
        v /= 0x10;

    }

    *p += n;

}

__nosanitize("undefined")
int vsnprintf(char* buf, size_t size, const char* fmt, va_list v) {

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);
    DEBUG_ASSERT(fmt);

    int p = 0;
    int l = 0;

    for(; *fmt; fmt++) {

        if(p > size - 1)
            break;

        if(*fmt != '%')
            { buf[p++] = *fmt; continue; }

        fmt++;
        DEBUG_ASSERT(*fmt);

        
        long w = 0;
        long m = LONG_MAX;


        if(*fmt == '*') {

            w = va_arg(v, long);

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

                m = va_arg(v, long);

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
                
                l = 1;

                fmt++;

            } else {

                if(*fmt == 'l') { // long
                    
                    l = 1;

                    fmt++;

                    if(*fmt == 'l') { // long long
                    
                        l = 2;
                    
                        fmt++;
                    
                    }

                }

            }

        }

        switch(*fmt) {

            case 's':
                for(char* s = (char*) va_arg(v, char*); s && *s && m--; s++)
                    buf[p++] = *s;

                break;

            case 'c':
                buf[p++] = (int) va_arg(v, int);
                break;

            case 'x':
            case 'X':
                hex((unsigned long) va_arg(v, unsigned long), w, buf, &p, *fmt == 'X');
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
                    hex((unsigned long) va_arg(v, unsigned long), w, buf, &p, 1);

                }

                break;

            case 'd':
            case 'u':
                dec((unsigned long) va_arg(v, unsigned long), w, buf, &p, *fmt == 'd', l);
                break;

            case '%':
                buf[p++] = '%';
                break;

            default:

                kprintf("%s(): WARN! invalid format %c\n", __func__, *fmt);

                buf[p++] = *fmt;
                break;

        }

        l = 0;

    }

    buf[p++] = '\0';
    return p;

}
