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
#include <aplus/base.h>
#include <aplus/debug.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/sysconfig.h>
#include <aplus/utils/list.h>
#include <aplus/utils/hashmap.h>
#include <aplus/utils/unicode.h>
#include <libc.h>



int core_init() {
#if DEBUG
    extern void __stack_chk_init();
    __stack_chk_init();
#endif

    libaplus_init(kmalloc, kcalloc, kfree);
    return 0;
}


#if DEBUG
void __attribute__((__noreturn__)) __chk_fail(void) {
    kprintf(ERROR "*** buffer overflow detected ***: kernel panic!\n");

    if(unlikely(current_task == kernel_task))
        __halt();
    

    sys_kill(current_task->pid, SIGSEGV);
    sys_yield();
    sys_exit(SIGSEGV);

    __builtin_unreachable();
}

void __attribute__((__noreturn__)) __stack_chk_fail (void) {
    kprintf(ERROR "*** stack smashing detected ***: kernel panic!\n");

    if(unlikely(current_task == kernel_task))
        __halt();
    
    
    sys_kill(current_task->pid, SIGSEGV);
    sys_yield();
    sys_exit(SIGSEGV);

    __builtin_unreachable();
}

#ifdef __ELF__
void __attribute__((visibility ("hidden"))) __stack_chk_fail_local (void) {
	__stack_chk_fail();
}
#endif

EXPORT(__chk_fail);
EXPORT(__stack_chk_fail);
EXPORT(__stack_chk_fail_local);
#endif




#if defined(__i386__)
extern long __divdi3;
extern long __udivdi3;
extern long __umoddi3;
extern long __moddi3;
extern long __divmoddi4;

EXPORT(__divdi3);
EXPORT(__udivdi3);
EXPORT(__umoddi3);
EXPORT(__moddi3);
EXPORT(__divmoddi4);
#endif


EXPORT(__locale_ctype_ptr);
EXPORT(mbd);
EXPORT(memset);
EXPORT(memcpy);
EXPORT(memcmp);
EXPORT(memmove);
EXPORT(strtok);
EXPORT(strcpy);
EXPORT(strcat);
EXPORT(strncat);
EXPORT(strlen);
EXPORT(strcmp);
EXPORT(strncmp);
EXPORT(strchr);
EXPORT(strtoul);
EXPORT(strncpy);
EXPORT(stpcpy);
EXPORT(strrchr);
EXPORT(sprintf);
EXPORT(fgets);
EXPORT(fseek);
EXPORT(fopen);
EXPORT(fclose);
EXPORT(rand);
EXPORT(srand);
EXPORT(strerror);
EXPORT(tmpnam);
EXPORT(atoi);
EXPORT(atof);
EXPORT(sscanf);
EXPORT(__errno);


/* libaplus */
EXPORT(_list_push);
EXPORT(_list_push_front);
EXPORT(_list_front);
EXPORT(_list_back);
EXPORT(_list_next);
EXPORT(_list_prev);
EXPORT(_list_length);
EXPORT(_list_remove);
EXPORT(_list_clear);
EXPORT(hashmap_new);
EXPORT(hashmap_get);
EXPORT(hashmap_put);
EXPORT(hashmap_remove);
EXPORT(hashmap_iterate);
EXPORT(hashmap_free);
EXPORT(__sysconfig);
EXPORT(utf8_to_ucs2);
EXPORT(ucs2_to_utf8);
EXPORT(utf8_bytes);


/* libvterm */
#if 0
EXPORT(vterm_screen_set_callbacks);
EXPORT(vterm_state_reset);
EXPORT(vterm_screen_reset);
EXPORT(vterm_obtain_screen);
EXPORT(vterm_input_write);
EXPORT(vterm_new_with_allocator);
EXPORT(vterm_obtain_state);
EXPORT(vterm_parser_set_callbacks);
EXPORT(vterm_set_utf8);
#endif




