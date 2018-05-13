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