#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <aplus/core/base.h>
#include <aplus/core/debug.h>



static void dec(unsigned long v, int w, char* buf, int* p) {

    int n = 1;
    int i = 9;

    while(v > i) {
        n++;
        i = (i * 10) + 9;
    }

    int o = 0;
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
    
    int n = 1;
    int i = 0xF;

    while(v > i) {
        n++;
        i = (i * 0x10) + 0xF;
    }

    int o = 0;
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
    DEBUG_ASSERT(fmt);


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
                hex((unsigned long) va_arg(v, unsigned long), w, buf, &p, 0);
                break;

            case 'X':
                hex((unsigned long) va_arg(v, unsigned long), w, buf, &p, 1);
                break;

            case 'p':
                buf[p++] = '0';
                buf[p++] = 'x';
                hex((unsigned long) va_arg(v, unsigned long), w, buf, &p, 1);
                break;

            case 'd':
                dec((unsigned int) va_arg(v, unsigned int), w, buf, &p);
                break;

            case '%':
                buf[p++] = '%';
                break;

            default:
                buf[p++] = *fmt;
                break;

        }

    }

    buf[p++] = '\0';
    return p;

}