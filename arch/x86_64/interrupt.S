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



        .text
        .align 16, 0x90
        .code64

        .global interrupt_common
        .type interrupt_common, @function
interrupt_common:

        cli
        cld


        # Save CPU Registers
        pushq   %r15
        pushq   %r14
        pushq   %r13
        pushq   %r12
        pushq   %r11
        pushq   %r10
        pushq   %r9
        pushq   %r8
        pushq   %rax
        pushq   %rcx
        pushq   %rdx
        pushq   %rbx
        pushq   %rbp
        pushq   %rsi
        pushq   %rdi


        movq    %rsp, %rdi

        # Call Exception Handler
        .extern x86_exception_handler
        STDCALL(x86_exception_handler)

        movq    %rax, %rsp


        .global interrupt_return
        .type interrupt_return, @function
interrupt_return:

        # Restore CPU Registers
        popq    %rdi
        popq    %rsi
        popq    %rbp
        popq    %rbx
        popq    %rdx
        popq    %rcx
        popq    %rax
        popq    %r8
        popq    %r9
        popq    %r10
        popq    %r11
        popq    %r12
        popq    %r13
        popq    %r14
        popq    %r15

        addq    $16, %rsp
        iretq

END(interrupt_return)
END(interrupt_common)