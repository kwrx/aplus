#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>

#include <aplus/base.h>
#include <aplus/sysconfig.h>

#include "lib/duktape.h"

#define _(x, y, z)                              \
    duk_ret_t l_##x(duk_context* ctx) {         \
        void f(duk_context* ctx)                \
            z                                   \
                                                \
        f(ctx);                                 \
        return y;                               \
    }



_(print, 0, {
    fprintf(stdout, "%s\n", duk_to_string(ctx, 0));
})

_(system, 1, {
    int e = system(duk_to_string(ctx, 0));
    if(e != 0)
        perror("system");
    

    duk_push_number(ctx, (double) e);
})

_(exec, 0, {
    char* s = (char*) duk_to_string(ctx, 0);
    char* argv[64];

    int i = 0;
    for(char* p = strtok(s, " "); p; p = strtok(NULL, " "))
        argv[i++] = p;

    argv[i] = NULL;
    execvp(argv[0], argv);
})


_(sleep, 0, {
    sleep(duk_to_int(ctx, 0));
})

_(sysconfig, 1, {
    char* s = (char*) sysconfig(duk_to_string(ctx, 0), NULL);
    if(!s)
        duk_push_undefined(ctx);
    else
        duk_push_string(ctx, s);
})