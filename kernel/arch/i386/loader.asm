[BITS 32]


%define CONFIG_KERNEL_BASE			0xC0000000
%define CONFIG_STACK_BASE			0xFFC00000
%define CONFIG_STACK_SIZE			0x00020000
%define CONFIG_VIDEOMODE			(0)

%define V2P(x)						((x) - CONFIG_KERNEL_BASE)


extern main
extern i386_init
extern load_bootargs

extern text
extern bss
extern end

extern mbd_grub


global _start
global stack

global PDT
global PT
global PT_1MB

global GDT32
global IDT32
global IRQ32
global TSS32


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
	dd _start
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
	mov ecx, cr0
	and cx, 0xFFFB
	or cx, 2
	mov cr0, ecx
	mov ecx, cr4
	or cx, 3 << 9
	mov cr4, ecx
	xor ecx, ecx


initpaging:
	mov ecx, 4096
	mov esi, 3
	mov edi, V2P(PT)

.L0:
	mov [edi], esi
	add esi, 0x1000
	add edi, 4
	loop .L0

	mov ecx, V2P(PDT)
	mov cr3, ecx
	

	mov eax, cr0
	or eax, (1 << 31)
	mov cr0, eax

	lea ecx, [high_start]
	jmp ecx




section .text
high_start:
	mov ecx, 256
	mov esi, 3
	mov edi, PT_1MB
.L0:
	mov [edi], esi
	add esi, 0x1000
	add edi, 4
	loop .L0


	
	mov dword [PDT + ((CONFIG_STACK_BASE >> 22) * 4)], V2P(PT_STACK) + 3	
	mov dword [PDT.VPAGES + ((CONFIG_STACK_BASE >> 22) * 4)], PT_STACK

	mov dword [PDT], V2P(PT_1MB) + 3
	mov dword [PDT.VPAGES], PT_1MB

	mov dword [PDT + 4], 0
	mov dword [PDT.VPAGES + 4], 0
	mov dword [PDT + 8], 0
	mov dword [PDT.VPAGES + 8], 0
	mov dword [PDT + 12], 0
	mov dword [PDT.VPAGES + 12], 0
	

	lgdt [GDT32.PTR]

.L1:
	jmp GDT32.KCODE:.L2

.L2:
	mov cx, GDT32.KDATA
	mov ds, cx
	mov ss, cx
	mov fs, cx
	mov gs, cx
	mov es, cx

	mov esp, CONFIG_STACK_BASE + CONFIG_STACK_SIZE


	add ebx, CONFIG_KERNEL_BASE
	mov [mbd_grub], ebx


	mov ecx, end
	sub ecx, bss
	xor eax, eax
	mov edi, bss
	rep stosb

	
	call load_bootargs
	call i386_init

	push dword 0
	push dword 0
	call main
	add esp, 8

	cli
	
arch_loop_idle:
	hlt
	jmp arch_loop_idle


gdt_load:
	lgdt [GDT32.PTR]
ret



	
section .data
align 0x1000

PT:
	times 4096 dd 0
PT_1MB:
	times 1024 dd 0
PT_STACK:
%assign i 0
%rep (CONFIG_STACK_SIZE / 4096)
	dd V2P(stack_bottom) + i + 0x03
%assign i i + 0x1000
%endrep
	times (1024 - (CONFIG_STACK_SIZE / 4096)) dd 0


PDT:
	dd V2P(PT) + 0x3
	dd V2P(PT) + 0x1000 + 0x3
	dd V2P(PT) + 0x2000 + 0x3
	dd V2P(PT) + 0x3000 + 0x3
	times ((CONFIG_KERNEL_BASE >> 22) - 4) dd 0

	dd V2P(PT) + 0x3
	dd V2P(PT) + 0x1000 + 0x3
	dd V2P(PT) + 0x2000 + 0x3
	dd V2P(PT) + 0x3000 + 0x3
	times (1024 - (CONFIG_KERNEL_BASE >> 22) - 4) dd 0
.VPAGES:
	dd PT
	dd PT + 0x1000
	dd PT + 0x2000
	dd PT + 0x3000
	times ((CONFIG_KERNEL_BASE >> 22) - 4) dd 0

	dd PT
	dd PT + 0x1000
	dd PT + 0x2000
	dd PT + 0x3000
	times (1024 - (CONFIG_KERNEL_BASE >> 22) - 4) dd 0

.PHYSADDR:
	dd V2P(PDT)
.REFCOUNT:
	dd 1
.NEXT:
	dd 0


GDT32:
	dd 0, 0
.KCODE: equ $ - GDT32
	dw 0xFFFF
	dw 0
	db 0
	db 0x9A
	db 0xCF
	db 0
.KDATA: equ $ - GDT32
	dw 0xFFFF
	dw 0
	db 0
	db 0x92
	db 0xCF
	db 0
.UCODE: equ $ - GDT32
	dw 0xFFFF
	dw 0
	db 0
	db 0xFA
	db 0xCF
.UDATA: equ $ - GDT32
	dw 0xFFFF
	dw 0
	db 0
	db 0xF2
	db 0xCF
	db 0
.TSS: equ $ - GDT32
	dd 0, 0
.PTR:
	dw $ - GDT32 - 1
	dd GDT32

IDT32:
	times 256 dq 0
.PTR:
	dw $ - IDT32 - 1
	dd IDT32

IRQ32:
	times 16 dq 0

TSS32:
	times 100 db 0



section .stack
align 0x1000
stack_bottom:
times CONFIG_STACK_SIZE db 0
stack:


