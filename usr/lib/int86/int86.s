;;
;;  int86.asm
;;
;;  Author:
;;       WareX <inferdevil97@gmail.com>
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

global int86					; void int86(unsigned char int_num, regs16_t* regs);

struc regs16_t
	.di resw 1
	.si resw 1
	.bp resw 1
	.sp resw 1
	.bx resw 1
	.dx resw 1
	.cx resw 1
	.ax resw 1
	.gs resw 1
	.fs resw 1
	.es resw 1
	.ds resw 1
	.ef resw 1
endstruc


%define INT32_BASE 		0x7c00
%define REBASE(x) 		(((x) - reloc) + INT32_BASE)
%define GDTENTRY(x)		((x) << 3)
%define CODE32			0x08
%define DATA32			0x10
%define CODE16			0x18
%define DATA16			0x20
%define STACK16			(INT32_BASE - regs16_t_size)

section .text
int86: use32
	cli
	pusha
	mov esi, reloc
	mov edi, INT32_BASE
	mov ecx, (int32_end - reloc)
	cld
	rep movsb
	jmp INT32_BASE
reloc: use32
	mov [REBASE(stack32_ptr)], esp
	sidt [REBASE(idt32_ptr)]
	sgdt [REBASE(gdt32_ptr)]
	lgdt [REBASE(gdt16_ptr)]
	lea esi, [esp + 0x24]
	lodsd
	mov [REBASE(ib)], al
	mov esi, [esi]
	mov edi, STACK16
	mov ecx, regs16_t_size
	mov esp, edi
	rep movsb
	jmp word CODE16:REBASE(p_mode16)
p_mode16: use16
	mov ax, DATA16
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	mov eax, cr0
	and al, ~0x01
	mov cr0, eax
	jmp word 0x0:REBASE(r_mode16)
r_mode16: use16
	xor ax, ax
	mov ds, ax
	mov ss, ax
	lidt [REBASE(idt16_ptr)]
	mov bx, 0x0870
	call resetpic
	popa
	pop gs
	pop fs
	pop es
	pop ds
	sti
	db 0xCD
ib: db 0x00
	cli
	xor sp, sp
	mov ss, sp
	mov sp, INT32_BASE
	pushf
	push ds
	push es
	push fs
	push gs
	pusha
	mov bx, 0x2028
	call resetpic
	mov eax, cr0
	inc eax
	mov cr0, eax
	jmp dword CODE32:REBASE(p_mode32)
p_mode32: use32
	mov ax, DATA32
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	lgdt [REBASE(gdt32_ptr)]
	lidt [REBASE(idt32_ptr)]
	mov esp, [REBASE(stack32_ptr)]
	mov esi, STACK16
	lea edi, [esp + 0x28]
	mov edi, [edi]
	mov ecx, regs16_t_size
	cld
	rep movsb
	popa
	sti
ret
	
resetpic:
	push ax
	mov al, 0x11
	out 0x20, al
	out 0xA0, al
	mov al, bh
	out 0x21, al
	mov al, bl
	out 0xA1, al
	mov al, 0x04
	out 0x21, al
	shr al, 1
	out 0xA1, al
	shr al, 1
	out 0x21, al
	out 0xA1, al
	pop ax
ret
	
stack32_ptr: 
	dd 0x0
idt32_ptr:
	dw 0
	dd 0
gdt32_ptr:
	dw 0
	dd 0
idt16_ptr:
	dw 0x03FF
	dd 0
gdt16_base:
	.null:
		dd 0
		dd 0
	.code32:
		dw 0xFFFF
		dw 0
		db 0
		db 0x9A
		db 0xCF
		db 0
	.data32:
		dw 0xFFFF
		dw 0
		db 0
		db 0x92
		db 0xCF
		db 0
	.code16:
		dw 0xFFFF
		dw 0
		db 0
		db 0x9A
		db 0x0F
		db 0
	.data16:
		dw 0xFFFF
		dw 0
		db 0
		db 0x92
		db 0x0F
		db 0
gdt16_ptr:
	dw gdt16_ptr - gdt16_base - 1
	dd gdt16_base
	
int32_end:


