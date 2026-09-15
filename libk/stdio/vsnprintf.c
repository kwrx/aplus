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

#include <aplus.h>
#include <aplus/debug.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>


/* Every write goes through __emit(), which drops anything past the buffer while still
   counting it. Before this, each conversion wrote straight into `buf` with only a
   between-conversions bounds check, so a single %s or a wide number ran off the end --
   /proc/version overflowed a 64-byte static array that way and handed the bytes after it
   to userspace. `len` is the length the output *would* have had, which is what C says to
   return, so callers can detect truncation with `ret >= size`. */
struct __sbuf {
    char* buf;
    size_t size;
    size_t len;
};

__nosanitize("undefined") static inline void __emit(struct __sbuf* s, char c) {

    if (likely(s->len + 1 < s->size)) {
        s->buf[s->len] = c;
    }

    s->len++;
}

__nosanitize("undefined") static void __emit_num(struct __sbuf* s, uintmax_t v, unsigned base, bool upper, ssize_t padding, bool negative) {

    /* Widest case is a 64-bit value in octal: 22 digits. */
    char tmp[24];
    size_t n = 0;

    do {

        unsigned d = (unsigned)(v % base);

        tmp[n++] = d < 10 ? (char)('0' + d) : (char)((upper ? 'A' : 'a') + (d - 10));

        v /= base;

    } while (v);


    if (negative) {
        __emit(s, '-');
    }

    for (ssize_t i = (ssize_t)n; i < padding; i++) {
        __emit(s, '0');
    }

    while (n) {
        __emit(s, tmp[--n]);
    }
}


__nosanitize("undefined") static void dec(intmax_t __v, ssize_t padding, struct __sbuf* s, bool negative) {

    bool neg = negative && __v < 0;

    //? Negating INTMAX_MIN overflows, so take the magnitude in unsigned arithmetic.
    uintmax_t v = neg ? ((uintmax_t)0 - (uintmax_t)__v) : (uintmax_t)__v;

    __emit_num(s, v, 10, false, padding, neg);
}


__nosanitize("undefined") static void hex(uintmax_t v, ssize_t padding, struct __sbuf* s, bool upper) {

    __emit_num(s, v, 16, upper, padding, false);
}


__nosanitize("undefined") static void oct(uintmax_t v, ssize_t padding, struct __sbuf* s) {

    __emit_num(s, v, 8, false, padding, false);
}

__nosanitize("undefined") int vsnprintf(char* buf, size_t size, const char* fmt, va_list v) {

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);
    DEBUG_ASSERT(fmt);

    struct __sbuf __out = {.buf = buf, .size = size, .len = 0};

    for (; *fmt; fmt++) {

        if (*fmt != '%') {
            __emit(&__out, *fmt);
            continue;
        }

        fmt++;

        if (*fmt == '\0') {
            kpanicf("vsnprintf: invalid format string '%s'\n", fmt);
        }


        long w = 0;
        long m = LONG_MAX;
        long l = sizeof(int);

        if (*fmt == '*') {

            w = va_arg(v, int);

            fmt++;

        } else if (*fmt >= '0' && *fmt <= '9') {

            do {

                w *= 10;
                w += (*fmt - '0');

                fmt++;

            } while (*fmt >= '0' && *fmt <= '9');
        }


        if (*fmt == '.') {

            fmt++;

            if (*fmt == '*') {

                m = va_arg(v, int);

                fmt++;

            } else if (*fmt >= '0' && *fmt <= '9') {

                m = 0;

                do {

                    m *= 10;
                    m += (*fmt - '0');

                    fmt++;

                } while (*fmt >= '0' && *fmt <= '9');
            }

        } else {

            if (*fmt == 'z') { // size_t

                l = sizeof(size_t);

                fmt++;

            } else if (*fmt == 'j') { // intmax_t

                l = sizeof(intmax_t);

                fmt++;

            } else if (*fmt == 't') { // ptrdiff_t

                l = sizeof(ptrdiff_t);

                fmt++;

            } else if (*fmt == 'h') { // short

                l = sizeof(short);

                fmt++;

                if (*fmt == 'h') { // char

                    l = sizeof(char);
                    fmt++;
                }

            } else if (*fmt == 'l') { // long

                l = sizeof(long);

                fmt++;

                if (*fmt == 'l') { // long long

                    l = sizeof(long long);
                    fmt++;
                }

            } else if (*fmt == 'L') { // long double

                kpanicf("vsnprintf: unsupported format specifier 'L'\n");
            }
        }


        switch (*fmt) {

            case 's':

                if (w > m) {
                    w = m;
                }

                if (w > 0) {

                    ssize_t i = 0;

                    for (char* s = va_arg(v, char*); s && *s && m--; s++, i++) {
                        __emit(&__out, *s);
                    }

                    for (; i < w; i++) {
                        __emit(&__out, ' ');
                    }

                } else {

                    for (char* s = va_arg(v, char*); s && *s && m--; s++) {
                        __emit(&__out, *s);
                    }
                }

                break;

            case 'c':

                __emit(&__out, ((int8_t)va_arg(v, int)));
                break;

            case 'x':
            case 'X':

                switch (l) {
                    case 1:
                        hex((uintmax_t)((uint8_t)va_arg(v, int)), w, &__out, (*fmt == 'X'));
                        break;
                    case 2:
                        hex((uintmax_t)((uint16_t)va_arg(v, int)), w, &__out, (*fmt == 'X'));
                        break;
                    case 4:
                        hex((uintmax_t)((uint32_t)va_arg(v, int)), w, &__out, (*fmt == 'X'));
                        break;
                    case 8:
                        hex((uintmax_t)((uint64_t)va_arg(v, int64_t)), w, &__out, (*fmt == 'X'));
                        break;
                    default:
                        DEBUG_ASSERT(0);
                        break;
                }

                break;

            case 'o':

                switch (l) {
                    case 1:
                        oct((uintmax_t)((uint8_t)va_arg(v, int)), w, &__out);
                        break;
                    case 2:
                        oct((uintmax_t)((uint16_t)va_arg(v, int)), w, &__out);
                        break;
                    case 4:
                        oct((uintmax_t)((uint32_t)va_arg(v, int)), w, &__out);
                        break;
                    case 8:
                        oct((uintmax_t)((uint64_t)va_arg(v, int64_t)), w, &__out);
                        break;
                    default:
                        DEBUG_ASSERT(0);
                        break;
                }

                break;

            case 'p':

                fmt++;

                if (*fmt == 'h') {

                    fmt++;

                    char space = ' ';

                    if (*fmt == 'C') {
                        space = ':';
                    } else if (*fmt == 'D') {
                        space = '-';
                    } else {
                        fmt--;
                    }

                    for (uint8_t* bytes = va_arg(v, void*); bytes && m--; bytes++) {
                        hex(*bytes, 2, &__out, 1);
                        __emit(&__out, space);
                    }

                } else {

                    fmt--;

                    __emit(&__out, '0');
                    __emit(&__out, 'x');
                    hex((intmax_t)va_arg(v, void*), w, &__out, 1);
                }

                break;

            case 'd':
            case 'i':
            case 'u':

                switch (l) {
                    case 1:
                        dec((intmax_t)((int8_t)va_arg(v, int)), w, &__out, *fmt != 'u');
                        break;
                    case 2:
                        dec((intmax_t)((int16_t)va_arg(v, int)), w, &__out, *fmt != 'u');
                        break;
                    case 4:
                        dec((intmax_t)((int32_t)va_arg(v, int)), w, &__out, *fmt != 'u');
                        break;
                    case 8:
                        dec((intmax_t)((int64_t)va_arg(v, int64_t)), w, &__out, *fmt != 'u');
                        break;
                    default:
                        DEBUG_ASSERT(0);
                        break;
                }

                break;


            case 'n':

                switch (l) {
                    case 1:
                        *((int8_t*)va_arg(v, int*)) = __out.len;
                        break;
                    case 2:
                        *((int16_t*)va_arg(v, int*)) = __out.len;
                        break;
                    case 4:
                        *((int32_t*)va_arg(v, int*)) = __out.len;
                        break;
                    case 8:
                        *((int64_t*)va_arg(v, int64_t*)) = __out.len;
                        break;
                    default:
                        DEBUG_ASSERT(0);
                        break;
                }

                break;

            case '%':

                __emit(&__out, '%');
                break;

            default:

                kpanicf("vsnprintf: unsupported format specifier '%c'\n", *fmt);

                __emit(&__out, *fmt);
                break;
        }
    }

    /* At most size-1 characters are kept, so the terminator always lands inside the
       buffer -- it used to be written at buf[p] with p already == size, one past the end. */
    if (likely(size > 0)) {
        buf[__out.len < size ? __out.len : size - 1] = '\0';
    }

    //? The length the output would have had, excluding the terminator, as C requires:
    //? a caller detects truncation with `ret >= size`. It used to include the terminator.
    return (int)__out.len;
}
