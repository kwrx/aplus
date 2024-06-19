/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aplus.
 * 
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <string.h>
#include <syscall.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/syscall.h>
#include <aplus/errno.h>
#include <aplus/hal.h>

#include <aplus/utils/list.h>



struct test_hook {
    
    const void* ptr;
    const char* name;

} __packed;


#if defined(DEBUG) && defined(CONFIG_HAVE_TEST)
static void test_run(const char* context, uintptr_t hook_start, uintptr_t hook_end) {

    DEBUG_ASSERT(context);
    DEBUG_ASSERT(hook_start);
    DEBUG_ASSERT(hook_end);
    DEBUG_ASSERT(hook_start < hook_end);

#if DEBUG_LEVEL_INFO
    size_t max = (hook_end - hook_start) / sizeof(struct test_hook);
    size_t cnt = 0;
#endif

    for(; hook_start < hook_end; hook_start += sizeof(struct test_hook)) {

        struct test_hook* e = (struct test_hook*) (hook_start);

        DEBUG_ASSERT(e->ptr);
        DEBUG_ASSERT(e->name);


#if DEBUG_LEVEL_INFO
        kprintf(" - <%s> (%2zd/%2zd) running \e[37m%32s\e[0m    ", context, ++cnt, max, e->name);
#endif

        ((void (*)(void)) e->ptr) ();

#if DEBUG_LEVEL_INFO
        kprintf("[\e[32mOK\e[0m]\n");
#endif

    }

#if DEBUG_LEVEL_INFO
    kprintf("\n");
#endif

}
#endif


void test_init(void) {

#if defined(DEBUG) && defined(CONFIG_HAVE_TEST)

#if DEBUG_LEVEL_INFO
    kprintf("\n--- \e[1mTESTS\e[0m --------------------------------------------\e[0m\n\n");
#endif


    { /* Kernel */

        extern uint8_t tests_start;
        extern uint8_t tests_end;

        uintptr_t hook_start = (uintptr_t) &tests_start;
        uintptr_t hook_end   = (uintptr_t) &tests_end;

        test_run("kernel", hook_start, hook_end);

    }


    { /* Modules */

        extern list(module_t*, m_queue);

        list_each(m_queue, ko) {

            if(ko->status != MODULE_STATUS_LOADED)
                continue;

            
            for(size_t i = 1; i < ko->exe.header->e_shnum; i++) {

                #define syname(p) \
                    ((const char*) ((uintptr_t) ko->exe.header + ko->exe.shstrtab->sh_offset + p))


                if(strcmp(syname(ko->exe.section[i].sh_name), ".tests") == 0) {

                    uintptr_t hook_start = (uintptr_t) ko->core.ptr + ko->exe.section[i].sh_addr;
                    uintptr_t hook_end   = (uintptr_t) ko->core.ptr + ko->exe.section[i].sh_addr + ko->exe.section[i].sh_size;

                    test_run(ko->name, hook_start, hook_end);

                }

                #undef syname

            }


        }

    }


#if DEBUG_LEVEL_INFO
    kprintf("--- \e[1mEND\e[0m ----------------------------------------------\e[0m\n\n");
#endif

#endif

}

