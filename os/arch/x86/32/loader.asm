[BITS 32]


%define CONFIG_KERNEL_BASE			0xC0000000
%define CONFIG_STACK_SIZE			0x00010000
%define CPU_MAX                     64
%define V2P(x)						((x) - CONFIG_KERNEL_BASE)


extern kmain
extern arch_early_init

extern mbd_raw
extern __bss_start
extern __bss_end

global _start
global interrupt_stack_area
global core_stack_area
global core_pd


%define X86_MAP_ENTRIES		(31)

%define X86_MAP_PAGESZ		(0x400000)
%define X86_MAP_ENTRYSZ		4





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


section .text

; Low Memory
_start:
    mov esp, V2P(core_stack_area)
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
;	Low --> 0x00000000-0x08000000
; 	High -> 0xC0000000-0xC8000000


    mov ecx, X86_MAP_ENTRIES
    mov esi, 0x183				; Global, 4MiB, R/W, Present
    mov edi, V2P(core_pd)
.L0:
    mov [edi], esi
    mov [edi + (768 * 4)], esi
    add esi, X86_MAP_PAGESZ
    add edi, X86_MAP_ENTRYSZ
loop .L0


    mov edi, V2P(core_pd)

.enable_paging:
    mov eax, edi
    mov cr3, eax

    mov eax, cr4
    or eax, (1 << 4)			; PSE
    and eax, ~(1 << 5)			; PAE
    and eax, ~(1 << 17)			; PCID
    mov cr4, eax

    mov eax, cr0
    or eax, (1 << 31) 			; PG
    or eax, (1 << 0)			; PE
    and eax, ~(1 << 29)         ; NW
    and eax, ~(1 << 30)         ; CD
    mov cr0, eax


jmp_high:
    lea ecx, [high_start]
    jmp ecx


; High Memory
high_start:
    mov esp, core_stack_area
    mov ebx, [mbd_raw]
    add ebx, CONFIG_KERNEL_BASE
    mov [mbd_raw], ebx

.clear_bss:
    mov ecx, __bss_end
    sub ecx, __bss_start
    xor eax, eax
    mov edi, __bss_start
    rep stosb


.unmap_low:
    mov edi, core_pd
    mov ecx, X86_MAP_ENTRIES
    xor eax, eax
    rep stosd

.reload_cr3:
    mov eax, cr3
    mov cr3, eax

.enable_global_pages:
    mov eax, cr4
    or eax, (1 << 7)			; PGE
    mov cr4, eax

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
    pushf
    pop eax
    and eax, 0x200
    test eax, eax
    je .idle
    hlt
    jmp .idle


section .data
align 0x1000
core_pd:
    times 4096 db 0



section .bss
align 0x1000

core_stack_area_bottom:
    resb CONFIG_STACK_SIZE * CPU_MAX
core_stack_area:
