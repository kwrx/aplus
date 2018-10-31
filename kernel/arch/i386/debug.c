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
#include <aplus/module.h>
#include <arch/i386/i386.h>
#include <libc.h>

#include <aplus/base.h>
#include <aplus/utils/list.h>

#if DEBUG
#include <libdis.h>


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


char* debug_lookup_symbol(uintptr_t address) {
   
    symbol_t* find(list(symbol_t*, symtab), uintptr_t address) {
        symbol_t* found = NULL;
        list_each(symtab, tmp) {
            if((uintptr_t) tmp->address == address) {
                found = tmp;
                break;
            }

            if((uintptr_t) tmp->address < address)
                if(!found || found->address < tmp->address)
                    found = tmp;
        }

        return found;
    }

    symbol_t* found;
    if(!(found = find(k_symtab, address)))
        if(!(found = find(m_symtab, address)))
            return NULL;

        
    static char buf[BUFSIZ];
    memset(buf, 0, BUFSIZ);

    sprintf(buf, "%s+%p", found->name, (void*) (address - (uintptr_t) found->address));
    return strdup(buf);
}


void debug_dump(void* _context, char* errmsg, uintptr_t dump, uintptr_t errcode) {
    i386_context_t* context = (i386_context_t*) _context;

    char* sym = debug_lookup_symbol(context->eip);
    if(unlikely(!sym))
        sym = "unknown";
    
    kprintf(ERROR "%s\n"
                  "\t Task: %d (%s)\n"
                  "\t Address: %p\n"
                  "\t Error: %p\n"
                  "\t PC: %p (%s)\n"
                  "\t SP: %p\n"
                  "\t GS: %p\n",
                  errmsg, 
                  current_task->pid, current_task->name,
                  dump, errcode,
                  context->eip, sym, context->esp, context->gs
    );

    //debug_stacktrace(5);
    kprintf("<%s>:\n", sym);



    static char line[BUFSIZ];
    memset(line, 0, sizeof(line));


    x86_insn_t i;
    x86_init(opt_none, NULL, NULL);


    dump = context->eip - 32;

    size_t s, p = 0;
    do {
        while((s = x86_disasm((void*) dump, 64, current_task->image->start, p, &i)) > 0) {
            x86_format_insn(&i, line, BUFSIZ, intel_syntax);
            if(p != 32)
                kprintf("     %08x:           %s\n", dump + p, line);
            else
                kprintf("  >> %08x:           %s\n", dump + p, line);

            p += s;
        }

        p++;
    } while(p < BUFSIZ);

    x86_cleanup();
}


void debug_stacktrace(int frames) {
    uintptr_t* ebp = (uintptr_t*) ((uintptr_t) &frames - 8);
    uintptr_t frame;
    for(frame = 0; frame < frames; frame++) {
        
        uintptr_t eip = ebp[1];
        if(!eip)
            break;

        if(frame == 0)
            kprintf(LOG "\tStack Trace:\n");

        ebp = (uintptr_t*) ebp[0];
        kprintf(LOG "\t\t[%d] %p\n", -frame, eip);
    }
}

EXPORT(debug_send);
EXPORT(debug_dump);
EXPORT(debug_stacktrace);
#endif
