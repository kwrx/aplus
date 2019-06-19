[BITS 64]

%define CONFIG_KERNEL_BASE              0xFFFFFFFF80000000
%define CONFIG_STACK_SIZE			    0x0000000000100000
%define V2P(x)                          ((x) - CONFIG_KERNEL_BASE)


extern kmain
extern arch_early_init

extern mbd_raw
extern __bss_start
extern __bss_end

global _start
global early_stack




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
%ifdef X86_BUILTIN_VIDEOMODE
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


[BITS 32]
section .text
_start:
    mov esp, V2P(early_stack)
    mov [V2P(mbd_raw)], ebx

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


init_paging_1_1:
;	Low --> 0x0000000000000000-0x0000000040000000
; 	High -> 0xFFFFFFFF80000000-0xFFFFFFFFC0000000

; PML4
    mov edi, V2P(early_pml4)
    mov esi, V2P(early_pdp_low)
    or esi, 0x01                            ; Present
    mov [edi], esi

    mov esi, V2P(early_pdp_high)
    or esi, 0x01
    mov [edi + (511 * 8)], esi

; PDP (Low)
    mov edi, V2P(early_pdp_low)
    mov esi, 0x183                          ; Global, 1GiB, R/W, Present
    mov [edi], esi

; PDP (High)
    mov edi, V2P(early_pdp_high)
    mov esi, 0x183
    mov [edi + (510 * 8)], esi


.enable_paging:
    mov eax, V2P(early_pml4)
    mov cr3, eax

    mov eax, cr4
    or eax, (1 << 5)                        ; PAE
    and eax, ~(1 << 17)                     ; PCID
    mov cr4, eax

	
	mov ecx, 0xC0000080
	rdmsr
	or eax, 1 << 11				            ; NX
	wrmsr

    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)                        ; LME
    wrmsr

    mov eax, cr0
    or eax, (1 << 31)                       ; PG
    and eax, ~(1 << 29)                     ; NW
    and eax, ~(1 << 30)                     ; CD
    mov cr0, eax

    lgdt [V2P(early_gdtp.ptr)]
    jmp 0x08:V2P(long_mode)


[BITS 64]
long_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    lea rax, [high_start]
    jmp rax

high_start:
    
    mov rsp, early_stack
    mov rbx, [mbd_raw]
    add rbx, CONFIG_KERNEL_BASE
    mov [mbd_raw], rbx


.clear_bss:
    mov rcx, __bss_end
    sub rcx, __bss_start
    xor rax, rax
    mov rdi, __bss_start
    rep stosb

.unmap_low:
    mov rdi, early_pml4
    mov qword [rdi], 0

.reload_cr3:
    mov rax, cr3
    mov cr3, rax


.enable_avx:
    ;xor rcx, rcx
    ;xgetbv
    ;or eax, 7                               ; X87, SSE, AVX
    ;xsetbv

.enable_global_pages:
    mov rax, cr4
    or rax, (1 << 7)                        ; PGE
    mov cr4, rax

.enable_pat:
	mov ecx, 0x277
	rdmsr

	and edx, 0xFFFFFF00
	or edx, 0x00000001

	mov ecx, 0x277
	wrmsr


.init:
    call arch_early_init
    call kmain

.idle:
    pushfq
    pop rax
    and rax, 0x200
    test rax, rax
    je .idle
    hlt
    jmp .idle


section .data
align 0x1000
early_pml4:
	times 4096 db 0
early_pdp_low:
    times 4096 db 0
early_pdp_high:
    times 4096 db 0

early_gdtp:
    dq 0
    dd 0x0000FFFF, 0x00AF9A00
    dd 0x0000FFFF, 0x00AF9200
.ptr:
    dw $ - early_gdtp - 1
    dq V2P(early_gdtp)


section .stack
align 0x1000
early_stack_bottom:
	times CONFIG_STACK_SIZE db 0
early_stack:
