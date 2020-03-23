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
#include <aplus/multiboot.h>
#include <arch/x86/asm.h>
#include <asm/linkage.h>




        .text
        .align 16, 0x90
        .code64

        .global arch_task_yield
        .type arch_task_yield, @function
arch_task_yield:

  # Save Caller Stack
    pushq   %r15
    movq    %rsp, %r15

  # Load Kernel Stack
    movq    $KERNEL_SYSCALL_STACK, %rsp
    addq    $KERNEL_SYSCALL_STACKSIZE, %rsp
  
  # Save Context
    pushq   $KERNEL_DS  
    pushq   %rsp

    pushfq
    pushq   $KERNEL_CS
    pushq   $.L0

    pushq   $0
    pushq   $0xFE

    movq    $24, %rax       # sched_yield

    .extern interrupt_common
    jmp     interrupt_common

.L0:

    addq    $8, %rsp

  # Restore Caller Stack
    movq    %r15, %rsp
    popq    %r15


END(arch_task_yield)