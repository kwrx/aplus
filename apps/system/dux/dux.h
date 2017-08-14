#ifndef _DUX_H
#define _DUX_H

#define D(x)    \
    extern duk_ret_t l_##x (duk_context*)




D(exec);
D(sysconfig);
D(system);
D(print);

static duk_function_list_entry c_funcs[] = {
    { "exec", l_exec, 1 },
    { "sysconfig", l_sysconfig, 1},
    { "system", l_system, 1 },
    { "print", l_print, 1 },
    { NULL, NULL, 0 }
};

#endif
