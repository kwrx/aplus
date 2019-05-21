[BITS 64]
section .text

global x86_lgdt
global x86_ltr
global x86_lidt
global x86_cli
global x86_sti
global gdtp
global idtp
global isrs

extern x86_isr_handler

%macro ISRNC 1
	global isr%1
	isr%1:
		push qword 0
		push qword %1
		jmp isr_stub
%endmacro

%macro ISREC 1
	global isr%1
	isr%1:
		push qword %1
		jmp isr_stub
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
ISREC 14
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
ISRNC 0xFD
ISRNC 0xFE



%assign i 0
%rep 224
	global irq%[i]
	irq%[i]:
		push qword 0
		push qword (i + 32)
		jmp isr_stub
%assign i i + 1
%endrep


isrs:
%assign i 0
%rep 32
	dq isr%[i]
%assign i i + 1
%endrep

%assign i 0
%rep 221
	dq irq%[i]
%assign i i + 1
%endrep

	dq isr0xFD
	dq isr0xFE
	dq irq223
	dq 0
	

isr_stub:
	push r15
	push r14
	push r13
	push r12
	push r11
	push r10
	push r9
	push r8
	push rbp
	push rdi
	push rsi
	push rdx
	push rcx
	push rbx
	push rax

	sub rsp, 8
	stmxcsr [rsp]

	cld
	lea rdi, [rsp]
	call x86_isr_handler

	ldmxcsr [rsp]
	add rsp, 8

	pop rax
	pop rbx
	pop rcx
	pop rdx
	pop rsi
	pop rdi
	pop rbp
	pop r8
	pop r9
	pop r10
	pop r11
	pop r12
	pop r13
	pop r14
	pop r15

	add rsp, 16
iretq



x86_lgdt:
	lgdt [gdtp.ptr]
	jmp .L0
.L0:
	mov cx, 0x10
	mov ds, cx
	mov es, cx
	mov fs, cx
	mov gs, cx
	mov ss, cx
ret

x86_ltr:
	mov ax, 0x28
	ltr ax
ret

x86_lidt:
	lidt [idtp.ptr]
ret

x86_sti:
	sti
ret

x86_cli:
	cli
ret

section .data
align 16
gdtp:
	times 32 dq 0
.ptr:
	dw $ - gdtp - 1
	dq gdtp

align 16
idtp:
	times 256 dq 0
	times 256 dq 0
.ptr:
	dw $ - idtp - 1
	dq idtp