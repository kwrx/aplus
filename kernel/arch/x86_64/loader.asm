[BITS 32]



%define CONFIG_KERNEL_BASE			(0xFFFFFFFFC0000000)
%define CONFIG_VIDEOMODE			(0)

%define V2P(x)						((x) - CONFIG_KERNEL_BASE)


extern main
extern x86_64_init
extern load_bootargs

extern text
extern bss
extern end


global mbd_grub
global _start

global arch_loop_idle


MALIGN 		equ 	(1 << 0)
MEMINFO 	equ		(1 << 1)
VMODE 		equ		(1 << 2)
FLAGS 		equ		(MALIGN | MEMINFO | VMODE)

MAGIC 		equ		0x1BADB002
CHECKSUM 	equ		-(MAGIC + FLAGS)


section .multiboot
multiboot:
align 4
	dd MAGIC
	dd FLAGS
	dd CHECKSUM
	dd 0
	dd 0
	dd 0
	dd 0
	dd 0
%if CONFIG_VIDEOMODE
	dd 0
	dd 1024
	dd 768
	dd 32
%else
	dd 1
	dd 0
	dd 0
	dd 0
%endif


section .text
_start:
	mov esp, V2P(stack)
	pushad

	cli
	finit


enable_sse:
	mov eax, cr0
	and ax, 0xFFFB
	or ax, 2
	mov cr0, eax
	mov eax, cr4
	or ax, 3 << 9
	mov cr4, eax
	xor eax, eax


switch64:
	mov ecx, 4096
	mov ebx, 3
	mov edi, V2P(PT)

.L0:
	mov [edi], ebx
	add ebx, 0x1000
	add edi, 8
	loop .L0


	mov eax, V2P(PML4T)
	mov cr3, eax

	mov eax, cr4
	or eax, 1 << 5
	mov cr4, eax

	mov ecx, 0xC0000080
	rdmsr
	or eax, 1 << 8
	wrmsr

	mov eax, cr0
	or eax, (1 << 31)
	mov cr0, eax

	popad

	lgdt [V2P(GDT64.Pointer)]
	jmp GDT64.Code:V2P(long_main)



section .data
align 0x1000
PML4T:
	dq V2P(PDPT) + 3
	times 510 dq 0
	dq V2P(PDPT) + 3
PDPT:
	dq V2P(PDT) + 3
	times 510 dq 0
	dq V2P(PDT) + 3
PDT:
	dq V2P(PT) + 3
	dq V2P(PT) + 0x1000 + 3
	dq V2P(PT) + 0x2000 + 3
	dq V2P(PT) + 0x3000 + 3
	dq V2P(PT) + 0x4000 + 3
	dq V2P(PT) + 0x5000 + 3
	dq V2P(PT) + 0x6000 + 3
	dq V2P(PT) + 0x7000 + 3
	times 504 dq 0
PT:
	times 4096 dq 0


GDT64:
    .Null: equ $ - GDT64
    dw 0
    dw 0
    db 0
    db 0
    db 0
    db 0
    .Code: equ $ - GDT64
    dw 0
    dw 0
    db 0
    db 10011000b
    db 00100000b
    db 0
    .Data: equ $ - GDT64
    dw 0
    dw 0
    db 0
    db 10010000b
    db 00000000b
    db 0
    .Pointer:
    dw $ - GDT64 - 1
    dq GDT64


[BITS 64]
section .text

long_main:
mov dword [0xb8000], 0x40404040
	jmp $	
	;lea rcx, [.L1]
	;jmp rcx
.L1:
	cli
	mov cx, GDT64.Data
	mov ds, cx
	mov es, cx
	mov gs, cx
	mov fs, cx

	mov rsp, stack

	mov rax, mbd_grub
	mov [rax], rbx

	mov rcx, end
	sub rcx, bss
	xor rax, rax
	mov rdi, bss
	rep stosb

	call load_bootargs
	call x86_64_init

	push qword 0
	push qword 0
	call main
	add esp, 32
	
	cli

arch_loop_idle:
	hlt
	jmp arch_loop_idle


section .stack
align 0x1000
times 0x4000 db 0
stack:

section .bss
align 0x1000
mbd_grub:
	resq 0

