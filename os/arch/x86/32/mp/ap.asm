[ORG 0x1000]
[BITS 16]

ap_start:
    lgdt [ap_gdt.ptr]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:ap_pstart


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

.enable_pat:
	mov ecx, 0x277
	rdmsr

	and edx, 0xFFFFFF00
	or edx, 0x00000001

	mov ecx, 0x277
	wrmsr    

.enable_global_pages:
    mov eax, cr4
    or eax, (1 << 7)            ; PGE
    mov cr4, eax

jmp_high:
    mov esp, [ap_stack]
    mov ecx, [ap_main]
    jmp ecx

.halt:
    hlt
    jmp .halt

align 16
ap_end:



times (0xF00 - ($ - $$)) db 0

ap_header:
    dd 0xCAFE1234
    dd ap_start
    dd ap_end
ap_main:
    dd 0
ap_cr3:
    dd 0
ap_stack:
    dd 0