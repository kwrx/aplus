[BITS 32]



%define CONFIG_KERNEL_BASE			(0xFFFFFFFFF8000000)
%define CONFIG_STACK_BASE			(0xFFFFFFFFFFC00000)
%define CONFIG_STACK_SIZE			(0x0000000000020000)
%define CONFIG_VIDEOMODE			(0)

%define V2P(x)						((x) - CONFIG_KERNEL_BASE)


extern main
extern x86_64_init
extern load_bootargs

extern text
extern bss
extern end


extern mbd_grub

global _start
global stack

global PML4T
global PDT
global PT
global PT_1MB

global GDT64
global IDT64
global IRQ64
global TSS64

global arch_loop_idle
global gdt_load


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
	mov esi, 3
	mov edi, V2P(PT)

.L0:
	mov [edi], esi
	add esi, 0x1000
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

PT:
	times 4096 dq 0
PT_1MB:
	times 512 dq 0
PT_STACK:
%assign i 0
%rep (CONFIG_STACK_SIZE / 4096)
	dq V2P(stack_bottom) + i + 0x03
%assign i i + 0x1000
%endrep
	times (512 - (CONFIG_STACK_SIZE / 4096)) dq 0


PML4T:
	dq V2P(PDPT) + 3
	times 510 dq 0
	dq V2P(PDPT) + 3
.VFRAMES:
	dq PDPT + 3
	times 510 dq 0
	dq PDPT + 3

PDPT:
	dq V2P(PDT) + 3
	times 510 dq 0
	dq V2P(PDT) + 3
.VFRAMES:
	dq PDT + 3
	times 510 dq 0
	dq PDT + 3

PDT:
	dq V2P(PT) + 0x3
	dq V2P(PT) + 0x1000 + 0x3
	dq V2P(PT) + 0x2000 + 0x3
	dq V2P(PT) + 0x3000 + 0x3
	dq V2P(PT) + 0x4000 + 0x3
	dq V2P(PT) + 0x5000 + 0x3
	dq V2P(PT) + 0x6000 + 0x3
	dq V2P(PT) + 0x7000 + 0x3
	times 496 dq 0

	dq V2P(PT) + 0x3
	dq V2P(PT) + 0x1000 + 0x3
	dq V2P(PT) + 0x2000 + 0x3
	dq V2P(PT) + 0x3000 + 0x3
	dq V2P(PT) + 0x4000 + 0x3
	dq V2P(PT) + 0x5000 + 0x3
	dq V2P(PT) + 0x6000 + 0x3
	dq V2P(PT) + 0x7000 + 0x3
.VPAGES:
	dq PT
	dq PT + 0x1000
	dq PT + 0x2000
	dq PT + 0x3000
	dq PT + 0x4000
	dq PT + 0x5000
	dq PT + 0x6000
	dq PT + 0x7000
	times 496 dq 0

	dq PT
	dq PT + 0x1000
	dq PT + 0x2000
	dq PT + 0x3000
	dq PT + 0x4000
	dq PT + 0x5000
	dq PT + 0x6000
	dq PT + 0x7000

.PHYSADDR:
	dq V2P(PDT)
.REFCOUNT:
	dd 1
.NEXT:
	dq 0



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

	mov rsp, CONFIG_STACK_BASE + CONFIG_STACK_SIZE


	add rbx, CONFIG_KERNEL_BASE
	mov [mbd_grub], rbx

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
stack_bottom:
times CONFIG_STACK_SIZE db 0
stack:

