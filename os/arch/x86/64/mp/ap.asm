[ORG 0x1000]
[BITS 16]

ap_start:
    lgdt [ap_gdt.ptr]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:ap_pstart


[BITS 64]
ap_gdt64:
    dq 0
    
    dw 0xFFFF
    dw 0x0000
    dw 0x9800
    dw 0x00AF

    dw 0xFFFF
    dw 0x0000
    dw 0x9200
    dw 0x00AF
.ptr:
    dw $ - ap_gdt64 - 1
    dd ap_gdt64

[BITS 32]
ap_gdt:
    dq 0
    
    dw 0xFFFF
    dw 0x0000
    dw 0x9800
    dw 0x00CF

    dw 0xFFFF
    dw 0x0000
    dw 0x9200
    dw 0x00CF
.ptr:
    dw $ - ap_gdt - 1
    dd ap_gdt

ap_pstart:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax


    finit

.enable_sse:
    mov ecx, cr0
	and cx, 0xFFFB
	or cx, 2
	mov cr0, ecx
	mov ecx, cr4
	or cx, 3 << 9
	mov cr4, ecx
	xor ecx, ecx

.enable_paging:
    mov eax, [ap_cr3]
    mov cr3, eax


	mov eax, cr4
	or eax, (1 << 5)			; PAE
	and eax, ~(1 << 17)			; PCID
	mov cr4, eax

	mov ecx, 0xC0000080
	rdmsr
	or eax, 1 << 11				; NX
	wrmsr

    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)            ; LME
    wrmsr


	mov eax, cr0
	or eax, (1 << 31) 			; PG
    and eax, ~(1 << 29)         ; NW
    and eax, ~(1 << 30)         ; CD 
	mov cr0, eax
    
.enable_pat:
	mov ecx, 0x277
	rdmsr

	and edx, 0xFFFFFF00
	or edx, 0x00000001

	mov ecx, 0x277
	wrmsr

.enable_gdt:
    lgdt [ap_gdt64.ptr]
    jmp 0x08:jmp_high


[BITS 64]
jmp_high:
    mov rsp, [ap_stack]
    mov rcx, [ap_main]
    jmp rcx

.halt:
    hlt
    jmp .halt

align 16
ap_end:



times (0xF00 - ($ - $$)) db 0

ap_header:
    dd 0xCAFE1234
    dq ap_start
    dq ap_end
ap_main:
    dq 0
ap_cr3:
    dq 0
ap_stack:
    dq 0