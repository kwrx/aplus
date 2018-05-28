/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <aplus.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <aplus/task.h>
#include <arch/x86_64/x86_64.h>
#include <libc.h>

#if DEBUG


void debug_send(char value) {
    static int initialized = 0;

    if(!initialized) {
        initialized = 1;

#if CONFIG_SERIAL_DEBUG
        outb(0x3F8 + 1, 0x00);
        outb(0x3F8 + 3, 0x80);
        outb(0x3F8 + 0, 0x03);
        outb(0x3F8 + 1, 0x00);
        outb(0x3F8 + 3, 0x03);
        outb(0x3F8 + 2, 0xC7);
        outb(0x3F8 + 4, 0x0B);
#endif    
    }


#if CONFIG_BOCHS
    outb(0xE9, value);
#endif


#if CONFIG_SERIAL_DEBUG
    int i;
    for(i = 0; i < 100000 && ((inb(0x3F8 + 5) & 0x20) == 0); i++)
        ;
    outb(0x3F8, value);
#endif
}


char* debug_lookup_symbol(symbol_t* symtab, uintptr_t address) {
#if 0
    symbol_t* tmp, *found = NULL;
    for(tmp = symtab; tmp; tmp = tmp->next) {
        if((uintptr_t) tmp->addr == address) {
            found = tmp;
            break;
        }

        if((uintptr_t) tmp->addr < address)
            if(!found || found->addr < tmp->addr)
                found = tmp;
    }
    

    if(!found)
        return NULL;
        
    static char buf[BUFSIZ];
    memset(buf, 0, BUFSIZ);

    sprintf(buf, "%s+%p", found->name, (void*) (address - (uintptr_t) found->addr));
    return strdup(buf);
#endif
    return NULL;
}


void debug_dump(void* _context, char* errmsg, uintptr_t dump, uintptr_t errcode) {
    kprintf (ERROR
        "%s\n"
        "\t Task: %d (%s)\n"
        "\t Address: %p\n"
        "\t Error: %p\n",
        errmsg, 
        current_task->pid, current_task->name,
        dump, errcode
    );
}

EXPORT(debug_send);
EXPORT(debug_dump);
#endif
