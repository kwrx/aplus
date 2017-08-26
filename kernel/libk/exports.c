#include <aplus.h>
#include <aplus/base.h>
#include <aplus/mm.h>
#include <aplus/utils/list.h>
#include <libc.h>


extern int __divdi3;
extern int __udivdi3;
extern int __umoddi3;

int libk_init() {
    libaplus_init(kmalloc, kcalloc, kfree);
    return E_OK;
}

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
EXPORT(tmpnam);
EXPORT(atoi);
EXPORT(sscanf);
EXPORT(_list_push);
EXPORT(_list_push_front);
EXPORT(_list_front);
EXPORT(_list_back);
EXPORT(_list_next);
EXPORT(_list_prev);
EXPORT(_list_length);
EXPORT(_list_remove);
EXPORT(_list_clear);

EXPORT(__divdi3);
EXPORT(__udivdi3);
EXPORT(__umoddi3);
EXPORT(__ctype_ptr__);
EXPORT(_impure_ptr);


