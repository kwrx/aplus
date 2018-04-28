#ifndef _MODULE_H
#define _MODULE_H

#include <aplus/base.h>
#include <aplus/elf.h>
#include <aplus/utils/list.h>

#define MODULE_NAME(x)                                  \
    __attribute__((section(".module_name")))            \
    const char __module_name__[] = x "\0"

#define MODULE_DEPS(x)                                  \
    __attribute__((section(".module_deps")))            \
    const char __module_deps__[] = x "\0"


#define MODULE_AUTHOR(x);
#define MODULE_LICENSE(x);



typedef struct module {
    char* name;
    list(char*, deps);

    int (*init) (void);
    int (*dnit) (void);
    
    uintptr_t loaded_address;
    uintptr_t image_address;

    size_t size;
    int loaded;
} module_t;


int module_init(void);
int module_dnit(void);

extern list(module_t*, m_queue);
extern list(symbol_t*, m_symtab);


#define __APLUS_MODULE__        1
#endif
