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


#ifndef _APLUS_BASE_H
#define _APLUS_BASE_H

#ifndef __APLUS__
#define __APLUS__
#endif

#ifndef __aplus__
#define __aplus__
#endif

#ifdef likely(x)
#undef likely(x)
#endif

#ifdef unlikely(x)
#undef unlikely(x)
#endif


#ifdef __GNUC__
#define likely(x)                       __builtin_expect(!!(x), 1)
#define unlikely(x)                     __builtin_expect(!!(x), 0)
#else
#define likely(x)                       (!!(x))
#define unlikely(x)                     (!!(x))
#endif


#ifndef E_OK
#define E_OK                            0
#endif

#ifndef E_ERR
#define E_ERR                           -1
#endif



#ifdef __GNUC__
#    ifdef __weak
#        undef __weak
#    endif

#    ifdef __packed
#        undef __packed
#    endif

#    ifdef __section
#        undef __section
#    endif

#    ifdef __align
#        undef __align
#    endif

#    ifdef __malloc
#        undef __malloc
#    endif

#    ifdef __optimize
#        undef __optimize
#    endif

#    ifdef __fastcall
#        undef __fastcall
#    endif

#    define __weak                          __attribute__((weak))
#    define __packed                        __attribute__((packed))
#    define __section(x)                    __attribute__((section(x)))
#    define __align(x)                      __attribute__((align(x)))
#    define __malloc                        __attribute__((malloc))
#    define __optimize(x)                   __attribute__((optimize("O" #x)))
#    define __fastcall                      __attribute__((fastcall))

#    define __PRAGMA(x)                     _Pragma(#x)
#    define WARNING(x)                      __PRAGMA(GCC diagnostic ignored x)
#else
#    define __weak
#    define __packed
#    define __section(x)
#    define __align(x)
#    define __malloc
#    define __optimize(x)
#    define WARNING(x)
#endif


#define PATH_FBDEV                      "/dev/fb0"
#define PATH_CONDEV                     "/dev/console"
#define PATH_KMEM                       "/dev/kmem"
#define PATH_MOUSEDEV                   "/dev/mouse"
#define PATH_KBDEV                      "/dev/kbd"
#define PATH_PWM                        "/dev/pwm"

#define PATH_SYSCONFIG                  "/etc/config"
#define PATH_FSTAB                      "/etc/fstab"

#define PATH_CURSORS                    "/usr/share/cursors"
#define PATH_FONTS                      "/usr/share/fonts"
#define PATH_ICONS                      "/usr/share/icons"
#define PATH_IMAGES                     "/usr/share/images"
#define PATH_KEYMAPS                    "/usr/share/kbd/keymaps"
#define PATH_TZDIR                      "/usr/share/zoneinfo"



#ifdef __cplusplus
extern "C" {
#endif

int libaplus_init(void* (*mallocfp) (size_t), void* (*callocfp) (size_t, size_t), void (*freefp) (void*));

__attribute__((malloc))
extern void* (*__libaplus_malloc) (size_t);

__attribute__((malloc))
extern void* (*__libaplus_calloc) (size_t, size_t);

extern void (*__libaplus_free) (void*); 
#ifdef __cplusplus
}
#endif

#endif