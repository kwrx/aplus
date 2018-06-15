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
#include <aplus/debug.h>
#include <aplus/task.h>
#include <aplus/mm.h>
#include <libc.h>

#include <arch/i386/i386.h>


void pagefault_handler(i386_context_t* context) {
    uintptr_t p;
    __asm__ ("mov eax, cr2" : "=a"(p));

    /*if(vmm_swap(p) == 0) {
        __asm__ ("sti");
        return;
    }*/


    debug_dump(context, "Exception! Page Fault occured!", p, context->err_code);
    
    if(unlikely(current_task == kernel_task)) {
        __asm__ ("cli");
        for(;;) __asm__("hlt");
    }

    
        
    

    __asm__("sti");
    sys_kill(current_task->pid, SIGSEGV);
    sys_yield();
    sys_exit(SIGSEGV);
}
