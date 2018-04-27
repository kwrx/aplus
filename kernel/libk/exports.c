#include <aplus.h>
#include <aplus/base.h>
#include <aplus/mm.h>
#include <aplus/sysconfig.h>
#include <aplus/utils/list.h>
#include <aplus/utils/hashmap.h>
#include <aplus/utils/unicode.h>
#include <libc.h>

#if defined(__i386__)
extern int __divdi3;
extern int __udivdi3;
extern int __umoddi3;
extern int __moddi3;
#endif



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
EXPORT(fgets);
EXPORT(fseek);
EXPORT(fopen);
EXPORT(fclose);
EXPORT(rand);
EXPORT(srand);
EXPORT(strerror);
EXPORT(tmpnam);
EXPORT(atoi);
EXPORT(atof);
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
EXPORT(hashmap_new);
EXPORT(hashmap_get);
EXPORT(hashmap_put);
EXPORT(hashmap_remove);
EXPORT(hashmap_iterate);
EXPORT(hashmap_free);
EXPORT(__sysconfig);
EXPORT(utf8_to_ucs2);
EXPORT(utf8_bytes);

#if defined(__i386__)
EXPORT(__divdi3);
EXPORT(__udivdi3);
EXPORT(__umoddi3);
EXPORT(__moddi3);
#endif

EXPORT(__locale_ctype_ptr);
EXPORT(_impure_ptr);


