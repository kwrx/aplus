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


#include <aplus.h>
#include <aplus/multiboot.h>
#include <arch/x86/asm.h>
#include <asm/linkage.h>


/**
 * arch_userspace_enter - Switch to usermode
 * @entry:      Entry point
 * @stack_ptr:  Stack pointer
 * @stack_base: Stack base
 *
 * This function switches to usermode and calls the specified entry point.
 * The entry point is called with the specified stack pointer.
 *
 * This function does not return.
 */

        .text
        .align 16, 0x90
        .code64

        .global arch_userspace_enter
        .type arch_userspace_enter, @function
arch_userspace_enter:

        cli
        cld

        movq    %cr3, %rax
        movq    %rax, %cr3

        movq    $USER_DS | 3, %rax
        movq    %rax, %ds
        movq    %rax, %es

        pushq   $USER_DS | 3
        pushq   %rsi

        pushq   $0x202
        pushq   $USER_CS | 3
        pushq   %rdi

        pushq   $0
        pushq   $0

        # Set CPU Registers
        pushq   $0
        pushq   $0
        pushq   $0
        pushq   $0
        pushq   $0
        pushq   $0
        pushq   $0
        pushq   $0
        pushq   $0
        pushq   $0
        pushq   $0
        pushq   $0
        pushq   $0
        pushq   $0
        pushq   %rdx

        .extern interrupt_return
        jmp     interrupt_return

END(arch_userspace_enter)
