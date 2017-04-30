[BITS 32]


extern isr_handler
extern irq_handler

extern current_pdt

%macro ISRNC 1
	global isr%1
	isr%1:
		push dword 0
		push dword %1
		jmp isr_stub
%endmacro

%macro ISREC 1
	global isr%1
	isr%1:
		push dword %1
		jmp isr_stub
%endmacro

%macro IRQ 1
	global irq%1
	irq%1:
		push dword 0
		push dword (%1 + 32)
		jmp irq_stub
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
ISRNC 0x7F
ISRNC 0x80


IRQ 0
IRQ 1
IRQ 2
IRQ 3
IRQ 4
IRQ 5
IRQ 6
IRQ 7
IRQ 8
IRQ 9
IRQ 10
IRQ 11
IRQ 12
IRQ 13
IRQ 14
IRQ 15


	
	
extern pagefault_handler

global isr14
isr14:
	cli
	push dword 14

	pusha
	push ds
	push es
	push fs
	push gs
	
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	
	push esp
	call pagefault_handler
	add esp, 4

	pop gs
	pop fs
	pop es
	pop ds
	popa
	add esp, 8
	sti
iretd
	
	
global read_eip
read_eip:
	pop eax
	jmp eax



		
		
global isr_stub	
isr_stub:
	cli	
	pusha
	push ds
	push es
	push fs
	push gs
		
	mov ax, 0x10		
	mov ds, ax			
	mov es, ax			
	mov fs, ax			
	mov gs, ax

	push esp			
	call isr_handler
	add esp, 4
	
	pop gs
	pop fs
	pop es
	pop ds
	popa
	add esp, 8
	sti
iretd
		
		
		
		
		
		
global irq_stub	
irq_stub:
	cli		
	pusha
	push ds
	push es
	push fs
	push gs

		
	mov ax, 0x10		
	mov ds, ax			
	mov es, ax			
	mov fs, ax			
	mov gs, ax			
	
	push esp						
	call irq_handler
	add esp, 4
	

	pop gs
	pop fs
	pop es
	pop ds
	popa
	add esp, 8	
	sti	
iretd

