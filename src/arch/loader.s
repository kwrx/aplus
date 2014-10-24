[BITS 32]

extern main
extern text
extern bss
extern end


MALIGN equ 		(1 << 0)
MEMINFO equ		(1 << 1)
VMODE equ		(1 << 2)
FLAGS equ		(MALIGN | MEMINFO | VMODE)
MAGIC equ		(0x1BADB002)
CHECKSUM equ	-(MAGIC + FLAGS)
STACKSIZE equ	0x4000



global _start
global magic
global mbd
global kernel_stack
global multiboot



section .bss
align 0x1000
kernel_stack: resb STACKSIZE
magic: resd 1
mbd: resd 1
	
section .multiboot
multiboot:
align 4
	dd MAGIC
	dd FLAGS
	dd CHECKSUM
	dd multiboot
	dd text
	dd bss
	dd end
	dd _start
	dd 0
	dd 0
	dd 0
	dd 32
		
		
	
section .text
_start:
	mov [mbd], ebx
	mov [magic], eax
	
	mov esp, kernel_stack + STACKSIZE
	
	cli
	call enable_fpu
	call enable_sse
	call enable_kb
	call main
	call reboot
	jmp $
	
	
reboot:
	cli
	mov cx, 0x02
	mov dx, 0x64
.L1:
	and cx, 0x02
	jz .L2
	
	in al, dx
	mov cx, ax
	jmp .L1
.L2:	
	mov al, 0xFE
	out dx, al
	jmp $
ret


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

enable_kb:
	in al, 0x60
	xor eax, eax
ret
	
