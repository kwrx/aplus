#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <aplus/core/base.h>
#include <aplus/core/debug.h>
#include <aplus/core/ipc.h>


/*!
 * @brief Print formatted output to the debugger and halt.
 */
void kpanicf(const char* fmt, ...) {

    static char buf[8192];
    static spinlock_t buflock = 0;

    __lock(&buflock, {

        va_list v;
        va_start(v, fmt);
        vsnprintf(buf, sizeof(buf), fmt, v);
        va_end(v);


        int i;
        for(i = 0; buf[i]; i++)
            arch_debug_putc(buf[i]);

    });


    for(;;);

}