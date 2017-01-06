#ifndef _ZETA_H
#define _ZETA_H


#define ZETA_APPS_PATH          "/home/apps"




#define __Z(x)                  \
    ZETA_LIBRARY_##x

#define __D(x, y, z, w, k)      \
    extern void* __Z(k);        \
    java_native_add(x, y, w, z, __Z(k))


#ifdef _ZETA_MAIN_H
static inline void ZETA_LIBRARY_LOAD() {
    #include "lib/base/Activity.h"
}
#endif

#endif