#ifndef _APLUS_SYSCONFIG_H
#define _APLUS_SYSCONFIG_H


#include <aplus/base.h>
#include <stdint.h>

typedef void* sysvalue_t;




#define sysconfig(x, y) \
    __sysconfig(x, (sysvalue_t) ((long) y))



#ifdef __cplusplus
extern "C" {
#endif

sysvalue_t __sysconfig(const char* option, sysvalue_t defvalue);

#ifdef __cplusplus
}
#endif



#endif