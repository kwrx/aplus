#include <xdev.h>
#include <libc.h>

extern int __divdi3;

EXPORT(__errno);
EXPORT(memset);
EXPORT(memcpy);
EXPORT(memcmp);
EXPORT(memmove);
EXPORT(strcpy);
EXPORT(strcat);
EXPORT(strlen);
EXPORT(strcmp);
EXPORT(strncmp);
EXPORT(__divdi3);
