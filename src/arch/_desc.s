;;
;;  desc.asm
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

extern isr_stub
extern irq_stub

%macro ISRNC 1
	global isr%1
	isr%1:
		push byte 0
		push byte %1
		jmp isr_stub
%endmacro

%macro ISREC 1
	global isr%1
	isr%1:
		push byte %1
		jmp isr_stub
%endmacro

%macro IRQ 1
	global irq%1
	irq%1:
		push byte 0
		push byte (%1 + 32)
		jmp irq_stub
%endmacro

ISRNC 0
ISRNC 1
ISRNC 2
ISRNC 3
ISRNC 4
ISRNC 5
ISRNC 6
ISRNC 7
ISREC 8
ISRNC 9
ISREC 10
ISREC 11
ISREC 12
ISREC 13
ISRNC 15
ISRNC 16
ISRNC 17
ISRNC 18
ISRNC 19
ISRNC 20
ISRNC 21
ISRNC 22
ISRNC 23
ISRNC 24
ISRNC 25
ISRNC 26
ISRNC 27
ISRNC 28
ISRNC 29
ISRNC 30
ISRNC 31


IRQ 1
IRQ 2
IRQ 3
IRQ 4
IRQ 5
IRQ 6
IRQ 7
IRQ 8
IRQ 9
IRQ 10
IRQ 11
IRQ 12
IRQ 13
IRQ 14
IRQ 15



global irq0
extern pit_handler

irq0:
	sti
	pusha
	push ds
	push es
	push fs
	push gs
	
	mov eax, 0x10
	mov ds, eax
	mov fs, eax
	mov es, eax
	mov gs, eax
	

	push esp
	call pit_handler
	mov esp, eax

	mov al, 0x20
	mov dx, 0x20
	out dx, al
	
	pop gs
	pop fs
	pop es
	pop ds
	popa
	sti
iret




extern syscall_handler
extern errno

global isr0x80

syscall_retval dd 0

isr0x80:
	sti
	pusha
	push ds
	push es
	push fs
	push gs
	
	mov eax, 0x10
	mov ds, eax
	mov fs, eax
	mov es, eax
	mov gs, eax
	

	mov eax, esp
	push eax
	call syscall_handler
	mov [syscall_retval], eax
	pop eax
	
	pop gs
	pop fs
	pop es
	pop ds
	popa
	sti
	
	mov eax, [syscall_retval]
	mov ebx, [errno]
iret
	
	
extern pagefault_handler
global isr14	
	
isr14:
	push byte 14
	pusha
	push ds
	push es
	push fs
	push gs
	
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	
	mov eax, esp
	push eax
	call pagefault_handler
	pop eax

	pop gs
	pop fs
	pop es
	pop ds
	popa
	add esp, 8
	sti
iret
	

	

