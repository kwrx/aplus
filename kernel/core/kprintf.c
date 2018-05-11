#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <libc.h>

#if DEBUG

int
kprintf(const char *fmt, ...) {


    char buf[BUFSIZ] = {0};
    char bfmt[BUFSIZ] = {0};

    memset(buf, 0, sizeof(buf));
    memset(bfmt, 0, sizeof(bfmt));

    sprintf(bfmt, "[%8f]%s", (double) timer_getus() / 1000000, fmt);

    va_list args;
    va_start(args, fmt);
    int out = vsprintf(buf, bfmt, args);
    
    
    int i;
    for(i = 0; i < out; i++)
        debug_send(buf[i]);


    va_end(args);
    return out;
}


EXPORT(kprintf);

#endif
