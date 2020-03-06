/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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
#include <arch/x86/asm.h>
#include <asm/linkage.h>


#
# Syscall Handler for AMD syscall/sysret instructions
#

    .code64
    .section .text

    .global x86_syscall_handler
    .type x86_syscall_handler, @function
x86_syscall_handler:

  # Save User Stack
    movq    %rsp, %r15

  # Load Kernel Stack
    movq    $KERNEL_SYSCALL_STACK, %rsp
    addq    $0x1000, %rsp

  # Save User Registers
    pushq   %rsi
    pushq   %r11
    pushq   %rcx
    
  # Load Parameters
    pushq   %r9             # p5
    movq    %r8,  %r9       # p4
    movq    %r10, %r8       # p3
    movq    %rdx, %rcx      # p2
    movq    %rsi, %rdx      # p1
    movq    %rdi, %rsi      # p0
    movq    %rax, %rdi      # idx


  # Invoke Syscall
    callq   syscall_invoke
    addq    $8, %rsp


  # Restore User Registers
    popq    %rcx
    popq    %r11
    popq    %r15

  # Restore User Stack
    movq    %r15, %rsp

    sysretq
    