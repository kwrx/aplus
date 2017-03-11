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


#define PATH_FBDEV                      "/dev/fb0"
#define PATH_GHSM                       "/dev/gshm"
#define PATH_MOUSEDEV                   "/dev/mouse"
#define PATH_KBDEV                      "/dev/kbd"
#define PATH_PWM                        "/dev/pwm"

#define PATH_SYSCONFIG                  "/etc/config"
#define PATH_FSTAB                      "/etc/fstab"

#define PATH_CURSORS                    "/usr/share/cursor"
#define PATH_FONTS                      "/usr/share/fonts"
#define PATH_ICONS                      "/usr/share/icons"
#define PATH_IMAGES                     "/usr/share/images"

#endif