#include <aplus.h>
#include <libc.h>


extern int __divdi3;
extern int __udivdi3;
extern int __umoddi3;
extern int __ctype_ptr__;

EXPORT(mbd);
EXPORT(__errno);
EXPORT(memset);
EXPORT(memcpy);
EXPORT(memcmp);
EXPORT(memmove);
EXPORT(strtok);
EXPORT(strcpy);
EXPORT(strcat);
EXPORT(strncat);
EXPORT(strlen);
EXPORT(strcmp);
EXPORT(strncmp);
EXPORT(strchr);
EXPORT(strtoul);
EXPORT(strncpy);
EXPORT(sprintf);
EXPORT(rand);
EXPORT(srand);
EXPORT(strerror);

EXPORT(__divdi3);
EXPORT(__udivdi3);
EXPORT(__umoddi3);
EXPORT(__ctype_ptr__);
EXPORT(_impure_ptr);


#if defined(__i386__) || defined(__x86_64__)
#include <aplus/bios.h>

/* TODO */
#endif
