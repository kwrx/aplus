#ifndef _ZETA_H
#define _ZETA_H


#define ZETA_APPS_PATH          "/sys/apps"




#define __Z(x)                  \
    ZETA_LIBRARY_##x

#define __D(x, y, z, w, k)      \
    extern void __Z(k) ();      \
    java_native_add(x, y, w, z, (void*) __Z(k))
    
#define __DEX(x, y, z, w, k)    \
    java_native_add(x, y, w, z, (void*) __Z(k))


#ifdef _ZETA_MAIN_H

extern j_value __Z(Activity_Load) (char*);

static inline void ZETA_LIBRARY_LOAD() {
    #include "lib/native/Activity.h"
    #include "lib/native/ZDebug.h"
}
#endif

#endif