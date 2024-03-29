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
#include <arch/x86/asm.h>
#include <asm/linkage.h>


#
# sched_yield() for kernel-space usage
#   WARN! do not use in interrupt-context
#

    .code64
    .align 16, 0x90
    .section .text

    .global sched_yield
    .type sched_yield, @function
sched_yield:

  # Save User Stack
    movq    %rsp, %rax

  # Load Kernel Stack
    movq    %gs:24, %rsp

    pushq   $KERNEL_DS  
    pushq   %rax

    pushfq
    pushq   $KERNEL_CS
    pushq   $.L0

    pushq   $0
    pushq   $0xFE

    movq    $24,  %rax  # sched_yield()

    .extern interrupt_common
    jmp     interrupt_common


    .align 16, 0x90
.L0:
    
    xorq    %rax, %rax
    ret

END(sched_yield)
  