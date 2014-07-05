;;
;;  loader.asm
;;
;;  Author:
;;       Antonio Natale <inferdevil97@gmail.com>
;;
;;  Copyright (c) 2014 WareX
;;
;;  This program is free software: you can redistribute it and/or modify
;;  it under the terms of the GNU General Public License as published by
;;  the Free Software Foundation, either version 3 of the License, or
;;  (at your option) any later version.
;;
;;  This program is distributed in the hope that it will be useful,
;;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;  GNU General Public License for more details.
;;
;;  You should have received a copy of the GNU General Public License
;;  along with this program.  If not, see <http://www.gnu.org/licenses/>.


[BITS 32]

MALIGN			equ		1 << 0
MEMINFO			equ		1 << 1
FLAGS			equ		MALIGN | MEMINFO
MAGIC			equ		0x1BADB002
CHECKSUM		equ		-(MAGIC + FLAGS)
STACKSIZE		equ		0x1000

global _start
global magic
global mbd
global kernel_initial_stack

extern main
extern text
extern bss
extern end
extern multiboot


section .multiboot
align 4
	dd MAGIC
	dd FLAGS
	dd CHECKSUM
	dd multiboot
	dd text
	dd bss
	dd end
	dd _start
	
section .text
_start:
	mov [mbd], ebx
	mov [magic], eax

	mov esp, kernel_initial_stack;
	
	call enable_fpu
	call enable_sse
	call init_kb
	call main
	
hang:
	nop
	pause
	jmp hang

enable_fpu:
	finit
	xor eax, eax
ret

enable_sse:	
	mov eax, cr0
	and ax, 0xFFFB
	or ax, 2
	mov cr0, eax
	mov eax, cr4
	or ax, 3 << 9
	mov cr4, eax
	xor eax, eax
ret

init_kb:
	in al, 0x60
	xor eax, eax
ret


align 32

mbd dd 0
magic dd 0	
	
section .bss
align 0x1000
stack_ptr_s resb STACKSIZE
kernel_initial_stack:
