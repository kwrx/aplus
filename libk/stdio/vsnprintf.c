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
#include <sys/types.h>
#include <aplus.h>
#include <aplus/debug.h>


static void dec(unsigned long v, int w, char* buf, int* p, int sign) {

    if(sign) {

        if((long) v < 0L) {

            v = -((long) v);

            buf[*p + 0] = '-';
            buf[*p + 1] = '\0';

            *p += 1;

        }

    }


    long n = 1;
    long i = 9;

    while(v > i) {
        n++;
        i = (i * 10) + 9;
    }

    long o = 0;
    while(n + o < w)
        { buf[*p] = '0'; *p += 1; o++; }


    i = n;
    while(i > 0) {

        buf[*p + --i] = (v % 10) + '0';
        v /= 10;

    }

    *p += n;

}

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


int vsnprintf(char* buf, size_t size, const char* fmt, va_list v) {

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);
    //DEBUG_ASSERT(fmt);

    int p = 0;

    for(; *fmt; fmt++) {

        if(p > size - 1)
            break;

        if(*fmt != '%')
            { buf[p++] = *fmt; continue; }

        fmt++;
        DEBUG_ASSERT(*fmt);

        
        int w = 0;
        while(*fmt >= '0' && *fmt <= '9')
            { w = (w * 10) + (*fmt - '0'); fmt++; }



        switch(*fmt) {

            case 's':
                for(char* s = (char*) va_arg(v, char*); *s; s++)
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
                buf[p++] = '0';
                buf[p++] = 'x';
                hex((unsigned long) va_arg(v, unsigned long), w, buf, &p, 1);
                break;

            case 'd':
            case 'u':
                dec((unsigned long) va_arg(v, unsigned long), w, buf, &p, *fmt == 'd');
                break;

            case '%':
                buf[p++] = '%';
                break;

            default:

                kprintf("%s (): WARN! invalid format %c\n", *fmt);

                buf[p++] = *fmt;
                break;

        }

    }

    buf[p++] = '\0';
    return p;

}
